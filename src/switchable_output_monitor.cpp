#include "switchable_output_monitor.hpp"
#include <QPointer>

SwitchableOutputMonitor::SwitchableOutputMonitor(Notifications *notifications, VenusServices *venus_services) :
		QObject(venus_services), mNotifications(notifications), mVenusServices(venus_services)
{
	connect(mVenusServices, SIGNAL(found(VenusService*)), this, SLOT(onServiceFound(VenusService*)));
}

void SwitchableOutputMonitor::onServiceFound(VenusService *service)
{
	VeQItem *root = service->item();
	mServiceByRoot.insert(root, service);

	// Switchable outputs can be added later.
	connect(root, SIGNAL(childAdded(VeQItem*)), this, SLOT(onServiceRootChildAdded(VeQItem*)));

	VeQItem *switchable = root->itemGet("SwitchableOutput");
	if (!switchable)
		return;

	// Handle already existing channels.
	for (VeQItem *channel : switchable->itemChildren())
		connectChannelFunction(channel);

	// The /Function path can be added later, so also connect to childAdded signal of /SwitchableOutput
	connect(switchable, SIGNAL(childAdded(VeQItem*)), this, SLOT(onSwitchableOutputChildAdded(VeQItem*)));
}

// Handle children added to the root.
void SwitchableOutputMonitor::onServiceRootChildAdded(VeQItem *child)
{
	if (!child || child->id() != "SwitchableOutput")
		return;

	// Handle already existing channels.
	for (VeQItem *channel : child->itemChildren())
		connectChannelFunction(channel);

	// Handle channels added later.
	connect(child, SIGNAL(childAdded(VeQItem*)), this, SLOT(onSwitchableOutputChildAdded(VeQItem*)));
}

void SwitchableOutputMonitor::onSwitchableOutputChildAdded(VeQItem *channel)
{
	connectChannelFunction(channel);
}

void SwitchableOutputMonitor::connectChannelFunction(VeQItem *channel)
{
	if (!channel)
		return;

	// Observe channel lifecycle and late-added children (State/Settings).
	connect(channel, SIGNAL(childAdded(VeQItem*)),
			this, SLOT(onChannelChildAdded(VeQItem*)), Qt::UniqueConnection);
	connect(channel, SIGNAL(destroyed(QObject*)),
			this, SLOT(onChannelDestroyed(QObject*)), Qt::UniqueConnection);

	VeQItem *functionItem = channel->itemGet("Settings/Function");
	if (functionItem) {
		connectFunctionItem(functionItem);
	}
	// Function item can be added later, so also connect to childAdded signals
	else {
		VeQItem *settingsItem = channel->itemGet("Settings");
		if (settingsItem) {
			connect(settingsItem, SIGNAL(childAdded(VeQItem*)),
					this, SLOT(onSettingsChildAdded(VeQItem*)), Qt::UniqueConnection);
		}
	}
}

void SwitchableOutputMonitor::onChannelChildAdded(VeQItem *child)
{
	if (!child)
		return;

	if (child->id() == "Settings") {
		// Settings/Function can arrive later; keep wiring to detect function changes.
		connect(child, SIGNAL(childAdded(VeQItem*)),
				this, SLOT(onSettingsChildAdded(VeQItem*)), Qt::UniqueConnection);

		VeQItem *functionItem = child->itemGet("Function");
		if (functionItem)
			connectFunctionItem(functionItem);
		return;
	}

	if (child->id() == "State") {
		// Function handling is deferred until State exists to avoid acting on incomplete channels.
		VeQItem *channel = child->itemParent();
		if (!channel)
			return;

		// If /State arrived late, we might have already seen the function, so apply it now.
		if (mPendingFunctions.contains(channel)) {
			// Both /State and /Function are now present, childAdded handlers are no longer needed.
			disconnect(channel, SIGNAL(childAdded(VeQItem*)), this, SLOT(onChannelChildAdded(VeQItem*)));
			handleFunctionWhenStateReady(channel, mPendingFunctions.value(channel));
		}
	}
}

void SwitchableOutputMonitor::onSettingsChildAdded(VeQItem *child)
{
	if (!child || child->id() != "Function")
		return;

	// No longer needed after Function is added.
	disconnect(child->parent(), SIGNAL(childAdded(VeQItem*)), this, SLOT(onSettingsChildAdded(VeQItem*)));
	connectFunctionItem(child);
}

void SwitchableOutputMonitor::connectFunctionItem(VeQItem *functionItem)
{
	if (!functionItem)
		return;

	functionItem->getValueAndChanges(this, SLOT(onSwitchableOutputFunctionChanged(QVariant)));
}

