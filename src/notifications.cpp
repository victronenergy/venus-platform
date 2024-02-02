#include <QTimer>
#include <QDebug>

#include "application.hpp"
#include "notifications.hpp"

Notifications::Notifications(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	mNoficationsItem = parentItem->itemGetOrCreate("Notifications");
	mNumberOfNotificationsItem = mNoficationsItem->itemGetOrCreate("NumberOfNotifications");
	mNumberOfActiveNotificationsItem = mNoficationsItem->itemGetOrCreate("NumberOfActiveNotifications");
	mAlarmItem = mNoficationsItem->itemGetOrCreate("Alarm");
	mAlertItem = mNoficationsItem->itemGetOrCreate("Alert");
	mNoficationsItem->itemAddChild("AcknowledgeAll", new VeQItemAcknowledgeAll(this));

	//QTimer::singleShot(9000, this, SLOT(test()));
}

void Notifications::acknowledgedAll()
{
	for (Notification *notification: mNotifications)
		notification->setAcknowledged(true);

	setAlert(false);
	setAlarm(false);
}

void Notifications::updateAlert()
{
	for (Notification *notification: mNotifications) {
		if (!notification->isAcknowledged()) {
			setAlert(true);
			return;
		}
	}

	setAlert(false);
}

void Notifications::updateAlarm()
{
	int activeNotifications = 0;
	bool alarm = false;

	for (Notification *notification: mNotifications) {
		if (notification->isActive()) {
			activeNotifications++;
			if (!alarm && notification->type() == Notification::ALARM && !notification->isAcknowledged()) {
				alarm = true;
			}
		}
	}

	mNumberOfActiveNotificationsItem->produceValue(activeNotifications);
	mNumberOfNotificationsItem->produceValue(mNotifications.length());
	setAlarm(alarm);
}

void Notifications::setAlert(bool alert)
{
	if (alert != isAlert()) {
		mAlertItem->produceValue(QVariant::fromValue(alert));
		emit alertChanged();
	}
}

void Notifications::setAlarm(bool alarm)
{
	if (alarm != isAlarm()) {
		mAlarmItem->produceValue(QVariant::fromValue(alarm));
		emit alarmChanged();
	}
}

Notification *Notifications::addNotification(Notification::Type type, const QString &devicename,
											 const QString &value, const QString description, const QString &alarmTrigger,
											 const QVariant &alarmValue, const QString &serviceName)
{
	Notification *notification;
	int index = mNotifications.size();
	int activeNotifications = mNumberOfActiveNotificationsItem->getValue().toInt();

	if (mNotifications.size() >= mMaxNotifications) {
		notification = mNotifications.last();
		index = notification->getIndex();
		removeNotification(notification);
	}

	notification = new Notification(type, devicename, description, value, serviceName, alarmTrigger, alarmValue, mNoficationsItem, index, this);
	mNotifications.insert(0, notification);
	connect(notification, SIGNAL(activeChanged(Notification *)), this, SLOT(activeChanged(Notification *)));
	mNumberOfNotificationsItem->produceValue(mNotifications.length());

	if (notification->type() == Notification::ALARM)
		setAlarm(true);

	if (notification->isActive()) {
		activeNotifications++;
		setAlert(true);
		mNumberOfActiveNotificationsItem->produceValue(activeNotifications);
	}

	return notification;
}

void Notifications::removeNotification(Notification *notification)
{
	notification->setActive(false);
	mNotifications.removeOne(notification);
	delete notification;
	updateAlarm();
	updateAlert();
}

void Notifications::activeChanged(Notification *notification)
{
	Q_UNUSED(notification);
	updateAlarm();
	updateAlert();
}

void Notifications::test()
{
	qDebug() << "NotificationCenter test()";
	addNotification(Notification::ALARM, "Multi", "VeBus error", "value");
	addNotification(Notification::NOTIFICATION, "Multi", "VeBus error", "value");
}
