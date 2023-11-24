#include <src/relay.h>

Relay::Relay(QString relayPath, NotificationCenter *notificationCenter, QObject *parent) :
	QObject(parent),
	mNotificationCenter(notificationCenter),
	mIntendedState(false)
{
	mPolaritySetting = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Relay/Polarity");
	mPolaritySetting->getValueAndChanges(this, SLOT(updateRelayState()), true, true);

	mRelayFunction = VeQItems::getRoot()->itemGetOrCreate("dbus/com.victronenergy.settings/Settings/Relay/Function");
	mRelayFunction->getValueAndChanges(this, SLOT(updateRelayState()), true, true);

	mRelayState = VeQItems::getRoot()->itemGetOrCreate(relayPath);
	mRelayState->getValueAndChanges(this, SLOT(updateRelayState()), true, true);

	connect(notificationCenter, SIGNAL(alarmChanged()), this, SLOT(alarmChanged()));
	setRelayOn(notificationCenter->getAlarm());
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
	setRelayOn(mNotificationCenter->getAlarm());
}