void SwitchableOutputMonitor::onSwitchableOutputFunctionChanged(QVariant value)
{
	if (!value.isValid())
		return;

	bool ok = false;
	int function = value.toInt(&ok);
	if (!ok)
		return;

	VeQItem *functionItem = qobject_cast<VeQItem *>(sender());
	if (!functionItem)
		return;

	VeQItem *settings = functionItem->itemParent();
	if (!settings)
		return;

	VeQItem *channel = settings->itemParent();
	if (!channel)
		return;

	handleFunctionWhenStateReady(channel, function);
}

void SwitchableOutputMonitor::handleFunctionWhenStateReady(VeQItem *channelItem, int function)
{
	if (!channelItem)
		return;

	// Wait for State to exist before applying any function logic.
	if (channelItem->itemGet("State")) {
		mPendingFunctions.remove(channelItem);
		handleSwitchableOutputFunctionChanged(channelItem, function);
		return;
	}

	// Remember the last function until the channel is fully formed.
	mPendingFunctions.insert(channelItem, function);
	connect(channelItem, SIGNAL(childAdded(VeQItem*)),
			this, SLOT(onChannelChildAdded(VeQItem*)), Qt::UniqueConnection);
}

void SwitchableOutputMonitor::handleSwitchableOutputFunctionChanged(VeQItem *channelItem, int function)
{
	SwitchableOutputChannel *channel = nullptr;

	// Clean up any existing channel logic before applying the new function.
	if (mChannels.contains(channelItem)) {
				qInfo() << "Removing function for channel" << channelItem->id();
				mChannels.take(channelItem)->deleteLater();
			}

	// Create and connect the new channel logic based on the function.
	switch (function) {
		case SwitchableOutputFunction::Alarm:
			{
				qInfo() << "Creating Alarm function for channel" << channelItem->id();
				channel = new SwitchableOutputFunctionAlarm(mNotifications, channelItem, this);
				break;
			}
			break;
		default:
			break;
	}

	// If a channel was created, keep track of it and connect to service changes for cleanup on disconnect.
	if (channel) {
		mChannels.insert(channelItem, channel);
		/* Clean up if the channel is destroyed for whatever reason.
		 * The QPointer ensures we don't try to remove the channel again
		 * if it was already removed by other means (e.g. service disconnect).
		*/
		QPointer<VeQItem> key = channelItem;
		connect(channel, &QObject::destroyed, this, [this, key] {
			if (key)
				mChannels.remove(key);
		});

		//<channel> -> /SwitchableOutput -> service root
		VeQItem *serviceRoot = channelItem->itemParent()->itemParent();

		if (serviceRoot) {
			VenusService *service = mServiceByRoot.value(serviceRoot, nullptr);
			if (service) {
				qDebug() << "Channel" << channelItem->id() << "in service" << service->getName() << "set to function" << function;
				// VenusService will not emit destroyed(), so we use connectedChanged to detect disconnections and clean up channels.
				connect(service, SIGNAL(connectedChanged()), channel, SLOT(onServiceConnectedChanged()));
			}
		}
	}
}

void SwitchableOutputMonitor::onChannelDestroyed(QObject *obj)
{
	VeQItem *channelItem = qobject_cast<VeQItem *>(obj);
	if (!channelItem)
		return;

	// Prevent leaks and stale entries if the channel disappears mid-setup.
	mPendingFunctions.remove(channelItem);
	if (mChannels.contains(channelItem)) {
		mChannels.take(channelItem)->deleteLater();
	}
}


void SwitchableOutputChannel::onServiceConnectedChanged()
{
	VenusService *service = qobject_cast<VenusService *>(sender());
	if (!service)
		return;

	// Not connected anymore, clean up the channel.
	if (!service->getConnected()) {
		qInfo() << "Service " << service->getName() << " disconnected, destroying channel " << this->mChannelItem->id();
		this->deleteLater();
	}
}

SwitchableOutputChannel::SwitchableOutputChannel(VeQItem *channelItem, QObject *parent) :
		QObject(parent),
		mStateItem(nullptr),
		mChannelItem(channelItem),
		mServiceName(channelItem ? channelItem->id() : QString()),
		mFunction(SwitchableOutputFunction::Manual)
{
	if (channelItem)
		mStateItem = channelItem->itemGet("State");
}

SwitchableOutputFunctionAlarm::SwitchableOutputFunctionAlarm(Notifications *notifications,
		VeQItem *channelItem, QObject *parent) :
		SwitchableOutputChannel(channelItem, parent),
		mNotifications(notifications)
{
	if (mNotifications) {
		connect(mNotifications, SIGNAL(alarmChanged()), this, SLOT(alarmChanged()));
		setOutput(mNotifications->isAlarm());
	}
}

void SwitchableOutputFunctionAlarm::setOutput(bool on)
{
	if (mStateItem)
		mStateItem->setValue(on ? 1 : 0);
}

void SwitchableOutputFunctionAlarm::alarmChanged()
{
	if (mNotifications)
		setOutput(mNotifications->isAlarm());
}
