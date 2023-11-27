#include <signal.h>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_dbus_connection.hpp>
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_exported_dbus_services.hpp>

#include "application.hpp"
#include "mqtt.hpp"
#include "time.hpp"

static QDir machineRuntimeDir = QDir("/etc/venus");
static QDir venusDir = QDir("/opt/victronenergy");

static bool serviceExists(QString const &svc) {
	return QDir("/service/" + svc).exists();
}

static bool templateExists(QString const &name) {
	return QFile("/opt/victronenergy/service-templates/conf/" + name + ".conf").exists();
}

QStringList getFeatureList(QString const &name, bool lines)
{
	QStringList ret;
	QFile file(machineRuntimeDir.filePath(name));

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return ret;

	QString line;
	while (!file.atEnd()) {
		line = file.readLine();
		if (lines) {
			line = line.trimmed();
			if (!line.isEmpty())
				ret.append(line);
		} else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
			ret.append(line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts));
#else
			ret.append(line.split(QRegExp("\\s+"), QString::SkipEmptyParts));
#endif
		}
	}

	return ret;
}

QString getFeature(QString const &name, bool optional)
{
	QStringList list = getFeatureList(name);

	if (!optional && list.count() != 1) {
		qCritical() << "required machine feature " + name + " does not exist";
		exit(EXIT_FAILURE);
	}

	return (list.count() >= 1 ? list[0] : QString());
}


static bool dataPartionError()
{
	QFile file("/run/data-partition-state");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.atEnd())
		return false;

	QTextStream in(&file);
	QString line = in.readLine();

	return line == "failed" || line == "failed-to-mount";
}

int VeQItemReboot::setValue(const QVariant &value)
{
	qDebug() << "Rebooting";

	/*
	 * Note: this used to spawn a child process to call reboot. Since it was found,
	 * that that can fail, just signal init directly. It should have been fixed in
	 * a later linux release...
	 *
	 * @note this only works if init accepts SIGINT.
	 */
	kill(1, SIGINT);

	return VeQItemAction::setValue(value);
}

enum Mk3Update {
	DISALLOWED,
	ALLOWED,
	NOT_APPLICABLE
};

class SettingsInfo : public VeQItemSettingsInfo
{
public:
	SettingsInfo(enum Mk3Update mk3update)
	{
		add("Gui/DemoMode", 0, 0, 3);
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
		add("Vebus/AllowMk3Fw212Update", mk3update, 0, 2);
	}
};

