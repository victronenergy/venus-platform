#include <veutil/qt/ve_qitem.hpp>

#include "buzzer.hpp"

Buzzer::Buzzer(const QString &buzzerPath, QObject *parent) :
	QObject(parent),
	mBuzzerToggle(false)
{
	mBuzzerItem = VeQItems::getRoot()->itemGetOrCreate(buzzerPath);
	connect(mBuzzerItem, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onItemStateChanged()));
	mBuzzerItem->getValue();
}

Buzzer::~Buzzer()
{
	setBuzzer(false);
}

bool Buzzer::isBeeping() const
{
	return mBuzzerItem->getState() == VeQItem::Requested ?
		mBuzzerToggle :
		mBuzzerItem->getValue().toBool();
}

void Buzzer::buzzerOn()
{
	//setBuzzer(true);
}

void Buzzer::buzzerOff()
{
	setBuzzer(false);
}

void Buzzer::onItemStateChanged()
{
	VeQItem::State state = mBuzzerItem->getState();
	if (state == VeQItem::Requested)
		return;
	if (state == VeQItem::Offline)
		return;
	mBuzzerItem->setValue(mBuzzerToggle);
}

void Buzzer::setBuzzer(bool on)
{
	qDebug() << "Buzzer::setBuzzer:" << on;
	mBuzzerToggle = on;
	mBuzzerItem->setValue(on);
}
