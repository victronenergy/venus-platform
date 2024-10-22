#include "alarm_item.hpp"

/* ok, warning, alarm types */
WarningAlarmMonitor *DeviceAlarms::addTripplet(const QString &description, const QString &busitemPathAlarm,
						VeQItem *busitemSetting, const QString &busitemPathValue)
{
	WarningAlarmMonitor *ret = new WarningAlarmMonitor(mService, AlarmMonitor::REGULAR, busitemPathAlarm,
										 description, busitemSetting, busitemPathValue, this);
	mAlarms.push_back(ret);
	return ret;
}

void DeviceAlarms::addErrorFlag(const QString &description, const QString &busitemPathAlarm, VeQItem *busitemSetting)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::ERROR_FLAG, busitemPathAlarm, description, busitemSetting, "", this));
}

void DeviceAlarms::addVebusError(const QString &busitemPathAlarm)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::VEBUS_ERROR, busitemPathAlarm, "", nullptr, "", this));
}

void DeviceAlarms::addBmsError(const QString &busitemPathAlarm)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::BMS_ERROR, busitemPathAlarm, "", nullptr, "", this));
}

void DeviceAlarms::addChargerError(const QString &busitemPathAlarm)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::CHARGER_ERROR, busitemPathAlarm, "", nullptr, "", this));
}

void DeviceAlarms::addAlternatorError(const QString &busitemPathAlarm)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::ALTERNATOR_ERROR, busitemPathAlarm, "", nullptr, "", this));
}

void DeviceAlarms::addGensetError(const QString &busitemPathAlarm)
{
	mAlarms.push_back(new AlarmMonitor(mService, AlarmMonitor::GENSET_ERROR, busitemPathAlarm, "", nullptr, "", this));
}

DeviceAlarms *DeviceAlarms::createBatteryAlarms(VenusService *service, Notifications *notications)
{
	return new BatteryAlarms(service, notications);
}

DeviceAlarms *DeviceAlarms::createGeneratorStartStopAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Generator not detected at AC input"), "/Alarms/NoGeneratorAtAcIn", nullptr, "");
	alarms->addTripplet(tr("Service interval exceeded"), "/Alarms/ServiceIntervalExceeded", nullptr);
	alarms->addTripplet(tr("GX Auto start/stop is disabled"), "/Alarms/AutoStartDisabled", nullptr);
	alarms->addTripplet(tr("Remote start is disabled on the genset"), "/Alarms/RemoteStartModeDisabled", nullptr);

	return alarms;
}

