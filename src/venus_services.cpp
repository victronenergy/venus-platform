#include <veutil/qt/ve_qitem.hpp>

#include "venus_service.hpp"
#include "venus_services.hpp"

void VenusServices::initialScan()
{
	connect(mQItemServices, SIGNAL(childAdded(VeQItem*)), SLOT(onServiceAdded(VeQItem*)));

	for (VeQItem *service: mQItemServices->itemChildren())
		onServiceAdded(service);
}

void VenusServices::onServiceAdded(VeQItem *serviceItem)
{
	VenusService *service;

	service = VenusService::createInstance(serviceItem);
	if (service == nullptr)
		return;
	service->setParent(this);
	connect(service, SIGNAL(serviceDestroyed()), SLOT(onServiceDestoyed()));
	connect(service, SIGNAL(initialized()), SLOT(onServiceInitialized()));
}

void VenusServices::onServiceDestoyed()
{
	VenusService *service = qobject_cast<VenusService *>(sender());
	mServices.remove(mServices.key(service));
}

void VenusServices::onConnectedChanged(VenusService *service)
{
	if (service->getConnected())
		emit connected(service);
	else
		emit disconnected(service);
}

void VenusServices::onServiceInitialized()
{
	VenusService *service = static_cast<VenusService *>(sender());

	QString name = service->getName();
	mServices.insert(name, service);
	emit found(service);
	onConnectedChanged(service);
}
