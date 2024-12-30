#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem.hpp>

class AlarmMonitor;

class Notification : public QObject
{
	Q_OBJECT

public:
	enum Type {
		WARNING,
		ALARM,
		NOTIFICATION
	};

	explicit Notification(Type type, const QString &devicename, const QString &description,
				const QString &value, const QString &serviceName,
				const QString &alarmTrigger, const QVariant &alarmValue,
				VeQItem *rootItem, int index, QObject *parent = 0);

	bool isAcknowledged() const { return mAcknowledged; }
	void setAcknowledged(bool acknowledged);

	/*
	 * By default notifications are active. Alarms condition can disappear
	 * however while the alarm has not been acknowledged yet. In that case
	 * active becomes false.
	 */
	bool isActive() const { return mActiveItem->getLocalValue().toBool(); }
	void setActive(bool active);

	Notification::Type type() const { return static_cast<Type>(mTypeItem->getLocalValue().toInt()); }
	int getIndex() const { return mIndexItem->id().toInt(); }

signals:
	void acknowledgedChanged(Notification *notification);
	void activeChanged(Notification *notification);

private slots:
	void acknowledgedItemChanged(QVariant var) { setAcknowledged(var.toBool()); }

private:
	VeQItem *mAcknowledgedItem;
	VeQItem *mActiveItem;
	VeQItem *mIndexItem;
	VeQItem *mTypeItem;
	bool mAcknowledged;
};
