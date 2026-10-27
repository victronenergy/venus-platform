#include "switchable_output_monitor.hpp"

int VeQItemGeneratorStartStopState::setValue(const QVariant &value)
{
	bool ok = false;
	int state = value.toInt(&ok);
	if (!ok || (state != 0 && state != 1))
		return -1;

	// The value is produced from the actual output state, once the outputs follow.
	emit stateRequested(state == 1);
	return 0;
}

SwitchableOutputMonitor::SwitchableOutputMonitor(Notifications *notifications, VenusServices *venus_services,
												 VeQItem *generatorStartStopItem) :
		QObject(venus_services), mNotifications(notifications), mVenusServices(venus_services)
{
	connect(mVenusServices, SIGNAL(found(VenusService*)), this, SLOT(onServiceFound(VenusService*)));

	mGeneratorStartStopState = new VeQItemGeneratorStartStopState();
	generatorStartStopItem->itemAddChild("State", mGeneratorStartStopState);
	connect(mGeneratorStartStopState, SIGNAL(stateRequested(bool)), this, SLOT(onGeneratorStartStopRequested(bool)));
}

void SwitchableOutputMonitor::onServiceFound(VenusService *service)
{
	VeQItem *root = service->item();
	// VenusService will not emit destroyed(), so use connectedChanged to clean up functions on disconnect.
	connect(service, SIGNAL(connectedChanged()), this, SLOT(onServiceConnectedChanged()));

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

void SwitchableOutputMonitor::onServiceConnectedChanged()
{
	VenusService *service = qobject_cast<VenusService *>(sender());
	if (!service || service->getConnected())
		return;

	qInfo() << "Service" << service->getName() << "disconnected, removing its functions";
	// <channel> -> /SwitchableOutput -> service root
	for (VeQItem *channelItem : mFunctions.keys()) {
		VeQItem *switchable = channelItem->itemParent();
		if (switchable && switchable->itemParent() == service->item())
			removeFunction(channelItem);
	}
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
			this, SLOT(onChannelItemDestroyed(QObject*)), Qt::UniqueConnection);

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
	disconnect(child->itemParent(), SIGNAL(childAdded(VeQItem*)), this, SLOT(onSettingsChildAdded(VeQItem*)));
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
	SwitchableOutputFunction *existing = mFunctions.value(channelItem, nullptr);
	if (existing && existing->type() == function)
		return;

	// Clean up any existing function before applying the new function.
	removeFunction(channelItem);

	// Create the new function based on the function type.
	SwitchableOutputFunction *outputFunction = nullptr;
	switch (function) {
	case SwitchableOutputFunctionType::Alarm:
		qInfo() << "Creating Alarm function for channel" << channelItem->id();
		outputFunction = new SwitchableOutputFunctionAlarm(mNotifications, channelItem, this);
		break;
	case SwitchableOutputFunctionType::GeneratorStartStop:
		qInfo() << "Creating GeneratorStartStop function for channel" << channelItem->id();
		outputFunction = addGeneratorStartStopFunction(channelItem);
		break;
	default:
		break;
	}

	if (outputFunction)
		mFunctions.insert(channelItem, outputFunction);
}

// All functions are removed here, so the bookkeeping is up to date immediately, not only once deleteLater is done.
void SwitchableOutputMonitor::removeFunction(VeQItem *channelItem)
{
	SwitchableOutputFunction *outputFunction = mFunctions.take(channelItem);
	if (!outputFunction)
		return;

	qInfo() << "Removing function for channel" << channelItem->id();
	removeGeneratorStartStopFunction(outputFunction);
	// Stop it from controlling the output while the delete is pending.
	outputFunction->detach();
	outputFunction->deleteLater();
}

void SwitchableOutputMonitor::onChannelItemDestroyed(QObject *obj)
{
	VeQItem *channelItem = qobject_cast<VeQItem *>(obj);
	if (!channelItem)
		return;

	// Prevent leaks and stale entries if the channel disappears mid-setup.
	mPendingFunctions.remove(channelItem);
	removeFunction(channelItem);
}