DeviceAlarms *DeviceAlarms::createDigitalInputAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet("", "/Alarm", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createSolarChargerAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low battery voltage"),		"/Alarms/LowVoltage",	nullptr,	"");
	alarms->addTripplet(tr("High battery voltage"),		"/Alarms/HighVoltage",	nullptr,	"");
	alarms->addTripplet(tr("High temperature"),			"/Alarms/HighTemperature",	nullptr,	"");
	alarms->addTripplet(tr("Short circuit"),			"/Alarms/ShortCircuit",		nullptr,	"");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createAcChargerAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low battery voltage"),		"/Alarms/LowVoltage",	nullptr,	"");
	alarms->addTripplet(tr("High battery voltage"),		"/Alarms/HighVoltage",	nullptr,	"");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createInverterAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low battery voltage"),	"/Alarms/LowVoltage",		service->item("/Settings/AlarmLevel/LowVoltage"),		"");
	alarms->addTripplet(tr("High battery voltage"),	"/Alarms/HighVoltage",		service->item("/Settings/AlarmLevel/HighVoltage"),		"");
	alarms->addTripplet(tr("Low AC voltage"),		"/Alarms/LowVoltageAcOut",	service->item("/Settings/AlarmLevel/LowVoltageAcOut"),	"/Ac/Out/L1/V");
	alarms->addTripplet(tr("High AC voltage"),		"/Alarms/HighVoltageAcOut",	service->item("/Settings/AlarmLevel/HighVoltageAcOut"),	"/Ac/Out/L1/V");
	alarms->addTripplet(tr("Low temperature"),		"/Alarms/LowTemperature",	nullptr,												"");
	alarms->addTripplet(tr("High temperature"),		"/Alarms/HighTemperature",	service->item("/Settings/AlarmLevel/HighTemperature"),	"");
	alarms->addTripplet(tr("Inverter overload"),	"/Alarms/Overload",			service->item("/Settings/AlarmLevel/Overload"),			"/Ac/Out/L1/I");
	alarms->addTripplet(tr("High DC ripple"),		"/Alarms/Ripple",			service->item("/Settings/AlarmLevel/Ripple"),			"");
	alarms->addTripplet(tr("Low SOC"),				"/Alarms/LowSoc",			service->item("/Settings/AlarmLevel/LowSoc"),			"/Soc");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createMultiRsAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low battery voltage"),	"/Alarms/LowVoltage",		service->item("/Settings/AlarmLevel/LowVoltage"),		"");
	alarms->addTripplet(tr("High battery voltage"),	"/Alarms/HighVoltage",		service->item("/Settings/AlarmLevel/HighVoltage"),		"");
	alarms->addTripplet(tr("Low AC voltage"),		"/Alarms/LowVoltageAcOut",	service->item("/Settings/AlarmLevel/LowVoltageAcOut"),	""); /* Single phase is not always on L1 */
	alarms->addTripplet(tr("High AC voltage"),		"/Alarms/HighVoltageAcOut",	service->item("/Settings/AlarmLevel/HighVoltageAcOut"),	""); /* Single phase is not always on L1 */
	alarms->addTripplet(tr("High temperature"),		"/Alarms/HighTemperature",	service->item("/Settings/AlarmLevel/HighTemperature"),	"");
	alarms->addTripplet(tr("Inverter overload"),	"/Alarms/Overload",			service->item("/Settings/AlarmLevel/Overload"),			""); /* Single phase is not always on L1 */
	alarms->addTripplet(tr("High DC ripple"),		"/Alarms/Ripple",			service->item("/Settings/AlarmLevel/Ripple"),			"");
	alarms->addTripplet(tr("Low SOC"),				"/Alarms/LowSoc",			service->item("/Settings/AlarmLevel/LowSoc"),			"/Soc");
	alarms->addTripplet(tr("Short circuit"),		"/Alarms/ShortCircuit",		service->item("/Settings/AlarmLevel/ShortCircuit"),		"");
	alarms->addTripplet(tr("Grid lost"),			"/Alarms/GridLost",			service->item("/Settings/AlarmLevel/GridLost"),			"");
	alarms->addTripplet(tr("Phase rotation"),		"/Alarms/PhaseRotation",	service->item("/Settings/AlarmLevel/PhaseRotation"),	"");
	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createSystemCalcAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Circuit breaker tripped"), "/Dc/Battery/Alarms/CircuitBreakerTripped", nullptr, "");
	alarms->addTripplet(tr("DVCC with incompatible firmware #48"), "/Dvcc/Alarms/FirmwareInsufficient", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createVecanAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet("Please set the VE.Can number to a free one", "/Alarms/SameUniqueNameUsed", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createEssAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Grid meter not found #49"), "/Alarms/NoGridMeter", nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createTankAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low level alarm"),  "/Alarms/Low/State",  nullptr, "/Level");
	alarms->addTripplet(tr("High level alarm"), "/Alarms/High/State", nullptr, "/Level");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createDcMeterAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("Low voltage"),		"/Alarms/LowVoltage",			nullptr,	"");
	alarms->addTripplet(tr("High voltage"),		"/Alarms/HighVoltage",			nullptr,	"");
	alarms->addTripplet(tr("Low aux voltage"),	"/Alarms/LowStarterVoltage",	nullptr,	"");
	alarms->addTripplet(tr("High aux voltage"),	"/Alarms/HighStarterVoltage",	nullptr,	"");
	alarms->addTripplet(tr("Low temperature"),	"/Alarms/LowTemperature",		nullptr,	"/Dc/0/Temperature");
	alarms->addTripplet(tr("High Temperature"),	"/Alarms/HighTemperature",		nullptr,	"/Dc/0/Temperature");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createAlternatorAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = createDcMeterAlarms(service, notications);

	alarms->addChargerError("/ErrorCode"); /* Victron: Orion XS */
	alarms->addAlternatorError("/Error/0/Id"); /* Third party: Wakespeed, ARCO, etc */

	return alarms;
}

DeviceAlarms *DeviceAlarms::createDcdcAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addChargerError("/ErrorCode");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createPlatformAlarms(VenusService *service, Notifications *notications)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, notications);

	alarms->addTripplet(tr("#42 Storage is corrupt on this device"), "/Device/DataPartitionError",  nullptr, "");

	return alarms;
}

