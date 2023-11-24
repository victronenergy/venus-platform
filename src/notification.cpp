#include "notification.h"

Notification::Notification(Type type, const QString &devicename,
							const QString &value, const QString &serviceName,
							const QString &alarmTrigger, const QVariant &alarmValue,
							VeQItem *rootItem, int index, NotificationDescriptions *descriptions, QObject *parent) :
	QObject(parent),
	mAcknowledged(false), mActive(true), mType(type), mDevicename(devicename),
	mDateTime(QDateTime::currentDateTime()), mValue(value),
	mIndex(index),
	mDescriptions(descriptions)
{
	mItem = rootItem->itemGetOrCreate(QString::number(index));
	mItem->itemGetOrCreateAndProduce("Service", serviceName);
	mItem->itemGetOrCreateAndProduce("DateTime", mDateTime.toTime_t());
	mItem->itemGetOrCreateAndProduce("Trigger", alarmTrigger);
	mItem->itemGetOrCreateAndProduce("AlarmValue", alarmValue);
	mItem->itemGetOrCreateAndProduce("DeviceName", mDevicename);
	mItem->itemGetOrCreateAndProduce("Value", mValue);
	mItem->itemGetOrCreateAndProduce("Type", type);
	mItem->itemGetOrCreateAndProduce("Active", 1);
	mItem->itemGetOrCreateAndProduce("Acknowledged", (int)mAcknowledged);
	mItem->itemGetOrCreateAndProduce("Description", descriptions->getDescription(serviceName, alarmTrigger, alarmValue));
}

void Notification::updateDescription()
{

	auto desc = mItem->itemGet("Description");
	QString svc = mItem->itemGet("Service")->getValue().toString();
	QString trigger = mItem->itemGet("Trigger")->getValue().toString();
	auto val = mItem->itemGet("AlarmValue")->getValue();

	// Set updated description
	desc->setValue(mDescriptions->getDescription(svc, trigger, val));
}