SwitchableOutputFunction *SwitchableOutputMonitor::addGeneratorStartStopFunction(VeQItem *channelItem)
{
	SwitchableOutputFunctionGeneratorStartStop *outputFunction = new SwitchableOutputFunctionGeneratorStartStop(channelItem, this);
	connect(outputFunction, SIGNAL(outputChanged()), this, SLOT(updateGeneratorStartStopState()));

	// Let a newly added output follow the last request of dbus-generator.
	if (mGeneratorStartStopRequest.isValid())
		outputFunction->setOutput(mGeneratorStartStopRequest.toBool());

	mGeneratorStartStopFunctions.insert(outputFunction);
	if (mGeneratorStartStopFunctions.size() == 1)
		emit generatorStartStopNeededChanged(true);
	updateGeneratorStartStopState();

	return outputFunction;
}

void SwitchableOutputMonitor::removeGeneratorStartStopFunction(SwitchableOutputFunction *outputFunction)
{
	auto *generatorFunction = qobject_cast<SwitchableOutputFunctionGeneratorStartStop *>(outputFunction);
	if (!generatorFunction || !mGeneratorStartStopFunctions.remove(generatorFunction))
		return;

	if (mGeneratorStartStopFunctions.isEmpty()) {
		// startstop0 will go away, start fresh when it is needed again.
		mGeneratorStartStopRequest = QVariant();
		emit generatorStartStopNeededChanged(false);
	}
	updateGeneratorStartStopState();
}

void SwitchableOutputMonitor::onGeneratorStartStopRequested(bool on)
{
	mGeneratorStartStopRequest = on;
	for (SwitchableOutputFunctionGeneratorStartStop *outputFunction : mGeneratorStartStopFunctions)
		outputFunction->setOutput(on);
}

// The proxy is on when any of the outputs is on, invalid when there are no outputs with a known state.
void SwitchableOutputMonitor::updateGeneratorStartStopState()
{
	QVariant state;
	for (SwitchableOutputFunctionGeneratorStartStop *outputFunction : mGeneratorStartStopFunctions) {
		QVariant output = outputFunction->getOutput();
		if (!output.isValid())
			continue;
		if (output.toInt() == 1) {
			state = 1;
			break;
		}
		state = 0;
	}
	mGeneratorStartStopState->produceValue(state);
}

SwitchableOutputFunction::SwitchableOutputFunction(SwitchableOutputFunctionType type, VeQItem *channelItem, QObject *parent) :
		QObject(parent),
		mType(type),
		mStateItem(channelItem ? channelItem->itemGet("State") : nullptr)
{
}

void SwitchableOutputFunction::setOutput(bool on)
{
	if (mStateItem)
		mStateItem->setValue(on ? 1 : 0);
}

SwitchableOutputFunctionAlarm::SwitchableOutputFunctionAlarm(Notifications *notifications,
		VeQItem *channelItem, QObject *parent) :
		SwitchableOutputFunction(SwitchableOutputFunctionType::Alarm, channelItem, parent),
		mNotifications(notifications)
{
	if (mNotifications) {
		connect(mNotifications, SIGNAL(alarmChanged()), this, SLOT(alarmChanged()));
		setOutput(mNotifications->isAlarm());
	}
}

void SwitchableOutputFunctionAlarm::alarmChanged()
{
	if (mNotifications)
		setOutput(mNotifications->isAlarm());
}

SwitchableOutputFunctionGeneratorStartStop::SwitchableOutputFunctionGeneratorStartStop(VeQItem *channelItem, QObject *parent) :
		SwitchableOutputFunction(SwitchableOutputFunctionType::GeneratorStartStop, channelItem, parent)
{
	if (stateItem())
		stateItem()->getValueAndChanges(this, SLOT(onStateChanged(QVariant)));
}

QVariant SwitchableOutputFunctionGeneratorStartStop::getOutput() const
{
	return stateItem() ? stateItem()->getValue() : QVariant();
}

void SwitchableOutputFunctionGeneratorStartStop::onStateChanged(QVariant value)
{
	Q_UNUSED(value);
	emit outputChanged();
}