DeviceAlarms *DeviceAlarms::createTemperatureSensorAlarms(VenusService *service, Notifications *noticationCenter)
{
	DeviceAlarms *alarms = new DeviceAlarms(service, noticationCenter);

	alarms->addTripplet(tr("Low battery"), "/Alarms/LowBattery", nullptr, "/BatteryVoltage");

	return alarms;
}

VebusAlarms::VebusAlarms(VenusService *service, Notifications *notications) :
	DeviceAlarms(service, notications)
{
	mNumberOfPhases = service->item("/Ac/NumberOfPhases");
	mNumberOfPhases->getValueAndChanges(this, SLOT(numberOfPhasesChanged(QVariant)));
	mConnectionType = service->item("/Mgmt/Connection");
	mConnectionType->getValueAndChanges(this, SLOT(connectionTypeChanged(QVariant)));
}

void VebusAlarms::init(bool single)
{
	VeQItem *settings = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Alarm");

	addVebusError("/VebusError");
	addTripplet(tr("Temperature sense error"),	"/Alarms/TemperatureSensor",	settings->itemGetOrCreate("/Vebus/TemperatureSenseError"));
	addTripplet(tr("Voltage sense error"),		"/Alarms/VoltageSensor",		settings->itemGetOrCreate("/Vebus/VoltageSenseError"));
	addTripplet(tr("Low battery voltage"),		"/Alarms/LowBattery",			settings->itemGetOrCreate("/Vebus/LowBattery"));
	addTripplet(tr("High DC ripple"),			"/Alarms/Ripple",				settings->itemGetOrCreate("/Vebus/HighDcRipple"));
	addTripplet(tr("Wrong phase rotation detected"), "/Alarms/PhaseRotation");

	// Phase 1 (note the description depends on the number of phases!)
	highTempTextL1Alarm = addTripplet(highTempTextL1(single),					"/Alarms/L1/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	inverterOverloadTextL1Alarm = addTripplet(inverterOverloadTextL1(single),	"/Alarms/L1/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));
	// Phase 2
	addTripplet(tr("High Temperature on L2"),	"/Alarms/L2/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	addTripplet(tr("Inverter overload on L2"),	"/Alarms/L2/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));
	// Phase 3
	addTripplet(tr("High Temperature on L3"),	"/Alarms/L3/HighTemperature",	settings->itemGetOrCreate("/Vebus/HighTemperature"));
	addTripplet(tr("Inverter overload on L3"),	"/Alarms/L3/Overload",			settings->itemGetOrCreate("/Vebus/InverterOverload"));

	// Grid alarm
	addTripplet(tr("Grid lost"),				"/Alarms/GridLost");

	// DC voltage and current alarms
	addTripplet(tr("High DC voltage"),			"/Alarms/HighDcVoltage",		settings->itemGetOrCreate("/Vebus/HighDcVoltage"));
	addTripplet(tr("High DC current"),			"/Alarms/HighDcCurrent",		settings->itemGetOrCreate("/Vebus/HighDcCurrent"));

	// VE.Bus BMS related alarms
	addTripplet(tr("BMS pre-alarm"),			"/Alarms/BmsPreAlarm");
	addTripplet(tr("BMS connection lost"),		"/Alarms/BmsConnectionLost");

	new mk3FirmwareUpdateNotification(this);
}

void VebusAlarms::update(bool single)
{
	// update phase 1 (note the description depends on the number of phases!)
	highTempTextL1Alarm->setDescription(highTempTextL1(single));
	inverterOverloadTextL1Alarm->setDescription(inverterOverloadTextL1(single));
}

