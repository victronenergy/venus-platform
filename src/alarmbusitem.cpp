#include <QDebug>
#include <QEvent>

#include "alarmbusitem.h"

/* ok, warning, alarm types */
AlarmMonitor *DeviceAlarms::addTripplet(const QString &busitemPathAlarm,
										VeQItem *busitemSetting, const QString &busitemPathValue)
{
	AlarmMonitor *ret = new AlarmMonitor(mService, AlarmMonitor::REGULAR, busitemPathAlarm,
										 busitemSetting, busitemPathValue, this);
	mAlarms.append(ret);
	return ret;
}

void DeviceAlarms::addVebusError(const QString &busitemPathAlarm)
{
	mAlarms.append(new AlarmMonitor(mService, AlarmMonitor::VEBUS_ERROR, busitemPathAlarm, nullptr, "", this));
}

void DeviceAlarms::addBmsError(const QString &busitemPathAlarm)
{
	mAlarms.append(new AlarmMonitor(mService, AlarmMonitor::BMS_ERROR, busitemPathAlarm, nullptr, "", this));
}

void DeviceAlarms::addChargerError(const QString &busitemPathAlarm)
{
	mAlarms.append(new AlarmMonitor(mService, AlarmMonitor::CHARGER_ERROR, busitemPathAlarm, nullptr, "", this));
}

void DeviceAlarms::addWakespeedError(const QString &busitemPathAlarm)
{
	mAlarms.append(new AlarmMonitor(mService, AlarmMonitor::WAKESPEED_ERROR, busitemPathAlarm, nullptr, "", this));
}

DeviceAlarms *DeviceAlarms::createBatteryAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	return new BatteryAlarms(service, noticationCenter);
}

DeviceAlarms *DeviceAlarms::createGeneratorStartStopAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/NoGeneratorAtAcIn", nullptr, "");
	alarms->addTripplet("/Alarms/ServiceIntervalExceeded", nullptr);
	alarms->addTripplet("/Alarms/AutoStartDisabled", nullptr);

	return alarms;
}

