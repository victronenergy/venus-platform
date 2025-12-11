#include <QString>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSet>

#include "avahi-qt/qt-watch.h"
#include "mdns_browser.h"

const QSet<QString> EMPIRBUS_IDS = {"49c956d6-ed5e-4451-beb9-0f55e3ef22cb", "de4a5f92-56af-4d27-bab8-15c64728fa21"};
FileDownloader::FileDownloader(const QUrl &url, QObject *parent) :
 QObject(parent), mUrl(url)
{
	connect(&mWebCtrl, SIGNAL(finished(QNetworkReply *)), this, SLOT(fileDownloaded(QNetworkReply *)));
	QNetworkRequest request(url);
	mWebCtrl.get(request);
}

void FileDownloader::fileDownloaded(QNetworkReply *pReply) {
	pReply->deleteLater();
	emit downloaded(mUrl, pReply->readAll());
}


MdnsBrowser::MdnsBrowser(VeQItem *rootItem, QObject *parent) :
    QObject(parent)
{
	mEmpirBusSettingsUrlItem = rootItem->itemGetOrCreateAndProduce("EmpirBus/SettingsUrl", "");
	mUserData.mdnsBrowser = this;
	mServiceBrowser = nullptr;
	mClient = nullptr;
	connect(this, SIGNAL(clientFailure()), this, SLOT(restart()));
	connect(this, SIGNAL(serviceFound(const AvahiAddress *, const uint16_t, AvahiStringList *)), this, SLOT(handleNewService(const AvahiAddress *, const uint16_t, AvahiStringList *)));
	connect(this, SIGNAL(serviceRemoved()), this, SLOT(onServiceRemoved()));
	startClient();
}

void MdnsBrowser::restart()
{
	if(mServiceBrowser) {
		avahi_service_browser_free(mServiceBrowser);
		mServiceBrowser = nullptr;
	}

	if(mClient) {
		avahi_client_free(mClient);
		mClient = nullptr;
	}

	startClient();
}

void MdnsBrowser::startClient()
{
	const AvahiPoll *qtPoll;
	int error;

	if (!(qtPoll = avahi_qt_poll_get())) {
		qCritical() << "[mDNS browser] Failed to create avahi qt poll object.";
		return;
	}

	/* Allocate a new client with AVAHI_CLIENT_NO_FAIL so the client will be created even if the avahi daemon isn't available.
	 * The client callback will be called when the state changes so the servicebrowser is created from there when the daemon becomes available.
	 * The client callback will also initiate a restart of the client on failure.
	 * The avahi daemon will be restarted on certain network changes (e.g. connecting to a network restarts the daemon).
	 */
	mClient = avahi_client_new(qtPoll, AVAHI_CLIENT_NO_FAIL, MdnsBrowser::clientCallback, &mUserData, &error);

	if (!mClient) {
		std::chrono::milliseconds retry(60000);
		qCritical() << "[mDNS browser] Failed to create client: " << avahi_strerror(error) << ", retrying after " << retry.count() << " milliseconds";
		QTimer::singleShot(retry, this, &MdnsBrowser::restart);
		return;
	}

	mUserData.client = mClient;
}

void MdnsBrowser::startBrowser(AvahiClient *c)
{
	if (mServiceBrowser)
		return;
	// Check for "_garmin-empirbus._tcp" services specifically
	// Checking for advertisements on IPv4 only is sufficient
	if (!(mServiceBrowser = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, "_garmin-empirbus._tcp", NULL, (AvahiLookupFlags) 0, browseCallback, &mUserData))) {
		std::chrono::milliseconds retry(60000);
		qCritical() << "[mDNS browser] Failed to create service browser: " << avahi_strerror(avahi_client_errno(mClient)) << ", retrying after " << retry.count() << " milliseconds";
		QTimer::singleShot(retry, this, &MdnsBrowser::restart);
	}
	qDebug() << "[mDNS browser] service browser started";
}

void MdnsBrowser::clientCallback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void *userdata) {
	if (!c || !userdata)
		return;

	MdnsBrowser *b = ((user_data *)userdata)->mdnsBrowser;

	/* Called whenever the client or server state changes */
	switch (state) {
	    case AVAHI_CLIENT_FAILURE:
		qWarning() << "[mDNS browser] Server connection failure: " << avahi_strerror(avahi_client_errno(c)) << ", retrying";
		emit b->clientFailure();
		    break;

	    case AVAHI_CLIENT_S_REGISTERING:
	    case AVAHI_CLIENT_S_RUNNING:
	    case AVAHI_CLIENT_S_COLLISION:
		    b->startBrowser(c);
		    break;

		// Do nothing while the client connects to the daemon
	    case AVAHI_CLIENT_CONNECTING:
		    break;
	}
}

