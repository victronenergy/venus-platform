#ifndef ALARMMONITOR_H
#define ALARMMONITOR_H

#include <QObject>
#include <veutil/qt/ve_qitem.hpp>

#include "src/notification.h"
#include "src/notification_center.h"
#include <src/dbus_service.h>

class DeviceAlarms;

class AlarmMonitor : public QObject
{
	Q_OBJECT

public:
	enum Type {
		REGULAR,
		VEBUS_ERROR,
		CHARGER_ERROR,
		BMS_ERROR,
		WAKESPEED_ERROR
	};

	enum Enabled {
		NO_ALARM,
		ALARM_ONLY,
		ALARM_AND_WARNING
	};

	enum DbusAlarm {
		DBUS_NO_ERROR,
		DBUS_WARNING,
		DBUS_ERROR
	};

	explicit AlarmMonitor(DBusService *service, Type type, const QString &busitemPathAlarm,
						  VeQItem *busitemSetting = nullptr, const QString &busitemPathValue = "",
						  DeviceAlarms *parent = 0);

	~AlarmMonitor();

private slots:
	void notificationDestroyed();
	void settingChanged(QVariant var);
	void settingStateChanged(VeQItem::State state);
	void updateAlarm(QVariant var);

private:
	DBusService *mService;
	Notification *mNotification;
	VeQItem *mBusitemValue;
	VeQItem *mAlarmTrigger;
	Type mType;
	Enabled mEnabledNotifications;
	DeviceAlarms *mDeviceAlarms;
	QString mBusitemPathAlarm;

	bool mustBeShown(DbusAlarm alarm);
	void addNotification(Notification::Type type);
};

#endif
