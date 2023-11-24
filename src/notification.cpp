#include "notification.h"

Notification::Notification(Type type, const QString &devicename,
							const QString &value, const QString &serviceName,
							const QString &alarmTrigger, const QVariant &alarmValue,
							VeQItem *rootItem, int index, QObject *parent) :
	QObject(parent),
	mAcknowledged(false), mActive(true), mType(type), mDevicename(devicename),
	mDateTime(QDateTime::currentDateTime()), mValue(value),
	mIndex(index)
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
}

