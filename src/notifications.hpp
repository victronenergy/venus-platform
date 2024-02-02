#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "notification.hpp"

class Notifications : public QObject
{
	Q_OBJECT

public:
	explicit Notifications(VeQItem *parentItem, QObject *parent = 0);

	bool isAlert() const { return mAlertItem->getLocalValue().toBool(); }
	bool isAlarm() const { return mAlarmItem->getLocalValue().toBool(); }

	Notification* addNotification(Notification::Type type, const QString &devicename,
									const QString &value, const QString description,
									const QString &alarmTrigger = "",
									const QVariant &alarmValue = "",
									const QString &serviceName = "");

	void removeNotification(Notification *notification);
	void acknowledgedAll();

signals:
	void alertChanged();
	void alarmChanged();

private slots:
	void activeChanged(Notification *notification);
	void test();

private:
	void updateAlert();
	void updateAlarm();
	void setAlert(bool alert);
	void setAlarm(bool alarm);

	QList<Notification *> mNotifications;
	int const mMaxNotifications = 20;

	VeQItem *mNoficationsItem;
	VeQItem *mNumberOfNotificationsItem;
	VeQItem *mNumberOfActiveNotificationsItem;
	VeQItem *mAlarmItem;
	VeQItem *mAlertItem;
};


class VeQItemAcknowledgeAll : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemAcknowledgeAll(Notifications *notifications) :
		VeQItemAction(),
		mNotifications(notifications)
	{}

	int setValue(const QVariant &value) override
	{
		mNotifications->acknowledgedAll();
		return VeQItemAction::setValue(value);
	}

private:
	Notifications *mNotifications;
};
