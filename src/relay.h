#ifndef RELAY_H
#define RELAY_H

#include <QObject>
#include <QString>
#include <src/notification_center.h>
#include <veutil/qt/ve_qitem.hpp>

class Relay : public QObject
{
	Q_OBJECT

public:
	Relay(QString relayPath, NotificationCenter *notificationCenter, QObject *parent = 0);
	bool getRelayOn() const;
	Q_INVOKABLE void setRelayOn(bool on);

private:
	void setHwState(bool closed);
	VeQItem *mPolaritySetting;
	VeQItem *mRelayFunction;
	VeQItem *mRelayState;
	NotificationCenter *mNotificationCenter;
	bool mIntendedState;

private slots:
	void updateRelayState();
	void alarmChanged();

signals:
	void relayOnChanged();
};

#endif
