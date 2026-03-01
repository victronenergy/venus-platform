#include "relay.hpp"

Relay::Relay(QString relayPath, Notifications *notifications, QObject *parent) :
	QObject(parent),
	mNotifications(notifications),
	mIntendedState(false)
{
	mPolaritySetting = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Relay/Polarity");
	mPolaritySetting->getValueAndChanges(this, &Relay::updateRelayState, VeQItem::DoFetch, Qt::QueuedConnection);

	mRelayFunction = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Relay/Function");
	mRelayFunction->getValueAndChanges(this, &Relay::updateRelayState, VeQItem::DoFetch, Qt::QueuedConnection);

	mRelayState = VeQItems::getRoot()->itemGetOrCreate(relayPath);
	mRelayState->getValueAndChanges(this, &Relay::updateRelayState, VeQItem::DoFetch, Qt::QueuedConnection);

	connect(notifications, &Notifications::alarmChanged, this, &Relay::alarmChanged);
	setRelayOn(notifications->isAlarm());
}

void Relay::updateRelayState()
{
	if (!mPolaritySetting->getValue().isValid() || !mRelayFunction->getValue().isValid())
		return;

	// only control the relay in alarm notification mode
	if (mRelayFunction->getValue() != 0)
		return;

	/* Set relay but allow software inversion */
	setHwState(mIntendedState ^ mPolaritySetting->getValue().toInt());
}

bool Relay::getRelayOn() const
{
	return mIntendedState;
}

void Relay::setRelayOn(bool on)
{
	if (mIntendedState == on)
		return;

	mIntendedState = on;
	emit relayOnChanged();
	updateRelayState();
}

/*
 * set the actual relay state
 *   true for closed / activated
 *   false for open / powered off state
 */
void Relay::setHwState(bool closed)
{
	if (!mRelayState->getValue().isValid())
		return;
	mRelayState->setValue(closed ? 1 : 0);
}

void Relay::alarmChanged()
{
	setRelayOn(mNotifications->isAlarm());
}
