#ifndef NOTIFICATION_CENTER_H
#define NOTIFICATION_CENTER_H

#include <QObject>
#include "src/notification.h"
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/notificationdescriptions.h>


class NotificationCenter : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool alert READ getAlert NOTIFY alertChanged)
	Q_PROPERTY(bool alarm READ getAlarm NOTIFY alarmChanged)
	Q_PROPERTY(QList<QObject*> notifications READ getNotifications NOTIFY notificationsChanged)

public:
	NotificationCenter(VeQItem *parentItem, QObject *parent = 0);

	bool getAlert() const { return mAlert; }
	bool getAlarm() const { return mAlarm; }
	QList<QObject *> getNotifications() const { return mNotifications; }

	Notification* addNotification(Notification::Type type, const QString &devicename,
									const QString &value,
									const QString &alarmTrigger = "",
									const QVariant &alarmValue = "",
									const QString &serviceName = "");

	void removeNotification(Notification *notification);

signals:
	void alertChanged();
	void alarmChanged();
	void notificationsChanged();
	void updateDescriptions();

public slots:
	void acknowledgedAll(QVariant var);
	void languageChanged();

private slots:
	void activeChanged(Notification *notification);
	void typeChanged(Notification *notification);
	void test();

private:
	bool mAlert;
	bool mAlarm;
	QList<QObject*> mNotifications;
	int mMaxNotifications;
	VeQItem *mItem;
	NotificationDescriptions *mDescriptions;

	void updateAlert();
	void updateAlarm();

	void setAlert(bool alert) {
		if (alert != mAlert) {
			mAlert = alert;
			mItem->itemGetOrCreate("/Alert")->setValue((int)alert);
			emit alertChanged();
		}
	}

	void setAlarm(bool alarm) {
		if (alarm != mAlarm) {
			mAlarm = alarm;
			mItem->itemGetOrCreate("/Alarm")->setValue((int)alarm);
			emit alarmChanged();
		}
	}
};

#endif
