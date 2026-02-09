#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <veutil/qt/ve_qitem.hpp>

#include "notifications.hpp"
#include "venus_services.hpp"


enum SwitchableOutputFunction {
	Alarm = 0,
	GeneratorStartStop = 1,
	Manual = 2,
	TankPump = 3,
	Temperature = 4,
	GensetHelperRelay = 5,
	OpportunityLoad = 6,
};

class SwitchableOutputChannel : public QObject
{
	Q_OBJECT
public:
	SwitchableOutputChannel(VeQItem *channelItem, QObject *parent = nullptr);
	VeQItem *getChannelItem() const { return mChannelItem; }

protected:
	VeQItem *mStateItem;

public slots:
	void onServiceConnectedChanged();

private:
	VeQItem *mChannelItem;
	QString mServiceName;
	SwitchableOutputFunction mFunction;
};

class SwitchableOutputFunctionAlarm : public SwitchableOutputChannel
{
	Q_OBJECT
public:
	SwitchableOutputFunctionAlarm(Notifications *notifications, VeQItem *channelItem, QObject *parent = nullptr);

private slots:
	void alarmChanged();

private:
	void setOutput(bool on);
	Notifications *mNotifications;
};


class SwitchableOutputMonitor : public QObject
{
	Q_OBJECT

public:
	explicit SwitchableOutputMonitor(Notifications *notifications, VenusServices *venus_services);

private slots:
	void onServiceFound(VenusService *service);
	void onServiceRootChildAdded(VeQItem *child);
	void onSwitchableOutputChildAdded(VeQItem *channel);
	void onChannelChildAdded(VeQItem *child);
	void onSettingsChildAdded(VeQItem *child);
	void onSwitchableOutputFunctionChanged(QVariant value);
	void onChannelDestroyed(QObject *obj);

private:
	void handleSwitchableOutputFunctionChanged(VeQItem *channelItem, int function);
	void handleFunctionWhenStateReady(VeQItem *channelItem, int function);

	void connectChannelFunction(VeQItem *channel);
	void connectFunctionItem(VeQItem *functionItem);
	QHash<VeQItem *, SwitchableOutputChannel *> mChannels;
	QHash<VeQItem *, int> mPendingFunctions;
	QHash<VeQItem*, VenusService *> mServiceByRoot;
	Notifications *mNotifications;
	VenusServices *mVenusServices;
};