void VebusAlarms::connectionTypeChanged(QVariant var)
{
	VeQItem *settings = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Alarm");

	if (var.isValid() && var.value<QString>() == "VE.Can") {
		// backwards compatible, the CAN-bus sends these e.g.
		addTripplet(tr("High Temperature"),			"/Alarms/HighTemperature",		settings->itemGetOrCreate("/Vebus/HighTemperature"));
		addTripplet(tr("Inverter overload"),		"/Alarms/Overload",				settings->itemGetOrCreate("/Vebus/InverterOverload"));
		mConnectionType->disconnect(this, SLOT(connectionTypeChanged(QVariant)));
	}
}

void VebusAlarms::numberOfPhasesChanged(QVariant var)
{
	if (!var.isValid())
		return;

	bool singlePhase = var.toInt() == 1;
	if (mAlarms.empty()) {
		init(singlePhase);
	} else {
		update(singlePhase);
	}
}

BatteryAlarms::BatteryAlarms(VenusService *service, Notifications *notications) :
	DeviceAlarms(service, notications), mDistributorAlarmsAdded(false)
{
	mNrOfDistributors = service->item("/NrOfDistributors");
	mNrOfDistributors->getValueAndChanges(this, SLOT(numberOfDistributorsChanged(QVariant)));

	addTripplet(tr("Low voltage"),						"/Alarms/LowVoltage",							nullptr,		"");
	addTripplet(tr("High voltage"),						"/Alarms/HighVoltage",							nullptr,		"");
	addTripplet(tr("High cell voltage"),				"/Alarms/HighCellVoltage",						nullptr,		"");
	addTripplet(tr("High current"),						"/Alarms/HighCurrent",							nullptr,		"");
	addTripplet(tr("High charge current"),				"/Alarms/HighChargeCurrent",					nullptr,		"");
	addTripplet(tr("High discharge current"),			"/Alarms/HighDischargeCurrent",					nullptr,		"");
	addTripplet(tr("High charge temperature"),			"/Alarms/HighChargeTemperature",				nullptr,		"/Dc/0/Temperature");
	addTripplet(tr("Low charge temperature"),			"/Alarms/LowChargeTemperature",					nullptr,		"/Dc/0/Temperature");
	addTripplet(tr("Low SOC"),							"/Alarms/LowSoc",								nullptr,		"/Soc");
	addTripplet(tr("State of health"),					"/Alarms/StateOfHealth",						nullptr,		"/Soh");
	addTripplet(tr("Low starter voltage"),				"/Alarms/LowStarterVoltage",					nullptr,		"");
	addTripplet(tr("High starter voltage"),				"/Alarms/HighStarterVoltage",					nullptr,		"");
	addTripplet(tr("Low temperature"),					"/Alarms/LowTemperature",						nullptr,		"/Dc/0/Temperature");
	addTripplet(tr("High Temperature"),					"/Alarms/HighTemperature",						nullptr,		"/Dc/0/Temperature");
	addTripplet(tr("Mid-point voltage"),				"/Alarms/MidVoltage",							nullptr,		"");
	addTripplet(tr("Low-fused voltage"),				"/Alarms/LowFusedVoltage",						nullptr,		"");
	addTripplet(tr("High-fused voltage"),				"/Alarms/HighFusedVoltage",						nullptr,		"");
	addTripplet(tr("Fuse blown"),						"/Alarms/FuseBlown",							nullptr,		"");
	addTripplet(tr("High internal temperature"),		"/Alarms/HighInternalTemperature",				nullptr,		"");
	addTripplet(tr("Internal failure"),					"/Alarms/InternalFailure",						nullptr,		"");
	addTripplet(tr("Battery temperature sensor"),		"/Alarms/BatteryTemperatureSensor",				nullptr,"");
	addTripplet(tr("Cell imbalance"),					"/Alarms/CellImbalance",						nullptr,		"");
	addTripplet(tr("Low cell voltage"),					"/Alarms/LowCellVoltage",						nullptr,		"/System/MinCellVoltage");
	addTripplet(tr("Bad contactor"),					"/Alarms/Contactor",							nullptr,		"");
	addTripplet(tr("BMS cable fault"),					"/Alarms/BmsCable",								nullptr,		"");
	addErrorFlag(tr("Communication error"),				"/Errors/SmartLithium/Communication",			nullptr);
	addErrorFlag(tr("Invalid battery configuration"),	"/Errors/SmartLithium/InvalidConfiguration",	nullptr);
	addErrorFlag(tr("Incorrect number of batteries"),	"/Errors/SmartLithium/NrOfBatteries",			nullptr);
	addErrorFlag(tr("Battery voltage not supported"),	"/Errors/SmartLithium/Voltage",					nullptr);
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
		addTripplet(tr("Distributor %1 connection lost").arg(distName), distPath + "/Alarms/ConnectionLost", nullptr, "");
		for (int f = 0; f < 8; f++) {
			const QString fusePath = (distPath + "/Fuse/%1").arg(f);
			addTripplet(tr("Fuse blown"), fusePath + "/Alarms/Blown", nullptr, fusePath + "/Name");
		}
	}

	mDistributorAlarmsAdded = true;
}

