#include <velib/qt/daemontools_service.hpp>
#include <velib/qt/v_busitems.h>

#include "application.hpp"

class SettingsInfo : public VeQItemSettingsInfo
{
public:
	SettingsInfo()
	{
		add("Services/BleSensors", 0, 0, 1);
		add("Services/Console", 0, 0, 0);
		add("System/RemoteSupport", 0, 0, 1);
		add("System/SSHLocal", 0, 0, 1);
	}
};

Application::Application::Application(int &argc, char **argv) :
	QCoreApplication(argc, argv),
	mLocalSettingsTimeout()
{
	VBusItems::setConnectionType(QDBusConnection::SystemBus);
	QDBusConnection dbus = VBusItems::getConnection();
	if (!dbus.isConnected()) {
		qCritical() << "DBus connection failed";
		::exit(EXIT_FAILURE);
	}

	VeQItemDbusProducer *producer = new VeQItemDbusProducer(VeQItems::getRoot(), "dbus", false, false);
	producer->setAutoCreateItems(false);
	producer->open(VBusItems::getConnection());
	mServices = producer->services();
	mSettings = new VeQItemDbusSettings(producer->services(), QString("com.victronenergy.settings"));

	VeQItem *item = mServices->itemGetOrCreate("com.victronenergy.settings");
	connect(item, SIGNAL(stateChanged(VeQItem*,State)), SLOT(onLocalSettingsStateChanged(VeQItem*)));
	if (item->getState() == VeQItem::Synchronized) {
		qDebug() << "Localsettings found";
		init();
	} else {
		qDebug() << "Localsettings not found";
		mLocalSettingsTimeout.setSingleShot(true);
		mLocalSettingsTimeout.setInterval(120000);
		connect(&mLocalSettingsTimeout, SIGNAL(timeout()), SLOT(onLocalSettingsTimeout()));
		mLocalSettingsTimeout.start();
	}
}

void Application::onLocalSettingsStateChanged(VeQItem *item)
{
	mLocalSettingsTimeout.stop();

	if (item->getState() != VeQItem::Synchronized) {
		qCritical() << "Localsettings not available" << item->getState();
		::exit(EXIT_FAILURE);
	}

	qDebug() << "Localsettings appeared";
	init();
}

void Application::onLocalSettingsTimeout()
{
	qCritical() << "Localsettings not available!";
	::exit(EXIT_FAILURE);
}

void Application::manageDaemontoolsServices()
{
	new DeamonToolsConsole(mSettings, "/service/dbus-ble-sensors", "Settings/Services/BleSensors", this);
	new DeamonToolsConsole(mSettings, "/service/vegetty", "Settings/Services/Console", this);

	QList<QString> sshdlist = QList<QString>() << "Settings/System/RemoteSupport" << "Settings/System/SSHLocal" << "Settings/System/VncInternet";
	// false: no restart -> symlinks / firewall rules will be changed instead.
	new DaemonToolsService(mSettings, "/service/openssh", sshdlist, this, false);

	QList<QString> list = QList<QString>() << "Settings/System/RemoteSupport" << "Settings/System/VncInternet";
	new DaemonToolsService(mSettings, "/service/ssh-tunnel", list, this, false);
}

void Application::remoteSupportChanged(VeQItem *item, QVariant var)
{
	Q_UNUSED(item);

	if (!var.isValid())
		return;

	if (var.toBool())
		QFile::link("/usr/share/support-keys/authorized_keys",
				"/run/ssh_support_keys");
	else
		QFile::remove("/run/ssh_support_keys");
}

void Application::sshLocalChanged(VeQItem *item, QVariant var)
{
	Q_UNUSED(item);

	if (!var.isValid())
		return;

	if (var.toBool())
		system("firewall allow tcp ssh");
	else
		system("firewall deny tcp ssh");
}

void Application::init()
{
	qDebug() << "Creating settings";
	if (!mSettings->addSettings(SettingsInfo())) {
		qCritical() << "Creating settings failed";
		::exit(EXIT_FAILURE);
	}

	// Remote support
	VeQItem *remoteSupport = mSettings->root()->itemGetOrCreate("Settings/System/RemoteSupport");
	remoteSupport->getValueAndChanges(this, SLOT(remoteSupportChanged(VeQItem *, QVariant)));

	// SSH on LAN
	VeQItem *sshLocal = mSettings->root()->itemGetOrCreate("Settings/System/SSHLocal");
	sshLocal->getValueAndChanges(this, SLOT(sshLocalChanged(VeQItem *, QVariant)));

	manageDaemontoolsServices();
}
