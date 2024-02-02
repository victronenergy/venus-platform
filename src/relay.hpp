#pragma once

#include <QObject>
#include <QString>

#include <veutil/qt/ve_qitem.hpp>

#include "notifications.hpp"

class Relay : public QObject
{
	Q_OBJECT

public:
	Relay(QString relayPath, Notifications *notifications, QObject *parent = 0);
	bool getRelayOn() const;
	Q_INVOKABLE void setRelayOn(bool on);

private:
	void setHwState(bool closed);
	VeQItem *mPolaritySetting;
	VeQItem *mRelayFunction;
	VeQItem *mRelayState;
	Notifications *mNotifications;
	bool mIntendedState;

private slots:
	void updateRelayState();
	void alarmChanged();

signals:
	void relayOnChanged();
};
