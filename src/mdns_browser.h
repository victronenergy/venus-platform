#pragma once

#include <veutil/qt/ve_qitems_dbus.hpp>
#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

class FileDownloader : public QObject
{
	Q_OBJECT

public:
	FileDownloader(const QString &hostname, const QUrl &url, QObject *parent = 0);
	const QString mName;
	const QUrl mUrl;


signals:
	void downloaded(const QString &, const QUrl &, const QByteArray &);

private slots:
	void fileDownloaded(QNetworkReply* pReply);

private:
	QNetworkAccessManager mWebCtrl;
};

class MdnsBrowser : public QObject {
	Q_OBJECT

public:
	MdnsBrowser(VeQItem *rootItem, QObject *parent);

signals:
	void serviceFound(const QString &, const AvahiAddress *addr, const uint16_t port, AvahiStringList *txt);
	void serviceRemoved(const QString &);
	void clientFailure();

private slots:
	void handleNewService(const QString &, const AvahiAddress *addr, const uint16_t port, AvahiStringList *txt);
	void onServiceRemoved(const QString &);
	void parseEmpirBusJson(const QString &, const QUrl &url, const QByteArray &data);
	void restart();

private:

	// user data object to pass to the callback functions.
	struct user_data {
		AvahiClient *client;
		MdnsBrowser *mdnsBrowser;
	};

	static void resolveCallback(
	        AvahiServiceResolver *r,
	        AVAHI_GCC_UNUSED AvahiIfIndex interface,
	        AVAHI_GCC_UNUSED AvahiProtocol protocol,
	        AvahiResolverEvent event,
	        const char *name,
	        const char *type,
	        const char *domain,
	        const char *host_name,
	        const AvahiAddress *address,
	        uint16_t port,
	        AvahiStringList *txt,
	        AvahiLookupResultFlags flags,
	        AVAHI_GCC_UNUSED void* userdata);

	static void browseCallback(
	        AvahiServiceBrowser *b,
	        AvahiIfIndex interface,
	        AvahiProtocol protocol,
	        AvahiBrowserEvent event,
	        const char *name,
	        const char *type,
	        const char *domain,
	        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
	        void* userdata);

	static void clientCallback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata);

	void startClient();
	void startBrowser(AvahiClient *);
	void setEmpirBusUrlsItem();

	AvahiClient *mClient;
	AvahiServiceBrowser *mServiceBrowser;
	QMap<QString, QJsonObject> mEmpirBusUrls;
	struct user_data mUserData;
	VeQItem *mEmpirBusSettingsUrlItem;
};
