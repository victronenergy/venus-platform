#include <velib/qt/daemontools_service.hpp>
#include <velib/qt/v_busitems.h>
#include <velib/qt/ve_qitem_dbus_publisher.hpp>

#include "application.hpp"

// FIXME: Since September 2019 Service/Mqtt no longer exists, see:
//   https://github.com/victronenergy/localsettings/commit/f8a7cb23943a8cc68df6e92e4fdbbf0f44dc8ce3#diff-27332c9bf657f3f9b7b8f1ea9e16aea18c2d62c2761b1419da3f2ce1d71de4a9R110
//   https://github.com/victronenergy/gui/commit/241e4e9c643dfd8e56b9bf7108d5456669b35878#diff-af69aaf026a1bde8ff3b133d8937dc397ea94aad22f1e92128bba3c61b41da5eL54
// Hence MqttN2k is broken for 3 years as well...

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
		add("Services/Bluetooth", 1, 0, 1);
		add("Services/Modbus", 0, 0, 1);
		add("Services/MqttLocal", 0, 0, 1);
		add("Services/MqttLocalInsecure", 0, 0, 1);
		add("Services/MqttN2k", 0, 0, 1);
		add("Services/MqttVrm", 0, 0, 1);
		add("Services/SignalK", 0, 0, 1);
		add("Services/NodeRed", 0, 0, 2);
		// Note: only for debugging over tcp/ip, _not_ socketcan itself...
		add("Services/Socketcand", 0, 0, 1);
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

void Application::mqttCheckLocalInsecure()
{
	if (!mMqttLocal || !mMqttLocalInsecure)
		return;

	QVariant local = mMqttLocal->getLocalValue();
	QVariant insecure = mMqttLocalInsecure->getLocalValue();

	if (!local.isValid() || !insecure.isValid())
		return;

	// If local MQTT is disable, disable insecure access as well.
	if (!local.toBool() && insecure.toBool())
		mMqttLocalInsecure->setValue(false);
}

void Application::mqttLocalChanged(VeQItem *item, QVariant var)
{
	Q_UNUSED(item);

	if (!var.isValid())
		return;

	if (mMqttLocalWasValid) {
		DaemonToolsService upnp("/service/simple-upnpd");
		upnp.restart();
	}
	mMqttLocalWasValid = true;

	if (var.toBool()) {
		qDebug() << "[Firewall] Allow local MQTT over SSL, port 8883";
		system("firewall allow tcp 8883");
	} else {
		qDebug() << "[Firewall] Disallow local MQTT over SSL, port 8883";
		system("firewall deny tcp 8883");
		mqttCheckLocalInsecure();
	}
}

void Application::mqttLocalInsecureChanged(VeQItem *item, QVariant var)
{
	Q_UNUSED(item);

	if (!var.isValid())
		return;

	if (var.toBool()) {
		qDebug() << "[Firewall] Allow insecure access to MQTT, port 1883 / 9001";
		system("firewall allow tcp 1883");
		system("firewall allow tcp 9001");
		mqttCheckLocalInsecure();
	} else {
		qDebug() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
		system("firewall deny tcp 1883");
		system("firewall deny tcp 9001");
	}
}

void Application::manageDaemontoolsServices()
{
	new DaemonToolsService(mSettings, "/service/dbus-ble-sensors", "Settings/Services/BleSensors", this);

	new DaemonToolsService(mSettings, "/service/dbus-pump", "Settings/Relay/Function", 3, this);

	new DaemonToolsService(mSettings, "/service/dbus-modbustcp", "Settings/Services/Modbus",
						   this, QStringList() << "-s" << "dbus-modbustcp");

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

	new DaemonToolsService(mSettings, "/service/vesmart-server", "Settings/Services/Bluetooth",
						   this, QStringList() << "-s" << "vesmart-server");

	QList<QString> mqttList = QList<QString>() << "Settings/Services/MqttLocal" << "Settings/Services/MqttVrm";
	new DaemonToolsService(mSettings, "/service/mosquitto", mqttList, this, true, QStringList() << "-s" << "mosquitto");
	new DaemonToolsService(mSettings, "/service/dbus-mqtt", mqttList, this, false, QStringList() << "-s" << "dbus-mqtt");
	new DaemonToolsService(mSettings, "/service/mqtt-rpc", mqttList, this, false, QStringList() << "-s" << "mqtt-rpc");

	// MQTT on LAN insecure
	mMqttLocalInsecure = mSettings->root()->itemGetOrCreate("Settings/Services/MqttLocalInsecure");
	mMqttLocalInsecure->getValueAndChanges(this, SLOT(mqttLocalInsecureChanged(VeQItem*,QVariant)));

	// MQTT on LAN
	mMqttLocal = mSettings->root()->itemGetOrCreate("Settings/Services/MqttLocal");
	mMqttLocal->getValueAndChanges(this, SLOT(mqttLocalChanged(VeQItem*,QVariant)));

	// CAN-bus debugging over tcp/ip
	new DaemonToolsService(mSettings, "/service/socketcand", "Settings/Services/Socketcand",
						   this, QStringList() << "-s" << "socketcand");
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
