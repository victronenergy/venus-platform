#include <velib/qt/daemontools_service.hpp>
#include <velib/qt/v_busitems.h>
#include <velib/qt/ve_qitem_dbus_publisher.hpp>

#include "application.hpp"

static bool serviceExists(QString const &svc) {
	return QDir("/service/" + svc).exists();
}

static bool templateExists(QString const &name) {
	return QFile("/opt/victronenergy/service-templates/conf/" + name + ".conf").exists();
}

class SettingsInfo : public VeQItemSettingsInfo
{
public:
	SettingsInfo()
	{
		add("Relay/Function", 0, 0, 0);
		add("Relay/Polarity", 0, 0, 0);
		add("Relay/1/Function", 2, 0, 0);
		add("Relay/1/Polarity", 0, 0, 0);
		if (templateExists("hostapd"))
			add("Services/AccessPoint", 1, 0, 1);
		add("Services/BleSensors", 0, 0, 1);
		add("Services/Bluetooth", 1, 0, 1);
		add("Services/Modbus", 0, 0, 1);
		add("Services/MqttLocal", 0, 0, 1);
		add("Services/MqttLocalInsecure", 0, 0, 1);
		add("Services/MqttVrm", 0, 0, 1);
		add("Services/NodeRed", 0, 0, 2);
		add("Services/SignalK", 0, 0, 1);
		// Note: only for debugging over tcp/ip, _not_ socketcan itself...
		add("Services/Socketcand", 0, 0, 1);
		add("System/ImageType", 0, 0, 1);
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
		spawn("firewall", QStringList() << "allow" << "tcp" << "8883");
	} else {
		qDebug() << "[Firewall] Disallow local MQTT over SSL, port 8883";
		spawn("firewall", QStringList() << "deny" << "tcp" << "8883");
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
		spawn("firewall", QStringList() << "allow" << "tcp" << "1883");
		spawn("firewall", QStringList() << "allow" << "tcp" << "9001");
		mqttCheckLocalInsecure();
	} else {
		qDebug() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
		spawn("firewall", QStringList() << "deny" << "tcp" << "1883");
		spawn("firewall", QStringList() << "deny" << "tcp" << "9001");
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

	if (templateExists("hostapd")) {
		VeQItemProxy::addProxy(mService->itemGetOrCreate("Services/AccessPoint"), "Enabled",
							   mSettings->root()->itemGetOrCreate("Settings/Services/AccessPoint"));
		new DaemonToolsService(mSettings, "/service/hostapd", "Settings/Services/AccessPoint",
							   this, QStringList() << "-s" << "hostapd");
	}

	/*
	 * On behalf of venus-access, since that one should be kept as simple as possible.
	 * This only handles the optional settings: venus-platform -> localsettings -> venus-access.
	 */
	if (QFile("/dev/ttyconsole").exists()) {
		VeQItemProxy::addProxy(mService->itemGetOrCreate("Services/Console"), "Enabled",
							   mSettings->root()->itemGetOrCreate("Settings/Services/Console"));
	}

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

	// The items exported to the dbus..
	VeQItemProducer *toDbus = new VeQItemProducer(VeQItems::getRoot(), "to-dbus", this);
	mService = toDbus->services()->itemGetOrCreate("com.victronenergy.platform", false);

	manageDaemontoolsServices();

	mCanInterfaceMonitor = new CanInterfaceMonitor(mSettings, this);
	connect(mCanInterfaceMonitor, SIGNAL(interfacesChanged()), SLOT(onCanInterfacesChanged()));
	mCanInterfaceMonitor->enumerate();

	// With everything ready, do export the service to the dbus
	VeQItemDbusPublisher *publisher = new VeQItemDbusPublisher(toDbus->services(), this);
	mService->produceValue(QString());
	publisher->open(VBusItems::getDBusAddress());
}

void Application::spawn(QString const &cmd, const QStringList &args)
{
	QProcess *proc = new QProcess();
	connect(proc, SIGNAL(finished(int)), proc, SLOT(deleteLater()));
	proc->start(cmd, args);
}
