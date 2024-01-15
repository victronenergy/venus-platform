#pragma once

#include <QObject>
#include <QString>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/venus_types.hpp>

class VenusService : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(VenusService)

public:
	VenusService(VeQItem *serviceItem, VenusServiceType serviceType, QObject *parent = 0);
	~VenusService();
	static VenusService *createInstance(VeQItem *serviceItem);

	QString getName() const { return mServiceItem->id(); }
	QString getDescription() const { return mDescription; }
	VenusServiceType getType() const { return mServiceType; }
	bool getConnected() { return mServiceItem->getState() != VeQItem::Offline; }

	inline VeQItem *item(QString const &id = QString()) {
		if (id.isEmpty())
			return mServiceItem;
		return mServiceItem->itemGetOrCreate(id);
	}

signals:
	void connectedChanged();
	void descriptionChanged();
	void initialized();
	void serviceDestroyed();

private slots:
	virtual void updateDescription();

protected:
	void setDescription(const QString &description);
	virtual void checkInitDone();

private:
	VenusServiceType mServiceType;
	QString mDescription;
	VeQItem *mServiceItem;
	bool mInitDone;
};

class VenusTankService : public VenusService
{
	Q_OBJECT
	Q_DISABLE_COPY(VenusTankService)

public:
	VenusTankService(VeQItem *serviceItem, QObject *parent = 0);

private slots:
	void updateDescription() override;
};