void MdnsBrowser::browseCallback(
        AvahiServiceBrowser *b,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        const char *type,
        const char *domain,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        void *userdata)
{
	if (!b || !userdata)
		return;

	AvahiClient *c = ((user_data *)userdata)->client;

	/* Called whenever new services becomes available on the LAN or is removed from the LAN */

	switch (event) {
	    case AVAHI_BROWSER_FAILURE:

		    qCritical() << "[mDNS browser] " << avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b)));
			// TODO Cleanup
		    return;

	    case AVAHI_BROWSER_NEW:
		    qDebug() << "[mDNS browser] " << "New service: " << name;

			/* We ignore the returned resolver object. In the callback
			   function we free it. If the server is terminated before
			   the callback function is called the server will free
			   the resolver for us. */

			if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags) 0, MdnsBrowser::resolveCallback, userdata)))
				qCritical() << "[mDNS browser] Failed to resolve service " << name << " : " << avahi_strerror(avahi_client_errno(c));

		    break;

	    case AVAHI_BROWSER_REMOVE: {
		    qDebug() << "[mDNS browser] " << "Removed service: " << name;
			MdnsBrowser *i = ((user_data *)userdata)->mdnsBrowser;
			emit i->serviceRemoved();
		    break;
	    }

	    case AVAHI_BROWSER_ALL_FOR_NOW:
	    case AVAHI_BROWSER_CACHE_EXHAUSTED:
		    break;
	}
}

void MdnsBrowser::resolveCallback(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    AVAHI_GCC_UNUSED const char *type,
    AVAHI_GCC_UNUSED const char *domain,
    AVAHI_GCC_UNUSED const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata) {

	if (!r || !userdata)
		return;

	/* Called whenever a service has been resolved successfully or timed out */

	switch (event) {
	    case AVAHI_RESOLVER_FAILURE:
		    qCritical() << "[mDNS browser] Failed to resolve service " << name << " : " << avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r)));
		    break;

	    case AVAHI_RESOLVER_FOUND: {
		    MdnsBrowser *i = ((user_data *)userdata)->mdnsBrowser;
			emit i->serviceFound(address, port, txt);
	    }
	}

	avahi_service_resolver_free(r);
}

void MdnsBrowser::onServiceRemoved()
{
	/* Since we only support one empirbus device in the network,
	 * it is fine to just clear the path when the service disappears.
	 */
	mEmpirBusSettingsUrlItem->setValue("");
}

void MdnsBrowser::handleNewService(const AvahiAddress *address, const uint16_t port, AvahiStringList *txt)
{
	/* Empirbus service discovery
	 * Check the mDNS txt record for "protovers" == 1 and the presence of "path".
	 * Download the json file pointed to by "path".
	 * That JSON file should contain an "id" and a "path" (and will contain other things we are not interested in).
	 * Check whether the ID matches one of the predefined empirbus ID's which indicate <path> is a settings page.
	 * If so, set the /EmpirBus/SettingsUrl dbus path equal to "<ip>:<port>/<path>".
	 * Next, VRM will use it to create a proxy to the settings page.
	*/
	AvahiStringList *proto_vers_item = avahi_string_list_find(txt, "protovers");
	AvahiStringList *path_item = avahi_string_list_find(txt, "path");
	if (proto_vers_item && avahi_string_list_get_size(proto_vers_item) > 1 && path_item) {
		char *value = nullptr;
		char *pathValue = nullptr;

		avahi_string_list_get_pair(proto_vers_item, nullptr, &value, nullptr);
		bool ok = false;
		int protoVers = QByteArray(value).toInt(&ok);
		avahi_string_list_get_pair(path_item, nullptr, &pathValue, nullptr);
		QString path = QString::fromUtf8(pathValue);
		if (ok && protoVers == 1 && path.endsWith(".json", Qt::CaseInsensitive)) {

			// Download json file
			char a[AVAHI_ADDRESS_STR_MAX];
			avahi_address_snprint(a, sizeof(a), address);

			QUrl url;
			url.setScheme("http");
			url.setHost(QString::fromUtf8(a));
			url.setPort(port);
			url.setPath(path);

			auto *fileDownloader = new FileDownloader(url, this);

			connect(fileDownloader, SIGNAL(downloaded(const QUrl &, const QByteArray &)), this, SLOT(parseEmpirBusJson(const QUrl &, const QByteArray &)));
			connect(fileDownloader, SIGNAL(downloaded(const QUrl &, const QByteArray &)), fileDownloader, SLOT(deleteLater()));
		}
	}
}

void MdnsBrowser::parseEmpirBusJson(const QUrl &_url, const QByteArray &data)
{
	QJsonDocument doc(QJsonDocument::fromJson(data));
	if (!doc.isEmpty() && doc.isObject()) {
		QJsonValue id = doc["id"];
		if (id != QJsonValue::Undefined && id.isString()) {

			// Check if id is equal to either one of the Empirbus IDs.
			QString idStr = id.toString();
			if (EMPIRBUS_IDS.contains(idStr)) {
				QJsonValue path = doc["path"];
				if (path != QJsonValue::Undefined && path.isString()) {
					QString pathStr = path.toString();
					QUrl url(_url);
					/* pathStr is something like /settings-web/#
					 * QUrl::setPath does not accept this pathStr because # is an empty fragment, and not a path.
					 * So instead of replacing the path, clear it and just suffix the url with whatever is in pathStr
					*/
					url.setPath("");
					QString urlStr = url.toString() + '/' + pathStr;
					qDebug() << "[mDNS browser] Setting empirbus settings page path to: " << urlStr;
					mEmpirBusSettingsUrlItem->setValue(urlStr);
					return;
				}
			}
		}
	}
}
