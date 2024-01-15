#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem.hpp>
#include "venus_service.hpp"

class VenusServices : public QObject
{
	Q_OBJECT

public:
	VenusServices(VeQItem *services, QObject *parent = 0) :
		QObject(parent),
		mQItemServices(services)
	{
	}

	void initialScan();

signals:
	void connected(VenusService *service);
	void disconnected(VenusService *service);

	// note: found is called once, while connected / disconnected can
	// occur multiple times. The service is only announced after the
	// basic properties, like its description are obtained.
	void found(VenusService *service);

private slots:
	void onConnectedChanged(VenusService *service);
	void onServiceAdded(VeQItem *serviceItem);
	void onServiceDestoyed();
	void onServiceInitialized();

private:
	QHash<QString, VenusService *> mServices;
	VeQItem *mQItemServices;
};
