#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem.hpp>

#include "alarm_monitor.hpp"
#include "venus_services.hpp"

// Object containing the alarms being monitored in a certain service
class DeviceAlarms : public QObject {
	Q_OBJECT

public:
	DeviceAlarms(VenusService *service, Notifications *notifications) :
		QObject(service),
		mService(service),
		mNotifications(notifications)
	{
	}

	/* ok, warning, alarm types */
	WarningAlarmMonitor *addTripplet(const QString &description, const QString &busitemPathAlarm,
							VeQItem *busitemSetting = nullptr, const QString &busitemPathValue = "");
	void addVebusError(const QString &busitemPathAlarm);
	void addBmsError(const QString &busitemPathAlarm);
	void addChargerError(const QString &busitemPathAlarm);
	void addAlternatorError(const QString &busitemPathAlarm);

	static DeviceAlarms *createBatteryAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createSolarChargerAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createAcChargerAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createInverterAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createMultiRsAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createSystemCalcAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createGeneratorStartStopAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createDigitalInputAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createVecanAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createEssAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createTankAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createDcMeterAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createAlternatorAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createDcdcAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createPlatformAlarms(VenusService *service, Notifications *notifications);
	static DeviceAlarms *createTemperatureSensorAlarms(VenusService *service, Notifications *notifications);

	Notifications *notifications() { return mNotifications; }

protected:
	VenusService *mService;
	std::vector<AlarmMonitor *> mAlarms;
	Notifications *mNotifications;
};

class mk3FirmwareUpdateNotification;

class VebusAlarms : public DeviceAlarms {
	Q_OBJECT

public:
	VebusAlarms(VenusService *service, Notifications *notifications);

	QString highTempTextL1(bool single) { return single ? tr("High Temperature") : tr("High Temperature on L1"); }
	QString inverterOverloadTextL1(bool single) { return single ? tr("Inverter overload") : tr("Inverter overload on L1"); }

	void init(bool single);

	void update(bool single);

private slots:
	void numberOfPhasesChanged(QVariant var);
	void connectionTypeChanged(QVariant var);

private:
	VeQItem *mNumberOfPhases;
	VeQItem *mConnectionType;
	WarningAlarmMonitor *highTempTextL1Alarm;
	WarningAlarmMonitor *inverterOverloadTextL1Alarm;

	friend class mk3FirmwareUpdateNotification;
};

class mk3FirmwareUpdateNotification : public QObject
{
	Q_OBJECT

public:
	mk3FirmwareUpdateNotification(VebusAlarms *alarms) :
		mVebusAlarms(alarms)
	{
		mk3Version = alarms->mService->item("Interfaces/Mk2/Version");
		updateSetting = mk3Version->itemRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Vebus/AllowMk3Fw212Update");

		mk3Version->getValueAndChanges(this, SLOT(checkMk3Version()));
		updateSetting->getValueAndChanges(this, SLOT(checkMk3Version()));
	}

private slots:
	void checkMk3Version() {
		QVariant version = mk3Version->getValue();
		QVariant updateAllowed = updateSetting->getValue();
		if (!version.isValid() || !updateAllowed.isValid())
			return;

		if (version.value<uint>() == 1170212 && updateAllowed.value<int>() == 0) {
			if (!mNotification)
				addNotification();
		} else {
			if (mNotification)
				mVebusAlarms->notifications()->removeNotification(mNotification);
		}
	}

	void notificationDestroyed() {
		mNotification = 0;
	}

private:
	void addNotification() {
		mNotification = mVebusAlarms->notifications()->addNotification(
					Notification::WARNING, "#51 mk3 firmware needs update",
											"see Device list -> Inverter/charger entry", "");
		connect(mNotification, SIGNAL(destroyed()), this, SLOT(notificationDestroyed()));
	}

	VeQItem *mk3Version;
	VeQItem *updateSetting;
	Notification *mNotification = nullptr;
	VebusAlarms *mVebusAlarms;
};

class BatteryAlarms : public DeviceAlarms {
	Q_OBJECT

public:
	BatteryAlarms(VenusService *service, Notifications *notifications);

private slots:
	void numberOfDistributorsChanged(QVariant var);

private:
	VeQItem *mNrOfDistributors;
	bool mDistributorAlarmsAdded;
};

// Object to add alarms to the discovered services
class AlarmBusitems : public QObject
{
	Q_OBJECT

public:
	explicit AlarmBusitems(VenusServices *services, Notifications *notifications);

private slots:
	void onVenusServiceFound(VenusService *service);

private:
	Notifications *mNotifications;
};
