#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QObject>
#include <QDateTime>
#include <veutil/qt/ve_qitem.hpp>

class Notification : public QObject
{
	Q_OBJECT

public:
	enum Type {
		WARNING,
		ALARM,
		NOTIFICATION
	};

	Notification(Type type, const QString &devicename,
					const QString &value, const QString &serviceName,
					const QString &alarmTrigger, const QVariant &alarmValue,
					VeQItem *rootItem, int index, QObject *parent = 0);

	bool acknowledged() const { return mAcknowledged; }

	void setAcknowledged(const bool acknowledged) {
		if (acknowledged != mAcknowledged) {
			mAcknowledged = acknowledged;
			emit acknowledgedChanged(this);
			auto ackItem = mItem->itemGet("Acknowledged");
			ackItem->setValue((int)acknowledged);
		}
	}

	/*
	 * By default notifications are active. Alarms condition can disappear
	 * however while the alarm has not been acknowledged yet. In that case
	 * active becomes false.
	 */
	bool active() const { return mActive; }

	void setActive(const bool active) {
		if (active != mActive) {
			mActive = active;
			emit activeChanged(this);
			auto activeItem = mItem->itemGetOrCreate("Active");
			activeItem->setValue((int)active);
		}
	}

	Notification::Type type() { return mType; }

	void setType(Type type) {
		if (type != mType) {
			mType = type;
			emit typeChanged(this);
			auto typeItem = mItem->itemGet("Type");
			typeItem->setValue(type);
		}
	}

	QString serviceName() const { return mDevicename; }
	QDateTime dateTime() const { return mDateTime; }

	QString value() const { return mValue; }

	void setValue(const QString &value) {
		if (value != mValue) {
			mValue = value;
			emit valueChanged(this);
			auto valItem = mItem->itemGet("Value");
			valItem->setValue(value);
		}
	}

	VeQItem* getItem() { return mItem; }
	int getIndex() { return mIndex; }

signals:
	void acknowledgedChanged(Notification *notification);
	void activeChanged(Notification *notification);
	void typeChanged(Notification *notification);
	void servicenameChanged(Notification *notification);
	void dateTimeChanged(Notification *notification);
	void descriptionChanged(Notification *notification);
	void valueChanged(Notification *notification);

private:
	bool mAcknowledged;
	bool mActive;
	Type mType;
	QString mDevicename;
	QDateTime mDateTime;
	QString mValue;
	int mIndex;
	VeQItem *mItem;
};

#endif
