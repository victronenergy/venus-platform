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
	void acknowledgedChanged(Notification *notification);
	void test();

private:
	void updateAlert();
	void updateAlarm();
	void setAlert(bool alert);
	void setAlarm(bool alarm);

	QList<Notification *> mNotifications;
	int const mMaxNotifications = 20;

	VeQItem *mNotificationsItem;
	VeQItem *mNumberOfNotificationsItem;
	VeQItem *mNumberOfActiveNotificationsItem;
	VeQItem *mNumberOfActiveAlarms;
	VeQItem *mNumberOfActiveWarnings;
	VeQItem *mNumberOfActiveInformations;
	VeQItem *mNumberOfUnAcknowledgedAlarms;
	VeQItem *mNumberOfUnAcknowledgedWarnings;
	VeQItem *mNumberOfUnAcknowledgedInformations;
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

class VeQItemInjectNotification : public VeQItemAction {
	Q_OBJECT
public:
	VeQItemInjectNotification(Notifications *notifications) :
		VeQItemAction(),
		mNotifications(notifications)
	{}

	int setValue(const QVariant &value) override
	{
		QString val = value.toString();
		QStringList list = val.split('\t');
		if (list.size() != 3)
			return -1;

		bool ok = true;
		int n = list[0].toInt(&ok);
		if (!ok)
			return -2;

		if (n < 0 || n > Notification::LAST)
			return -3;

		Notification::Type type = static_cast<Notification::Type>(n);
		mNotifications->addNotification(type, list[1], "", list[2], "")->setActive(false);

		return VeQItemAction::setValue(value);
	}

private:
	Notifications *mNotifications;
};