DeviceAlarms *DeviceAlarms::createDigitalInputAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarm", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createSolarChargerAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/LowVoltage",	nullptr,	"");
	alarms->addTripplet("/Alarms/HighVoltage",	nullptr,	"");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createAcChargerAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/LowVoltage",	nullptr,	"");
	alarms->addTripplet("/Alarms/HighVoltage",	nullptr,	"");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createInverterAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/LowVoltage",		service->item("/Settings/Alarms/LowVoltage"),		"");
	alarms->addTripplet("/Alarms/HighVoltage",		service->item("/Settings/Alarms/HighVoltage"),		"");
	alarms->addTripplet("/Alarms/LowVoltageAcOut",	service->item("/Settings/Alarms/LowVoltageAcOut"),	"/Ac/Out/L1/V");
	alarms->addTripplet("/Alarms/HighVoltageAcOut",	service->item("/Settings/Alarms/HighVoltageAcOut"),	"/Ac/Out/L1/V");
	alarms->addTripplet("/Alarms/LowTemperature",	nullptr,											"");
	alarms->addTripplet("/Alarms/HighTemperature",	service->item("/Settings/Alarms/HighTemperature"),	"");
	alarms->addTripplet("/Alarms/Overload",			service->item("/Settings/Alarms/Overload"),			"/Ac/Out/L1/I");
	alarms->addTripplet("/Alarms/Ripple",			service->item("/Settings/Alarms/Ripple"),			"");
	alarms->addTripplet("/Alarms/LowSoc",			service->item("/Settings/Alarms/LowSoc"),			"/Soc");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createMultiRsAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/LowVoltage",		service->item("/Settings/Alarms/LowVoltage"),		"");
	alarms->addTripplet("/Alarms/HighVoltage",		service->item("/Settings/Alarms/HighVoltage"),		"");
	alarms->addTripplet("/Alarms/LowVoltageAcOut",	service->item("/Settings/Alarms/LowVoltageAcOut"),	""); /* Single phase is not always on L1 */
	alarms->addTripplet("/Alarms/HighVoltageAcOut",	service->item("/Settings/Alarms/HighVoltageAcOut"),	""); /* Single phase is not always on L1 */
	alarms->addTripplet("/Alarms/HighTemperature",	service->item("/Settings/Alarms/HighTemperature"),	"");
	alarms->addTripplet("/Alarms/Overload",			service->item("/Settings/Alarms/Overload"),			""); /* Single phase is not always on L1 */
	alarms->addTripplet("/Alarms/Ripple",			service->item("/Settings/Alarms/Ripple"),			"");
	alarms->addTripplet("/Alarms/LowSoc",			service->item("/Settings/Alarms/LowSoc"),			"/Soc");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createSystemCalcAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Dc/Battery/Alarms/CircuitBreakerTripped", nullptr, "");
	alarms->addTripplet("/Dvcc/Alarms/FirmwareInsufficient", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createVecanAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/SameUniqueNameUsed", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createEssAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/NoGridMeter", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createTankAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/Low/State",  nullptr, "/Level");
	alarms->addTripplet("/Alarms/High/State", nullptr, "/Level");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createPlatformAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Device/DataPartitionError",  nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createDcMeterAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet("/Alarms/LowVoltage",			nullptr,	"");
	alarms->addTripplet("/Alarms/HighVoltage",			nullptr,	"");
	alarms->addTripplet("/Alarms/LowStarterVoltage",	nullptr,	"");
	alarms->addTripplet("/Alarms/HighStarterVoltage",	nullptr,	"");
	alarms->addTripplet("/Alarms/LowTemperature",		nullptr,	"/Dc/0/Temperature");
	alarms->addTripplet("/Alarms/HighTemperature",		nullptr,	"/Dc/0/Temperature");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createAlternatorAlarms(DBusService *service, NotificationCenter *noticationCenter)
{
	DeviceAlarms *alarms = createDcMeterAlarms(service, noticationCenter);

	/* Wakespeed is the only alternator controller we support for now */
	alarms->addWakespeedError("/ErrorCode");

	return alarms;
}

AlarmBusitem::AlarmBusitem(DBusServices *services, NotificationCenter *notificationCenter) :
	QObject(services),
	mNotificationCenter(notificationCenter)
{
	connect(services, SIGNAL(dbusServiceFound(DBusService*)), SLOT(dbusServiceFound(DBusService*)));
}

void AlarmBusitem::dbusServiceFound(DBusService *service)
{
	switch (service->getType())
	{
	case DBusServiceType::DBUS_SERVICE_BATTERY:
		DeviceAlarms::createBatteryAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_FUELCELL:
	case DBusServiceType::DBUS_SERVICE_DCSOURCE:
	case DBusServiceType::DBUS_SERVICE_DCLOAD:
	case DBusServiceType::DBUS_SERVICE_DCSYSTEM:
		DeviceAlarms::createDcMeterAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_ALTERNATOR:
		DeviceAlarms::createAlternatorAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_MULTI:
		new VebusAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_MULTI_RS:
		DeviceAlarms::createMultiRsAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_SOLAR_CHARGER:
		DeviceAlarms::createSolarChargerAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_AC_CHARGER:
		DeviceAlarms::createAcChargerAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_INVERTER:
		DeviceAlarms::createInverterAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_SYSTEM_CALC:
		DeviceAlarms::createSystemCalcAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_GENERATOR_STARTSTOP:
		DeviceAlarms::createGeneratorStartStopAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_DIGITAL_INPUT:
		DeviceAlarms::createDigitalInputAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_VECAN:
		DeviceAlarms::createVecanAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_HUB4:
		DeviceAlarms::createEssAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_TANK:
		DeviceAlarms::createTankAlarms(service, mNotificationCenter);
		break;
	case DBusServiceType::DBUS_SERVICE_PLATFORM:
		DeviceAlarms::createPlatformAlarms(service, mNotificationCenter);
		break;
	default:
		;
	}
}

VebusAlarms::VebusAlarms(DBusService *service, NotificationCenter *noticationCenter) : DeviceAlarms(service, noticationCenter)
{
	mConnectionType = service->item("/Mgmt/Connection");
	mConnectionType->getValueAndChanges(this, SLOT(connectionTypeChanged(QVariant)));
}

void VebusAlarms::init(bool single)
{
	VeQItem *settings = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Alarm");

	addVebusError("/VebusError");
	addTripplet("/Alarms/TemperatureSensor",	settings->itemGetOrCreate("/Vebus/TemperatureSenseError"));
	addTripplet("/Alarms/VoltageSensor",		settings->itemGetOrCreate("/Vebus/VoltageSenseError"));
	addTripplet("/Alarms/LowBattery",			settings->itemGetOrCreate("/Vebus/LowBattery"));
	addTripplet("/Alarms/Ripple",				settings->itemGetOrCreate("/Vebus/HighDcRipple"));
	addTripplet("/Alarms/PhaseRotation");

	// Phase 1 (note the description depends on the number of phases!)
	highTempTextL1Alarm = addTripplet("/Alarms/L1/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	inverterOverloadTextL1Alarm = addTripplet("/Alarms/L1/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));
	// Phase 2
	addTripplet("/Alarms/L2/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	addTripplet("/Alarms/L2/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));
	// Phase 3
	addTripplet("/Alarms/L3/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	addTripplet("/Alarms/L3/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));

	// Grid alarm
	addTripplet("/Alarms/GridLost");

	// DC voltage and current alarms
	addTripplet("/Alarms/HighDcVoltage",		settings->itemGetOrCreate("/Vebus/HighDcVoltage"));
	addTripplet("/Alarms/HighDcCurrent",		settings->itemGetOrCreate("/Vebus/HighDcCurrent"));

	// VE.Bus BMS related alarms
	addTripplet("/Alarms/BmsPreAlarm");
	addTripplet("/Alarms/BmsConnectionLost");

	new mk3FirmwareUpdateNotification(this);
}

