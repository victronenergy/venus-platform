#include <QTimer>
#include <QDebug>

#include "application.hpp"
#include "notifications.hpp"

Notifications::Notifications(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	mNotificationsItem = parentItem->itemGetOrCreate("Notifications");
	mNumberOfNotificationsItem = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfNotifications", 0);
	mNumberOfActiveNotificationsItem = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfActiveNotifications", 0);
	mNumberOfActiveAlarms = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfActiveAlarms", 0);
	mNumberOfActiveWarnings = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfActiveWarnings", 0);
	mNumberOfActiveInformations = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfActiveInformations", 0);
	mNumberOfUnacknowledgedAlarms = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfUnacknowledgedAlarms", 0);
	mNumberOfUnacknowledgedWarnings = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfUnacknowledgedWarnings", 0);
	mNumberOfUnacknowledgedInformations = mNotificationsItem->itemGetOrCreateAndProduce("NumberOfUnacknowledgedInformations", 0);
	mAlarmItem = mNotificationsItem->itemGetOrCreate("Alarm");
	mAlertItem = mNotificationsItem->itemGetOrCreate("Alert");
	mNumberOfNotificationsItem = mNotificationsItem->itemGetOrCreate("NumberOfNotifications");
	mNumberOfActiveNotificationsItem = mNotificationsItem->itemGetOrCreate("NumberOfActiveNotifications");
	mAlarmItem = mNotificationsItem->itemGetOrCreate("Alarm");
	mAlertItem = mNotificationsItem->itemGetOrCreate("Alert");
	mNotificationsItem->itemAddChild("AcknowledgeAll", new VeQItemAcknowledgeAll(this));
	mNotificationsItem = parentItem->itemGetOrCreate("Notifications");


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
		if (notification->isActive())
			activeNotifications--;
		removeNotification(notification);
	}

	notification = new Notification(type, devicename, description, value, serviceName, alarmTrigger, alarmValue, mNotificationsItem, index, this);
	mNotifications.insert(0, notification);
	connect(notification, SIGNAL(activeChanged(Notification *)), this, SLOT(activeChanged(Notification *)));
	connect(notification, SIGNAL(acknowledgedChanged(Notification *)), this, SLOT(acknowledgedChanged(Notification *)));
	mNumberOfNotificationsItem->produceValue(mNotifications.length());

	if (type == Notification::ALARM)
	{
		setAlarm(true);
		mNumberOfActiveAlarms->produceValue(mNumberOfActiveAlarms->getValue().toInt() + 1);
		mNumberOfUnacknowledgedAlarms->produceValue(mNumberOfUnacknowledgedAlarms->getValue().toInt() + 1);
	} else if (type == Notification::WARNING)
	{
		mNumberOfActiveWarnings->produceValue(mNumberOfActiveWarnings->getValue().toInt() + 1);
		mNumberOfUnacknowledgedWarnings->produceValue(mNumberOfUnacknowledgedWarnings->getValue().toInt() + 1);
	} else
	{
		mNumberOfActiveInformations->produceValue(mNumberOfActiveInformations->getValue().toInt() + 1);
		mNumberOfUnacknowledgedInformations->produceValue(mNumberOfUnacknowledgedInformations->getValue().toInt() + 1);
	}

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
	notification->setAcknowledged(true);
	mNotifications.removeOne(notification);
	delete notification;
	updateAlarm();
	updateAlert();
}

void Notifications::activeChanged(Notification *notification)
{
	updateAlarm();
	updateAlert();

	if (notification->type() == Notification::ALARM)
		mNumberOfActiveAlarms->produceValue(mNumberOfActiveAlarms->getValue().toInt() - 1);
	else if (notification->type() == Notification::WARNING)
		mNumberOfActiveWarnings->produceValue(mNumberOfActiveWarnings->getValue().toInt() - 1);
	else
		mNumberOfActiveInformations->produceValue(mNumberOfActiveInformations->getValue().toInt() - 1);
}

void Notifications::acknowledgedChanged(Notification *notification)
{
	updateAlarm();
	updateAlert();

	bool acknowledged = notification->isAcknowledged();
	int numUnacknowledged;
	if (notification->type() == Notification::ALARM)
	{
		numUnacknowledged = mNumberOfUnacknowledgedAlarms->getValue().toInt();
		mNumberOfUnacknowledgedAlarms->produceValue(acknowledged ? numUnacknowledged - 1: numUnacknowledged + 1);
	}
	else if (notification->type() == Notification::WARNING)
	{
		numUnacknowledged = mNumberOfUnacknowledgedWarnings->getValue().toInt();
		mNumberOfUnacknowledgedWarnings->produceValue(acknowledged ? numUnacknowledged - 1: numUnacknowledged + 1);
	}
	else
	{
		numUnacknowledged = mNumberOfUnacknowledgedInformations->getValue().toInt();
		mNumberOfUnacknowledgedInformations->produceValue(acknowledged ? numUnacknowledged - 1: numUnacknowledged + 1);
	}
}

void Notifications::test()
{
	qDebug() << "NotificationCenter test()";
	addNotification(Notification::ALARM, "Multi", "VeBus error", "value");
	addNotification(Notification::NOTIFICATION, "Multi", "VeBus error", "value");
}
