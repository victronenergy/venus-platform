#include "alarm_monitor.hpp"
#include "alarm_item.hpp"

#include <veutil/qt/alternator_error.hpp>
#include <veutil/qt/charger_error.hpp>
#include <veutil/qt/bms_error.hpp>
#include <veutil/qt/vebus_error.hpp>

AlarmMonitor::AlarmMonitor(VenusService *service, Type type, const QString &busitemPathAlarm, const QString &description,
			VeQItem *alarmEnableItem, const QString &alarmValuePath, DeviceAlarms *parent) :
	QObject(parent), mService(service), mNotification(0), mAlarmTrigger(0), mType(type),
	mDeviceAlarms(parent), mBusitemPathAlarm(busitemPathAlarm), mDescription(description)
{
	// Alarms can optionally be enabled / supressed by a setting, but the setting is not necessarily present
	if (alarmEnableItem != nullptr) {
		mEnabledNotifications = NO_ALARM;
		connect(alarmEnableItem, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(settingStateChanged(VeQItem::State)));
		// Get the value, this will trigger a settingStateChanged event
		alarmEnableItem->getValueAndChanges(this, SLOT(settingChanged(QVariant)));
	} else {
		mEnabledNotifications = ALARM_AND_WARNING;
	}

	// Optionally an value can be associated with an alarm, e.g. voltage
	// for an low voltage alarm.
	if (!alarmValuePath.isEmpty()) {
		mBusitemValue = service->item(alarmValuePath);
		mBusitemValue->getText();
	} else {
		mBusitemValue = new VeQItem(0, this);
	}

	// The actual trigger of the alarm, see Type for the supported formats.
	mAlarmTrigger = service->item(busitemPathAlarm);
	mAlarmTrigger->getValueAndChanges(this, SLOT(updateAlarm(QVariant)));
}

AlarmMonitor::~AlarmMonitor()
{
	if (mNotification)
		mNotification->setActive(false);
}

bool AlarmMonitor::mustBeShown(DbusAlarm alarm)
{
	if (alarm == DBUS_NO_ERROR)
		return false;

	if (mEnabledNotifications == ALARM_ONLY && alarm == DBUS_WARNING)
		return false;

	return true;
}

void AlarmMonitor::updateAlarm(QVariant var)
{
	// If there was a previous warning / error it is no longer valid
	if (mNotification) {
		mNotification->setActive(false);
		mNotification->disconnect(this);
		mNotification = 0;
	}

	if (!var.isValid() || mEnabledNotifications == NO_ALARM)
		return;

	DbusAlarm alarm = DBUS_NO_ERROR;
	switch (mType)
	{
	case REGULAR:
		alarm = static_cast<DbusAlarm>(var.toInt());
		break;
	case VEBUS_ERROR:
	{
		int vebusErrorCode = var.toInt();
		if (vebusErrorCode == 0) {
			alarm = DBUS_NO_ERROR;
		} else {
			alarm = DBUS_ERROR;
			mDescription = VebusError::getDescription(vebusErrorCode);
		}
		break;
	}
	case BMS_ERROR:
	{
		int error = var.toInt();
		if (error == 0) {
			alarm = DBUS_NO_ERROR;
		} else {
			alarm = DBUS_ERROR;
			mDescription = BmsError::getDescription(error);
		}
		break;
	}
	case CHARGER_ERROR:
	{
		int error = var.toInt();
		if (error == 0) {
			alarm = DBUS_NO_ERROR;
		} else {
			alarm = ChargerError::isWarning(error) ? DBUS_WARNING : DBUS_ERROR;
			mDescription = ChargerError::getDescription(error);
		}
		break;
	}
	case ALTERNATOR_ERROR:
	{
		auto error = var.toString();
		if (error == "") {
			alarm = DBUS_NO_ERROR;
		} else {
			alarm = error.contains(":e-") ? DBUS_ERROR : DBUS_WARNING;
			mDescription = AlternatorError::getDescription(error);
		}
		break;
	}
	default:
		break;
	}

	if (!mustBeShown(alarm))
		return;

	if (alarm == DBUS_WARNING)
		addNotification(Notification::WARNING);
	else
		addNotification(Notification::ALARM);
}

// Copied to mEnabledNotifications since having the enabled as a setting is optional
void AlarmMonitor::settingChanged(QVariant var)
{
	mEnabledNotifications = static_cast<Enabled>(var.toInt());
	if (mAlarmTrigger)
		updateAlarm(mAlarmTrigger->getValue());
}

void AlarmMonitor::settingStateChanged(VeQItem::State state)
{
	if (state != VeQItem::Offline)
		return;

	/* Setting not present, allow warnings and alarms */
	mEnabledNotifications = ALARM_AND_WARNING;
	disconnect(sender(), SIGNAL(stateChanged(VeQItem::State)), this, SLOT(settingStateChanged(VeQItem::State)));
}

void AlarmMonitor::addNotification(Notification::Type type)
{
	mNotification = mDeviceAlarms->notifications()->addNotification(type, mService->getDescription(),
																		mBusitemValue->getText(), mDescription,
																	mBusitemPathAlarm, mAlarmTrigger->getValue(), mService->getName());
	connect(mNotification, SIGNAL(destroyed()), this, SLOT(notificationDestroyed()));
}

void AlarmMonitor::notificationDestroyed()
{
	mNotification = 0;
}
