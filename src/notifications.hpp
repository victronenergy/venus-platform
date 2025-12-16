#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "venus_services.hpp"
#include "notification.hpp"

class Notifications : public QObject
{
	Q_OBJECT

public:
	explicit Notifications(VeQItem *parentItem, VenusServices *venusServices, QObject *parent = 0);

	bool isAlert() const { return mAlertItem->getLocalValue().toBool(); }
	bool isAlarm() const { return mAlarmItem->getLocalValue().toBool(); }

	int addTrackedNotification(Notification::Type type, const QString &devicename,
									const QString description,
									const QString &service = "",
									const QString &alarmTrigger = "");

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
	VenusServices *mVenusServices;
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

	/*
	 * Simple (original) format: <type>\t<device name>\t<description>
	 * Extended format: <type>\t<device name>\t<description>\t<service name>\t<ID>
	 *
	 * type is an integer corresponding to Notification::Type
	 * device name is a string, e.g. "Inverter/charger"
	 * description is a string, e.g. "Low voltage"
	 * service name is the full service UID e.g. "com.victronenergy.vebus.ttyO1"
	 * ID is a string that can be used to identify the notification. It'll be written to the /Trigger path.
	 *
	 * In the extended format, the notification is not immediately set to inactive.
	 * Instead, the caller is responsible for setting the active and acknowledged state of the notification,
	 * using the returned index and the service name to identify the notification.
	 * Venus-platform monitors the service connection state, and will automatically remove the notification when the service disappears.
	 */
	int setValue(const QVariant &value) override
	{
		QString val = value.toString();
		QStringList list = val.split('\t');

		Notification::Type type;
		QString devicename;
		QString description;
		QString serviceName;
		QString id;

		if (list.size() != 3 && list.size() != 5) {
			return -1;
		}

		bool ok = true;
		int n = list[0].toInt(&ok);
		if (!ok)
			return -2;

		if (n < 0 || n > Notification::LAST)
			return -3;

		type = static_cast<Notification::Type>(n);
		devicename = list[1];
		description = list[2];

		// Simple inject format
		if (list.size() == 3) {
			mNotifications->addNotification(type, devicename, "", description, "")->setActive(false);
			return VeQItemAction::setValue(value);
		}

		// Extended inject format
		serviceName = list[3];
		id = list[4];
		// Return the index of the notification, so it can be tracked by the caller.
		return mNotifications->addTrackedNotification(type, devicename, description, serviceName, id);

	}

private:
	Notifications *mNotifications;
};
