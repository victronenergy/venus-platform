#ifndef DBUS_SERVICE_H
#define DBUS_SERVICE_H

#include <QObject>
#include <QString>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/dbus_service_type.h>


class DBusService : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(DBusService)

public:

	using ServiceType = DBusServiceType::ServiceType;

	DBusService(VeQItem *serviceItem, ServiceType serviceType, QObject *parent = 0);
	~DBusService();

	QString getName() const
	{
		return mServiceItem->id();
	}

	QString getDescription() const {return mDescription;}

	static DBusService *createInstance(VeQItem *serviceItem);

	DBusServiceType::ServiceType getType() const {return mServiceType; }

	Q_INVOKABLE QString path(QString str) {
		return getName() + str;
	}
	inline VeQItem *item(const QString &id = QString()) {
		if (id.isEmpty())
			return mServiceItem;
		return mServiceItem->itemGetOrCreate(id);
	}

	bool getConnected() { return mServiceItem->getState() != VeQItem::Offline; }

signals:
	void descriptionChanged();
	void connectedChanged();
	void serviceDestroyed();
	void initialized();

private slots:
	virtual void updateDescription();

protected:
	void setDescription(const QString &description);
	virtual void checkInitDone();

private:
	DBusServiceType::ServiceType  mServiceType;
	QString mDescription;
	VeQItem *mServiceItem;
	bool mInitDone;
};

class DBusTankService : public DBusService
{
	Q_OBJECT
	Q_DISABLE_COPY(DBusTankService)

public:
	DBusTankService(VeQItem *serviceItem, QObject *parent = 0);

	void updateDescription() override;

private:
	static const std::vector<QString> &knownFluidTypes();
	static const QString fluidTypeName(unsigned int type);
};

Q_DECLARE_METATYPE(DBusService *)

#endif