void VebusAlarms::connectionTypeChanged(QVariant var)
{
	VeQItem *settings = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Alarm");

	if (var.isValid() && var.value<QString>() == "VE.Can") {
		// backwards compatible, the CAN-bus sends these e.g.
		addTripplet("/Alarms/HighTemperature",		settings->itemGetOrCreate("/Vebus/HighTemperature"));
		addTripplet("/Alarms/Overload",				settings->itemGetOrCreate("/Vebus/InverterOverload"));
		mConnectionType->disconnect(this, SLOT(connectionTypeChanged(QVariant)));
	}
}


BatteryAlarms::BatteryAlarms(DBusService *service, NotificationCenter *noticationCenter) :
	DeviceAlarms(service, noticationCenter), mDistributorAlarmsAdded(false)
{
	mNrOfDistributors = service->item("/NrOfDistributors");
	mNrOfDistributors->getValueAndChanges(this, SLOT(numberOfDistributorsChanged(QVariant)));

	addTripplet("/Alarms/LowVoltage",			nullptr,		"");
	addTripplet("/Alarms/HighVoltage",			nullptr,		"");
	addTripplet("/Alarms/HighCurrent",			nullptr,		"/Dc/0/Current");
	addTripplet("/Alarms/HighChargeCurrent",	nullptr,		"/Dc/0/Current");
	addTripplet("/Alarms/HighDischargeCurrent",	nullptr,		"/Dc/0/Current");
	addTripplet("/Alarms/HighChargeTemperature", nullptr,		"/Dc/0/Temperature");
	addTripplet("/Alarms/LowChargeTemperature", nullptr,		"/Dc/0/Temperature");
	addTripplet("/Alarms/LowSoc",				nullptr,		"/Soc");
	addTripplet("/Alarms/StateOfHealth",		nullptr,		"/Soh");
	addTripplet("/Alarms/LowStarterVoltage",	nullptr,		"");
	addTripplet("/Alarms/HighStarterVoltage",	nullptr,		"");
	addTripplet("/Alarms/LowTemperature",		nullptr,		"/Dc/0/Temperature");
	addTripplet("/Alarms/HighTemperature",		nullptr,		"/Dc/0/Temperature");
	addTripplet("/Alarms/MidVoltage",			nullptr,		"");
	addTripplet("/Alarms/LowFusedVoltage",		nullptr,		"");
	addTripplet("/Alarms/HighFusedVoltage",		nullptr,		"");
	addTripplet("/Alarms/FuseBlown",			nullptr,		"");
	addTripplet("/Alarms/HighInternalTemperature",nullptr,		"");
	addTripplet("/Alarms/InternalFailure",		nullptr,		"");
	addTripplet("/Alarms/BatteryTemperatureSensor",	nullptr,	"");
	addTripplet("/Alarms/CellImbalance",		nullptr,		"");
	addTripplet("/Alarms/LowCellVoltage",		nullptr,		"/System/MinCellVoltage");
	addTripplet("/Alarms/Contactor",			nullptr,		"");
	addTripplet("/Alarms/BmsCable",				nullptr,		"");
	addBmsError("/ErrorCode");
}

void BatteryAlarms::numberOfDistributorsChanged(QVariant var)
{
	if (!var.isValid() || var.toInt() <= 0 || mDistributorAlarmsAdded)
		return;

	/* Register alarms for all distributors and fuses as /NrOfDistributors reflects the number of connected
	 * distributors, not which distributors are connected. I.e. distributor C & D can be present without A & B being present. */
	for (int d = 0; d < 8; d++) {
		const char distName = 'A' + d;
		const QString distPath = QString("/Distributor/%1").arg(distName);
		addTripplet(distPath + "/Alarms/ConnectionLost", nullptr, "");
		for (int f = 0; f < 8; f++) {
			const QString fusePath = (distPath + "/Fuse/%1").arg(f);
			addTripplet( fusePath + "/Alarms/Blown", nullptr, fusePath + "/Name");
		}
	}

	mDistributorAlarmsAdded = true;
}
