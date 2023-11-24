#ifndef ALARMBUSITEM_H
#define ALARMBUSITEM_H

#include <QObject>
#include <QLinkedList>

#include <veutil/qt/ve_qitem.hpp>

#include "alarmmonitor.h"
#include <src/dbus_services.h>
#include <src/dbus_service.h>
#include <veutil/qt/dbus_service_type.h>

// Object containing the alarms being monitored in a certain service
class DeviceAlarms : public QObject {
	Q_OBJECT

public:
	DeviceAlarms(DBusService *service, NotificationCenter *noticationCenter) :
		QObject(service),
		mService(service),
		mNotificationCenter(noticationCenter)
	{
	}

	/* ok, warning, alarm types */
	AlarmMonitor *addTripplet(const QString &busitemPathAlarm,
							  VeQItem *dbbusitemSetting = nullptr, const QString &busitemPathValue = "");
	void addVebusError(const QString &busitemPathAlarm);
	void addBmsError(const QString &busitemPathAlarm);
	void addChargerError(const QString &busitemPathAlarm);
	void addWakespeedError(const QString &busitemPathAlarm);

	static DeviceAlarms *createBatteryAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createSolarChargerAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createAcChargerAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createInverterAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createMultiRsAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createSystemCalcAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createGeneratorStartStopAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createDigitalInputAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createVecanAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createEssAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createTankAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createDcMeterAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createAlternatorAlarms(DBusService *service, NotificationCenter *noticationCenter);
	static DeviceAlarms *createPlatformAlarms(DBusService *service, NotificationCenter *noticationCenter);

	NotificationCenter *notificationCenter() { return mNotificationCenter; }

protected:
	DBusService *mService;
	QLinkedList<AlarmMonitor *> mAlarms;
	NotificationCenter *mNotificationCenter;
};

// Object to add alarms to the discovered services
class AlarmBusitem : public QObject
{
	Q_OBJECT

public:
	explicit AlarmBusitem(DBusServices *services, NotificationCenter *notificationCenter);

public slots:
	void dbusServiceFound(DBusService *service);

private:
	NotificationCenter *mNotificationCenter;
};

class mk3FirmwareUpdateNotification;

class VebusAlarms : public DeviceAlarms {
	Q_OBJECT

public:
	VebusAlarms(DBusService *service, NotificationCenter *noticationCenter);

	QString highTempTextL1(bool single) { return single ? tr("High Temperature") : tr("High Temperature on L1"); }
	QString inverterOverloadTextL1(bool single) { return single ? tr("Inverter overload") : tr("Inverter overload on L1"); }

	void init(bool single);

private slots:
	void connectionTypeChanged(QVariant var);

private:
	VeQItem *mConnectionType;
	AlarmMonitor *highTempTextL1Alarm;
	AlarmMonitor *inverterOverloadTextL1Alarm;

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
				mVebusAlarms->notificationCenter()->removeNotification(mNotification);
		}
	}

	void notificationDestroyed() {
		mNotification = 0;
	}

private:
	void addNotification() {
		mNotification = mVebusAlarms->notificationCenter()->addNotification(
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
	BatteryAlarms(DBusService *service, NotificationCenter *noticationCenter);

private slots:
	void numberOfDistributorsChanged(QVariant var);

private:
	VeQItem *mNrOfDistributors;
	bool mDistributorAlarmsAdded;
};

#endif
