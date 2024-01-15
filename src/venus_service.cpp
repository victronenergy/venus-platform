#include <QEvent>
#include <QStringList>

#include "venus_service.hpp"
#include "venus_services.hpp"

VenusService::VenusService(VeQItem *serviceItem, VenusServiceType serviceType, QObject *parent) :
	QObject(parent),
	mServiceType(serviceType),
	mServiceItem(serviceItem),
	mInitDone(false)
{
	// Note: to support service without a CustomName item at all, also the state changes need
	// to be connected to, since there won't be any valueChange if the CustomName is not supported
	// at all!
	connect(item("CustomName"), SIGNAL(stateChanged(VeQItem::State)), SLOT(updateDescription()));
	connect(item("ProductName"), SIGNAL(stateChanged(VeQItem::State)), SLOT(updateDescription()));
	item("ProductName")->getValueAndChanges(this, SLOT(updateDescription()), true, true);
	item("CustomName")->getValueAndChanges(this, SLOT(updateDescription()), true, true);
	connect(serviceItem, SIGNAL(stateChanged(VeQItem::State)), SIGNAL(connectedChanged()));
	connect(serviceItem, SIGNAL(stateChanged(VeQItem::State)), SLOT(updateDescription()));
}

VenusService::~VenusService()
{
	emit serviceDestroyed();
	mServiceItem->itemDelete();
}

void VenusService::updateDescription()
{
	VeQItem *productNameItem = item("ProductName");
	VeQItem *customNameItem = item("CustomName");

	if (customNameItem->getState() == VeQItem::Requested || productNameItem->getState() == VeQItem::Requested)
		return;

	QString customName = customNameItem->getValue().toString();

	// If a custom name is avaiable and set, use that as device description
	if (customNameItem->getState() == VeQItem::Synchronized && !customName.isEmpty()) {
		setDescription(customName);
	// Otherwise use the product name when valid
	} else if (mServiceItem->getState() == VeQItem::Synchronized &&
				(customNameItem->getState() == VeQItem::Offline || customName.isEmpty()) &&
				productNameItem->getState() == VeQItem::Synchronized) {
		setDescription(productNameItem->getValue().toString());
	// pending ..
	} else {
		return;
	}
}

void VenusService::setDescription(const QString &description)
{
	if (description == mDescription)
		return;

	mDescription = description;
	emit descriptionChanged();

	checkInitDone();
}

void VenusService::checkInitDone()
{
	if (mInitDone || mDescription.isEmpty())
		return;

	mInitDone = true;
	emit initialized();
}

VenusService *VenusService::createInstance(VeQItem *serviceItem)
{
	QString name = serviceItem->id();
	VenusServiceType type = venusServiceType(name);

	switch (type) {
	case VenusServiceType::UNKNOWN:
		return nullptr;
	case VenusServiceType::TANK:
		return new VenusTankService(serviceItem);
	default:
		return new VenusService(serviceItem, type);
	}
}

VenusTankService::VenusTankService(VeQItem *serviceItem, QObject *parent) :
	VenusService(serviceItem, VenusServiceType::TANK, parent)
{
	// MIND IT! since bulk init might be active here, the state can bounch a bit.
	connect(item("DeviceInstance"), SIGNAL(stateChanged(VeQItem::State)), SLOT(updateDescription()));
	connect(item("FluidType"), SIGNAL(stateChanged(VeQItem::State)), SLOT(updateDescription()));

	item("DeviceInstance")->getValueAndChanges(this, SLOT(updateDescription()), true, true);
	item("FluidType")->getValueAndChanges(this, SLOT(updateDescription()), true, true);
}

void VenusTankService::updateDescription()
{
	VeQItem *customNameItem = item("CustomName");
	QString customName = customNameItem->getValue().toString();

	// Mind it, not all tanks have a customName!!!
	if (customNameItem->getState() != VeQItem::Synchronized && customNameItem->getState() != VeQItem::Offline)
		return;

	// If a custom name is avaiable and set, used that as device description
	if (!customName.isEmpty()) {
		setDescription(customName);
		return;
	}

	VeQItem *vrmInstanceItem = item("DeviceInstance");
	VeQItem *typeItem = item("FluidType");

	if (vrmInstanceItem->getState() != VeQItem::Synchronized || typeItem->getState() != VeQItem::Synchronized)
		return;

	QString description = venusFluidTypeName(typeItem->getValue().toUInt()) + " " + tr("tank");
	description += " (" + QString::number(vrmInstanceItem->getValue().toInt()) + ")";
	setDescription(description);
}
