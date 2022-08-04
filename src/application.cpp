#include <velib/qt/daemontools_service.hpp>
#include <velib/qt/v_busitems.h>
#include <velib/qt/ve_qitem_dbus_publisher.hpp>

#include "application.hpp"

class SettingsInfo : public VeQItemSettingsInfo
{
public:
	SettingsInfo()
	{
		add("Relay/Function", 0, 0, 0);
		add("Relay/Polarity", 0, 0, 0);
		add("Relay/1/Function", 2, 0, 0);
		add("Relay/1/Polarity", 0, 0, 0);
		add("Services/BleSensors", 0, 0, 1);
		add("Services/SignalK", 0, 0, 1);
		add("Services/NodeRed", 0, 0, 2);
		add("System/ImageType", 0, 0, 1);
	}
};

static bool serviceExists(QString const &svc) {
	return QDir("/service/" + svc).exists();
}

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

void Application::onCanInterfacesChanged()
{
	mService->itemGetOrCreateAndProduce("CanBus/Interfaces", QVariant::fromValue(mCanInterfaceMonitor->canInfo()));
}

void Application::manageDaemontoolsServices()
{
	new DaemonToolsService(mSettings, "/service/dbus-ble-sensors", "Settings/Services/BleSensors", this);

	new DaemonToolsService(mSettings, "/service/dbus-pump", "Settings/Relay/Function", 3, this);

	// Temperature relay
	QList<QString> tempSensorRelayList = QList<QString>() << "Settings/Relay/Function" << "Settings/Relay/1/Function";
	new DaemonToolsService(mSettings, "/service/dbus-tempsensor-relay", tempSensorRelayList, 4, this, false);

	// Large image services
	if (serviceExists("node-red-venus")) {
		QList<int> start = QList<int>() << 1 << 2;
		new DaemonToolsService(mSettings, "/service/node-red-venus", "Settings/Services/NodeRed", start, this);
	}

	if (serviceExists("signalk-server"))
		new DaemonToolsService(mSettings, "/service/signalk-server", "Settings/Services/SignalK", this);
}

void Application::init()
{
	qDebug() << "Creating settings";
	if (!mSettings->addSettings(SettingsInfo())) {
		qCritical() << "Creating settings failed";
		::exit(EXIT_FAILURE);
	}

	manageDaemontoolsServices();

	// The part exporting items from the dbus..
	VeQItemProducer *toDbus = new VeQItemProducer(VeQItems::getRoot(), "to-dbus", this);
	mService = toDbus->services()->itemGetOrCreate("com.victronenergy.platform", false);

	VeQItemDbusPublisher *publisher = new VeQItemDbusPublisher(toDbus->services(), this);
	publisher->open(VBusItems::getDBusAddress());
	mService->produceValue(QString());

	mCanInterfaceMonitor = new CanInterfaceMonitor(mSettings, this);
	connect(mCanInterfaceMonitor, SIGNAL(interfacesChanged()), SLOT(onCanInterfacesChanged()));
	mCanInterfaceMonitor->enumerate();
}
