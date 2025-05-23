#include <QDateTime>

#include "notification.hpp"

Notification::Notification(Type type, const QString &devicename, const QString &description,
							const QString &value, const QString &serviceName,
							const QString &alarmTrigger, const QVariant &alarmValue,
							VeQItem *rootItem, int index, QObject *parent) :
	QObject(parent)
{
	mIndexItem = rootItem->itemGetOrCreate(QString::number(index));
	mIndexItem->itemGetOrCreateAndProduce("Service", serviceName);
	mIndexItem->itemGetOrCreateAndProduce("DateTime", QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);
	mIndexItem->itemGetOrCreateAndProduce("Trigger", alarmTrigger);
	mIndexItem->itemGetOrCreateAndProduce("AlarmValue", alarmValue);
	mIndexItem->itemGetOrCreateAndProduce("DeviceName", devicename);
	mIndexItem->itemGetOrCreateAndProduce("Value", value);
	mTypeItem = mIndexItem->itemGetOrCreateAndProduce("Type", type);
	mActiveItem = mIndexItem->itemGetOrCreateAndProduce("Active", QVariant::fromValue(true));
	mAcknowledgedItem = mIndexItem->itemGetOrCreateAndProduce("Acknowledged", QVariant::fromValue(false));
	mSilencedItem = mIndexItem->itemGetOrCreateAndProduce("Silenced", QVariant::fromValue(false));
	mIndexItem->itemGetOrCreateAndProduce("Description", description);

	mAcknowledgedItem->getValueAndChanges(this, SLOT(acknowledgedItemChanged(QVariant)));
}

void Notification::setAcknowledged(bool acknowledged)
{
	if (acknowledged != isAcknowledged()) {
		mAcknowledged = acknowledged;
		mAcknowledgedItem->produceValue(QVariant::fromValue(acknowledged));
		emit acknowledgedChanged(this);
	}
}

void Notification::setSilenced(bool silenced)
{
	if (silenced != isSilenced()) {
		mSilencedItem->produceValue(QVariant::fromValue(silenced));
		emit silencedChanged(this);
	}
}

void Notification::setActive(bool active)
{
	if (active != isActive()) {
		mActiveItem->produceValue(QVariant::fromValue(active));
		emit activeChanged(this);
	}
}