GensetAlarms::GensetAlarms(VenusService *service, Notifications *notications) :
	DeviceAlarms(service, notications)
{
	mNumberOfPhasesItem = service->item("/NrOfPhases");
	mNumberOfPhasesItem->getValueAndChanges(this, SLOT(numberOfPhasesChanged(QVariant)));

	addGensetError("/Error/0/Id");
}

void GensetAlarms::numberOfPhasesChanged(QVariant var)
{
	if (!var.isValid())
		return;

	mNumberOfPhases = var.toInt();
}

AlarmBusitems::AlarmBusitems(VenusServices *services, Notifications *notifications) :
	QObject(services),
	mNotifications(notifications)
{
	connect(services, SIGNAL(found(VenusService*)), SLOT(onVenusServiceFound(VenusService*)));
}

void AlarmBusitems::onVenusServiceFound(VenusService *service)
{
	switch (service->getType())
	{
	case VenusServiceType::BATTERY:
		DeviceAlarms::createBatteryAlarms(service, mNotifications);
		break;
	case VenusServiceType::FUEL_CELL:
	case VenusServiceType::DC_SOURCE:
	case VenusServiceType::DC_LOAD:
	case VenusServiceType::DC_SYSTEM:
		DeviceAlarms::createDcMeterAlarms(service, mNotifications);
		break;
	case VenusServiceType::ALTERNATOR:
		DeviceAlarms::createAlternatorAlarms(service, mNotifications);
		break;
	case VenusServiceType::MULTI:
		new VebusAlarms(service, mNotifications);
		break;
	case VenusServiceType::MULTI_RS:
		DeviceAlarms::createMultiRsAlarms(service, mNotifications);
		break;
	case VenusServiceType::SOLAR_CHARGER:
		DeviceAlarms::createSolarChargerAlarms(service, mNotifications);
		break;
	case VenusServiceType::AC_CHARGER:
		DeviceAlarms::createAcChargerAlarms(service, mNotifications);
		break;
	case VenusServiceType::INVERTER:
		DeviceAlarms::createInverterAlarms(service, mNotifications);
		break;
	case VenusServiceType::SYSTEM_CALC:
		DeviceAlarms::createSystemCalcAlarms(service, mNotifications);
		break;
	case VenusServiceType::GENERATOR_STARTSTOP:
		DeviceAlarms::createGeneratorStartStopAlarms(service, mNotifications);
		break;
	case VenusServiceType::GENSET:
	case VenusServiceType::DCGENSET:
		new GensetAlarms(service, mNotifications);
		break;
	case VenusServiceType::DIGITAL_INPUT:
		DeviceAlarms::createDigitalInputAlarms(service, mNotifications);
		break;
	case VenusServiceType::VECAN:
		DeviceAlarms::createVecanAlarms(service, mNotifications);
		break;
	case VenusServiceType::HUB4:
		DeviceAlarms::createEssAlarms(service, mNotifications);
		break;
	case VenusServiceType::TANK:
		DeviceAlarms::createTankAlarms(service, mNotifications);
		break;
	case VenusServiceType::DC_DC:
		DeviceAlarms::createDcdcAlarms(service, mNotifications);
		break;
	case VenusServiceType::PLATFORM:
		DeviceAlarms::createPlatformAlarms(service, mNotifications);
		break;
	case VenusServiceType::TEMPERATURE_SENSOR:
		DeviceAlarms::createTemperatureSensorAlarms(service, mNotifications);
		break;
	default:
		;
	}
}
