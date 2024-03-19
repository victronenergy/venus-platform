#pragma once

#include <QObject>
#include <veutil/qt/ve_qitem.hpp>

#include "notification.hpp"
#include "notifications.hpp"
#include "venus_services.hpp"

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
		ALTERNATOR_ERROR,
		ERROR_FLAG,
		GENSET_ERROR,
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

	explicit AlarmMonitor(VenusService *service, Type type, const QString &busitemPathAlarm,
						  const QString &description = "", VeQItem *busitemSetting = nullptr,
						  const QString &busitemPathValue = "", DeviceAlarms *parent = 0);

	~AlarmMonitor();
	void setDescription(QString description) { mDescription = description; }

	QString serviceDescription() const { return mService->getDescription(); }

private slots:
	void notificationDestroyed();
	void settingChanged(QVariant var);
	void settingStateChanged(VeQItem::State state);
	void updateAlarm(QVariant var);

private:
	VenusService *mService;
	Notification *mNotification;
	VeQItem *mBusitemValue;
	VeQItem *mAlarmTrigger;
	Type mType;
	Enabled mEnabledNotifications;
	DeviceAlarms *mDeviceAlarms;
	QString mBusitemPathAlarm;
	QString mDescription;

	bool mustBeShown(DbusAlarm alarm);
	void addNotification(Notification::Type type);
};

class WarningAlarmMonitor : public AlarmMonitor
{
	Q_OBJECT

public:
	WarningAlarmMonitor(VenusService *service, Type type, const QString &busitemPathAlarm,
							  const QString &description = "", VeQItem *busitemSetting = nullptr,
							  const QString &busitemPathValue = "", DeviceAlarms *parent = 0)
		: AlarmMonitor(service, type, busitemPathAlarm, description, busitemSetting, busitemPathValue, parent)
	{
	}

	void setDescription(QString description) { mDescription = description; }

private:
	QString mDescription;
};
