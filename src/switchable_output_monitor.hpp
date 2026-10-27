#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "notifications.hpp"
#include "venus_services.hpp"


enum SwitchableOutputFunctionType {
	Alarm = 0,
	GeneratorStartStop = 1,
	Manual = 2,
	TankPump = 3,
	Temperature = 4,
	GensetHelperRelay = 5,
	OpportunityLoad = 6,
};

class SwitchableOutputFunction : public QObject
{
	Q_OBJECT
public:
	SwitchableOutputFunction(SwitchableOutputFunctionType type, VeQItem *channelItem, QObject *parent = nullptr);
	SwitchableOutputFunctionType type() const { return mType; }
	void detach() { mStateItem.clear(); }

protected:
	void setOutput(bool on);
	VeQItem *stateItem() const { return mStateItem; }

private:
	SwitchableOutputFunctionType mType;
	// Guarded, since the channel item can be destroyed before this function is (deleteLater).
	QPointer<VeQItem> mStateItem;
};

class SwitchableOutputFunctionAlarm : public SwitchableOutputFunction
{
	Q_OBJECT
public:
	SwitchableOutputFunctionAlarm(Notifications *notifications, VeQItem *channelItem, QObject *parent = nullptr);

private slots:
	void alarmChanged();

private:
	Notifications *mNotifications;
};

// The output is controlled by dbus-generator, through the generator start/stop proxy of the monitor.
class SwitchableOutputFunctionGeneratorStartStop : public SwitchableOutputFunction
{
	Q_OBJECT
public:
	SwitchableOutputFunctionGeneratorStartStop(VeQItem *channelItem, QObject *parent = nullptr);
	using SwitchableOutputFunction::setOutput;
	QVariant getOutput() const;

signals:
	void outputChanged();

private slots:
	void onStateChanged(QVariant value);
};

// Proxy for the /State of all outputs with the generator start/stop function, written by dbus-generator.
class VeQItemGeneratorStartStopState : public VeQItemQuantity
{
	Q_OBJECT
public:
	int setValue(const QVariant &value) override;

signals:
	void stateRequested(bool on);
};

class SwitchableOutputMonitor : public QObject
{
	Q_OBJECT

public:
	explicit SwitchableOutputMonitor(Notifications *notifications, VenusServices *venus_services,
									 VeQItem *generatorStartStopItem);

signals:
	void generatorStartStopNeededChanged(bool needed);

private slots:
	void onServiceFound(VenusService *service);
	void onServiceConnectedChanged();
	void onServiceRootChildAdded(VeQItem *child);
	void onSwitchableOutputChildAdded(VeQItem *channel);
	void onChannelChildAdded(VeQItem *child);
	void onSettingsChildAdded(VeQItem *child);
	void onSwitchableOutputFunctionChanged(QVariant value);
	void onChannelItemDestroyed(QObject *obj);
	void onGeneratorStartStopRequested(bool on);
	void updateGeneratorStartStopState();

private:
	void handleSwitchableOutputFunctionChanged(VeQItem *channelItem, int function);
	void handleFunctionWhenStateReady(VeQItem *channelItem, int function);
	void removeFunction(VeQItem *channelItem);
	SwitchableOutputFunction *addGeneratorStartStopFunction(VeQItem *channelItem);
	void removeGeneratorStartStopFunction(SwitchableOutputFunction *outputFunction);

	void connectChannelFunction(VeQItem *channel);
	void connectFunctionItem(VeQItem *functionItem);
	QHash<VeQItem *, SwitchableOutputFunction *> mFunctions;
	QHash<VeQItem *, int> mPendingFunctions;
	Notifications *mNotifications;
	VenusServices *mVenusServices;

	// Generator start/stop functions, dbus-generator must run while there are any.
	QSet<SwitchableOutputFunctionGeneratorStartStop *> mGeneratorStartStopFunctions;
	VeQItemGeneratorStartStopState *mGeneratorStartStopState;
	// Last state requested by dbus-generator, applied to outputs that get the function later.
	QVariant mGeneratorStartStopRequest;
};
