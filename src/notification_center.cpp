#include <QTimer>
#include <QDebug>

#include "application.hpp"
#include "notification_center.h"

NotificationCenter::NotificationCenter(VeQItem *parentItem, QObject *parent) :
	QObject(parent),
	mAlert(false), mAlarm(false), mMaxNotifications(20)
{
	mItem = parentItem->itemGetOrCreate("Notifications");
	mItem->itemGetOrCreate("/NumberOfNotifications");
	mItem->itemGetOrCreate("/NumberOfActiveNotifications");
	mItem->itemGetOrCreate("/Alarm");
	mItem->itemGetOrCreate("/Alert");

	auto ackAll = mItem->itemGetOrCreateAndProduce("AcknowledgeAll", 0);
	connect(ackAll, SIGNAL(valueChanged(QVariant)), this, SLOT(acknowledgedAll(QVariant)));

	mDescriptions = new NotificationDescriptions();
}

void NotificationCenter::acknowledgedAll(QVariant var)
{
	Q_UNUSED(var);
	foreach (QObject *obj, mNotifications) {
		Notification *notification = qobject_cast<Notification *>(obj);
		notification->setAcknowledged(true);
	}
	setAlert(false);
	setAlarm(false);
	static_cast<VeQItem *>(QObject::sender())->setValue(0);
}

void NotificationCenter::languageChanged()
{
	emit updateDescriptions();
}

void NotificationCenter::updateAlert()
{
	foreach (QObject *obj, mNotifications) {
		Notification *notification = qobject_cast<Notification *>(obj);
		if (!notification->acknowledged()) {
			setAlert(true);
			return;
		}
	}

	setAlert(false);
}

void NotificationCenter::updateAlarm()
{
	int activeAlarms = 0;
	bool alarm = false;

	foreach (QObject *obj, mNotifications) {
		Notification *notification = qobject_cast<Notification * >(obj);
		if (notification->active()) {
			activeAlarms++;
			if (!alarm && notification->type() == Notification::ALARM && !notification->acknowledged()) {
				alarm = true;
			}
		}
	}
	mItem->itemGet("/NumberOfActiveNotifications")->setValue(activeAlarms);
	mItem->itemGet("/NumberOfNotifications")->setValue(mNotifications.length());

	if (alarm) {
		setAlarm(true);
		return;
	}

	setAlarm(false);
}

Notification *NotificationCenter::addNotification(Notification::Type type, const QString &devicename,
													const QString &value, const QString &alarmTrigger,
													const QVariant &alarmValue, const QString &serviceName)
{
	Notification *notification;
	int index = mNotifications.size();
	auto activeNotificationsItem = mItem->itemGet("/NumberOfActiveNotifications");
	int activeNotifications =activeNotificationsItem->getValue().toInt();



	if (mNotifications.size() >= mMaxNotifications) {
		notification = qobject_cast<Notification *>(mNotifications.last());
		index = notification->getIndex();
		removeNotification(notification);
	}

	notification = new Notification(type, devicename, value, serviceName, alarmTrigger, alarmValue, mItem, index, mDescriptions, this);
	mNotifications.insert(0, notification);
	connect(notification, SIGNAL(activeChanged(Notification *)), this, SLOT(activeChanged(Notification *)));
	connect(notification, SIGNAL(typeChanged(Notification *)), this, SLOT(typeChanged(Notification *)));
	connect(this,SIGNAL(updateDescriptions()), notification, SLOT(updateDescription()));
	emit notificationsChanged();

	mItem->itemGet("/NumberOfNotifications")->setValue(mNotifications.length());

	if (notification->type() == Notification::ALARM)
		setAlarm(true);

	if (notification->active()) {
		setAlert(true);
		activeNotificationsItem->setValue(++activeNotifications);
	}

	return notification;
}

void NotificationCenter::removeNotification(Notification *notification)
{
	notification->setActive(false);
	mNotifications.removeOne(notification);
	emit notificationsChanged();
	delete notification;
	updateAlarm();
	updateAlert();
}

void NotificationCenter::activeChanged(Notification *notification)
{
	Q_UNUSED(notification);
	updateAlarm();
	updateAlert();
}

void NotificationCenter::typeChanged(Notification *notification)
{
	if (notification->type() == Notification::ALARM) {
		setAlarm(true);
		setAlert(true);
	}
}

void NotificationCenter::test()
{
	qDebug() << "NotificationCenter test()";
	addNotification(Notification::ALARM, "Multi", "VeBus error", "value");
	addNotification(Notification::NOTIFICATION, "Multi", "VeBus error", "value");
}