Application::Application::Application(int &argc, char **argv) :
	QCoreApplication(argc, argv),
	mLocalSettingsTimeout()
{
	QDBusConnection dbus = VeDbusConnection::getConnection();
	if (!dbus.isConnected()) {
		qCritical() << "DBus connection failed";
		::exit(EXIT_FAILURE);
	}

	VeQItemDbusProducer *producer = new VeQItemDbusProducer(VeQItems::getRoot(), "dbus", false, false);
	producer->setAutoCreateItems(false);
	producer->open(dbus);
	mServices = producer->services();
	mSettings = new VeQItemDbusSettings(producer->services(), QString("com.victronenergy.settings"));

	VeQItem *item = mServices->itemGetOrCreate("com.victronenergy.settings");
	connect(item, SIGNAL(stateChanged(VeQItem::State)), SLOT(onLocalSettingsStateChanged(VeQItem::State)));
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

void Application::onLocalSettingsStateChanged(VeQItem::State state)
{
	mLocalSettingsTimeout.stop();

	if (state != VeQItem::Synchronized) {
		qCritical() << "Localsettings not available" << state;
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

void Application::mqttLocalChanged(QVariant var)
{
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

void Application::mqttLocalInsecureChanged(QVariant var)
{
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

void Application::mk3UpdateAllowedChanged(QVariant var)
{
	static QVariant lastValue;
	bool ok;

	if (lastValue.toInt(&ok) == 0 && ok && var.toInt(&ok) == 1 && ok) {
		qDebug() << "restarting all Vebus services";
		spawn("sh", QStringList() << "-c" << "svc -t /service/mk2-dbus.*");
	}

	lastValue = var;
}

void Application::demoSettingChanged(QVariant var)
{
	static bool isStarted = false;

	if (var.isValid()) {
		if (var.toInt() > 0) {
			isStarted = true;
			spawn(venusDir.filePath("dbus-recorder/startdemo.sh"),
				QStringList() << QString::number(var.toInt()));
		} else if (isStarted) {
			/*
			 * NOTE: isStarted prevents calling stopdemo during a normal boot
			 * without the demo mode enabled, since the stop script restarts
			 * normal services and that can interfere with the normal startup.
			 */
			isStarted = false;
			spawn(venusDir.filePath("dbus-recorder/stopdemo.sh"));
		}
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
		VeQItemProxy::addProxy(mService->itemGetOrCreate("Services/NodeRed"), "Mode",
							   mSettings->root()->itemGetOrCreate("Settings/Services/NodeRed"));
	}

	if (serviceExists("signalk-server")) {
		new DaemonToolsService(mSettings, "/service/signalk-server", "Settings/Services/SignalK", this);
		VeQItemProxy::addProxy(mService->itemGetOrCreate("Services/SignalK"), "Enabled",
							   mSettings->root()->itemGetOrCreate("Settings/Services/SignalK"));
	}

	new DaemonToolsService(mSettings, "/service/vesmart-server", "Settings/Services/Bluetooth",
						   this, QStringList() << "-s" << "vesmart-server");

	QList<QString> mqttList = QList<QString>() << "Settings/Services/MqttLocal" << "Settings/Services/MqttVrm";
	new DaemonToolsService(mSettings, "/service/mosquitto", mqttList, this, true, QStringList() << "-s" << "mosquitto");
	new DaemonToolsService(mSettings, "/service/dbus-mqtt", mqttList, this, false, QStringList() << "-s" << "dbus-mqtt");
	new DaemonToolsService(mSettings, "/service/mqtt-rpc", mqttList, this, false, QStringList() << "-s" << "mqtt-rpc");

	// MQTT on LAN insecure
	mMqttLocalInsecure = mSettings->root()->itemGetOrCreate("Settings/Services/MqttLocalInsecure");
	mMqttLocalInsecure->getValueAndChanges(this, SLOT(mqttLocalInsecureChanged(QVariant)));

	// MQTT on LAN
	mMqttLocal = mSettings->root()->itemGetOrCreate("Settings/Services/MqttLocal");
	mMqttLocal->getValueAndChanges(this, SLOT(mqttLocalChanged(QVariant)));

	VeQItem *item = mSettings->root()->itemGetOrCreate("Settings/Vebus/AllowMk3Fw212Update");
	item->getValueAndChanges(this, SLOT(mk3UpdateAllowedChanged(QVariant)));

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
	QString installerVersion;
	QFile file("/data/venus/installer-version");
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		file.readLine(); // version number
		file.readLine(); // Victron
		installerVersion = file.readLine();
	}

	// mk3 firmware 212 should not automatically be updated after a Venus update.
	// When shipped with a newer version it should be fine. Since the mk3 version is unknown
	// without the service being started (which will update it), check the installer date instead.
	enum Mk3Update mk3update = !installerVersion.isEmpty() && installerVersion >= "202210050000" ? NOT_APPLICABLE : DISALLOWED;

	qDebug() << "Creating settings";
	if (!mSettings->addSettings(SettingsInfo(mk3update))) {
		qCritical() << "Creating settings failed";
		::exit(EXIT_FAILURE);
	}

	// The items exported to the dbus..
	VeQItemProducer *toDbus = new VeQItemProducer(VeQItems::getRoot(), "to-dbus", this);
	mService = toDbus->services()->itemGetOrCreate("com.victronenergy.platform", false);

	manageDaemontoolsServices();

	mCanInterfaceMonitor = new CanInterfaceMonitor(mSettings, mService, this);
	connect(mCanInterfaceMonitor, SIGNAL(interfacesChanged()), SLOT(onCanInterfacesChanged()));
	mCanInterfaceMonitor->enumerate();

	mUpdater = new Updater(mService, this);

	mService->itemGetOrCreate("Device")->itemAddChild("Reboot", new VeQItemReboot());
	mService->itemGetOrCreate("Device")->itemAddChild("Time", new VeQItemTime());

	QProcess *proc = Application::spawn("get-unique-id");
	proc->waitForFinished();
	mService->itemGetOrCreateAndProduce("Device/UniqueId", QString(proc->readAllStandardOutput().trimmed()));

	int error = dataPartionError() ? 1 : 0;
	mService->itemGetOrCreateAndProduce("Device/DataPartitionError", error);

	mService->itemGetOrCreate("Mqtt")->itemAddChild("RegisterOnVrm", new VeQItemMqttBridgeRegistrar());

	// Demo mode
	VeQItem *demoModeSetting = mSettings->root()->itemGetOrCreate("Settings/Gui/DemoMode");
	demoModeSetting->getValueAndChanges(this, SLOT(demoSettingChanged(QVariant)));

	// With everything ready, do export the service to the dbus
	VeQItemExportedDbusServices *publisher = new VeQItemExportedDbusServices(toDbus->services(), this);
	mService->produceValue(QString());
	publisher->open(VeDbusConnection::getDBusAddress());
}

QProcess *Application::spawn(QString const &cmd, const QStringList &args)
{
	QProcess *proc = new QProcess();
	connect(proc, SIGNAL(finished(int)), proc, SLOT(deleteLater()));
	proc->start(cmd, args);
	return proc;
}
