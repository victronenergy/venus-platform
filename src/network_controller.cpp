#include <QJsonDocument>

#include "network_controller.hpp"

int VeQItemJson::setValue(const QVariant &value)
{
	if (!value.isValid())
		return VeQItemAction::setValue(value);

	QJsonDocument doc = QJsonDocument::fromJson(value.toByteArray());
	if (!doc.isNull())
		emit jsonParsed(doc);

	return VeQItemAction::setValue(value);
}

int VeQItemScan::setValue(const QVariant &value)
{
	CmTechnology *tech = mConnman->getTechnology("wifi");
	if (tech)
		tech->scan();

	return VeQItemAction::setValue(value);
}

NetworkController::NetworkController(VeQItem *parentItem, QObject *parent)
	: QObject{parent}, mWifiService(nullptr), mEthernetService(nullptr)
{
	mConnman = CmManager::instance(this);
	VeQItemJson *parser = new VeQItemJson();

	mItem = parentItem->itemGetOrCreate("Network");
	mItem->itemAddChild("Services", new VeQItemQuantity());
	mItem->itemAddChild("SetValue", parser);

	VeQItem *ethernet = mItem->itemGetOrCreate("Ethernet");
	ethernet->itemAddChild("LinkLocalIpAddress", new VeQItemQuantity());

	CmTechnology *tech = mConnman->getTechnology("wifi");
	QStringList services;
	if (tech && tech->powered()) {
		VeQItem *wifi = mItem->itemGetOrCreate("Wifi");
		wifi->itemAddChild("Scan", new VeQItemScan(mConnman));
		wifi->itemAddChild("State", new VeQItemQuantity(-1, "", "Disconnected"));
		wifi->itemAddChild("SignalStrength", new VeQItemQuantity());

		services = mConnman->getServiceList("wifi");
		for (auto &s: services) {
			CmService *service = mConnman->getService(s);
			if (service && service->favorite()) {
				mWifiService = service;
				connectServiceSignals(mWifiService);
				updateWifiState();
				break;
			}
		}
	}

	services = mConnman->getServiceList("ethernet");
	if (!services.empty()) {
		mEthernetService = mConnman->getService(services[0]);
		if (mEthernetService) {
			connectServiceSignals(mEthernetService);
			updateLinkLocal();
		}
	}

	connect(mConnman, SIGNAL(serviceListChanged()), this, SLOT(buildServicesList()));
	connect(mConnman, SIGNAL(serviceChanged(QString,QVariantMap)), this, SLOT(buildServicesList()));
	connect(mConnman, SIGNAL(serviceAdded(QString)), this, SLOT(onServiceAdded(QString)));
	connect(mConnman, SIGNAL(serviceRemoved(QString)), this, SLOT(onServiceRemoved(QString)));
	connect(parser, SIGNAL(jsonParsed(QJsonDocument)), this, SLOT(handleCommand(QJsonDocument)));

	buildServicesList();
}

void NetworkController::connectServiceSignals(CmService *service)
{
	connect(service, SIGNAL(ipv4Changed()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(ipv4ConfigChanged()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(nameserversChanged()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(nameserversConfigChanged()), this, SLOT(buildServicesList()));

	if (service->type() == "wifi") {
		connect(service, SIGNAL(stateChanged()), this, SLOT(updateWifiState()));
		connect(service, SIGNAL(strengthChanged()), this, SLOT(updateWifiSignalStrength()));
	} else {
		connect(mEthernetService, SIGNAL(stateChanged()), this, SLOT(updateLinkLocal()));
	}
}

void NetworkController::buildServicesList()
{
	QVariantMap obj;

	// Loop over technologies
	QStringList technologies = mConnman->getTechnologyList();
	bool hasBluetooth = technologies.contains("bluetooth");
	mItem->itemGetOrCreateAndProduce("HasBluetoothSupport", hasBluetooth);
	technologies.removeAll("bluetooth");
	technologies.removeAll("p2p");

	for (auto &t: technologies) {
		// Loop over services within technology
		QStringList services = mConnman->getServiceList(t);

		QVariantMap list;
		for (auto &s: services) {
			CmService *service = mConnman->getService(s);
			if (service) {
				QVariantMap ipv4 = service->ipv4Config()["Method"] == "dhcp" ? service->ipv4() : service->ipv4Config();
				QStringList dns = service->nameservers();
				QStringList security = service->security();
				QVariantMap properties = service->properties();
				QVariantMap ethernet = service->ethernet();

				QVariantMap m;
				m.insert("Service", s);
				m.insert("State", properties["State"].toString());
				if (properties.contains("Strength"))
					m.insert("Strength", properties["Strength"].toUInt());
				if (!security.empty())
					m.insert("Secured", QString(security.contains("none") ? "no" : "yes"));
				if (t == "wifi")
					m.insert("Favorite", QString(service->favorite() ? "yes" : "no"));
				m.insert("Address", ipv4["Address"].toString());
				m.insert("Gateway", ipv4["Gateway"].toString());
				m.insert("Method", ipv4["Method"].toString());
				m.insert("Netmask", ipv4["Netmask"].toString());
				m.insert("Mac", ethernet["Address"].toString());
				m.insert("Nameservers", dns);

				list.insert(properties["Name"].toString(), m);

				// If there is a wifi service which is not the current service but has state == "ready",
				// then set it as the active wifi service.
				// This may happen when the current connection fails (for some reason) and
				// connman connects to another favorite network automatically.
				if ((t == "wifi") && (properties["State"] == "ready") && (mWifiService != service)) {
					mWifiService = service;
					connectServiceSignals(service);
					updateWifiState();
				}
			}
		}
		obj.insert(t, list);
	}
	QJsonDocument doc = QJsonDocument::fromVariant(obj);
	QString json = doc.toJson(QJsonDocument::Compact);
	mItem->itemGetOrCreateAndProduce("Services", json);
}

void NetworkController::handleCommand(const QJsonDocument &doc)
{
	CmService *service = nullptr;
	QVariantMap map = doc.toVariant().toMap();

	if (map.contains("Service"))
		service = mConnman->getService(doc["Service"].toString());

	if (map.contains("Agent")) {
		if (doc["Agent"] == "on")
			mAgent = mConnman->registerAgent("/com/victronenergy/ccgx");
		else if (map["Agent"] == "off")
			mConnman->unRegisterAgent("/com/victronenergy/ccgx");
	} else if (map.contains("Action")) {
		if (map["Action"] == "connect") {
			if (mWifiService && mWifiService != service)
				mWifiService->disconnect();
			mAgent->passphrase(doc["Passphrase"].toString());
			mWifiService = service;

			if (mWifiService) {
				mWifiService->connect();
				connectServiceSignals(mWifiService);
			}
		} else if (map["Action"] == "disconnect") {
			if (mWifiService) {
				mWifiService->disconnect();
				mWifiService = nullptr;
			}
		} else if (map["Action"] == "remove") {
			if (service) {
				service->remove();
				if (mWifiService == service)
					mWifiService = nullptr;
			}
		}
	} else if (service) {
		// Check for manual configuration parameters
		setServiceProperties(service, map);
	}
}

void NetworkController::onServiceAdded(const QString &path)
{
	if (!mEthernetService) {
		CmService *s = mConnman->getService(path);
		if (s && s->type() == "ethernet") {
			mEthernetService = s;
			connectServiceSignals(mEthernetService);
			updateLinkLocal();
		}
	}
}

void NetworkController::onServiceRemoved(const QString &path)
{
	if (mEthernetService && path == mEthernetService->path()) {
		mEthernetService = nullptr;
		updateLinkLocal();
	} else if (mWifiService && path == mWifiService->path()) {
		mWifiService = nullptr;
		updateWifiState();
	}
}

void NetworkController::setServiceProperties(CmService *service, const QVariantMap &data)
{
	if (!service)
		return;

	if (data.contains("Method"))
		setIpConfiguration(service, data["Method"]);
	if (data.contains("Address"))
		setIpv4Property(service, "Address", data["Address"]);
	if (data.contains("Gateway"))
		setIpv4Property(service, "Gateway", data["Gateway"]);
	if (data.contains("Netmask"))
		setIpv4Property(service, "Netmask", data["Netmask"]);
	if (data.contains("Nameserver"))
		setDnsServer(service, data["Nameserver"]);
}

QString NetworkController::getLinkLocalAddr()
{
	QProcess proc;

	proc.start("ip -o -4 addr sh dev ll-eth0 scope link");
	proc.waitForFinished();

	QString out(proc.readAllStandardOutput());
	QString ip = out.simplified().section(' ', 3, 3);

	return ip.section('/', 0, 0);
}

void NetworkController::updateLinkLocal()
{
	mItem->itemGetOrCreateAndProduce("Ethernet/LinkLocalIpAddress", getLinkLocalAddr());
}

void NetworkController::updateWifiState()
{
	QString state = mWifiService?mWifiService->state() : "idle";
	mItem->itemGetOrCreateAndProduce("Wifi/State", state);
	if (state == "ready" || state == "online" || state == "association" || state == "configuration") {
		updateWifiSignalStrength();
	}
}

void NetworkController::updateWifiSignalStrength()
{
	mItem->itemGetOrCreateAndProduce("Wifi/SignalStrength", mWifiService ? mWifiService->strength() : 0);
}

void NetworkController::setIpConfiguration(CmService *service, QVariant var)
{
	if (!service || !var.isValid())
		return;

	QVariantMap ipv4Config = service->ipv4();
	QStringList nameServersConfig = service->nameservers();
	QVariant oldMethod = ipv4Config["Method"];

	QString method = var.toString();

	if (method == oldMethod)
		return;

	if (method == "dhcp") {
		ipv4Config["Address"] = "255.255.255.255";
		service->ipv4Config(ipv4Config);
		ipv4Config["Method"] = "dhcp";
		nameServersConfig.clear();
	} else if (method == "manual") {
		ipv4Config["Method"] = "manual";
		QString ipAddress = service->checkIpAddress(ipv4Config["Address"].toString());
		/*
		 * Make sure the ip settings are valid when switching to "manual"
		 * When the ip settings are not valid, connman will continuously disconnect
		 * and reconnect the service and it is impossible to set the ip-address.
		 */
		if (ipAddress == "") {
			ipv4Config["Address"] = "169.254.1.2";
			ipv4Config["Netmask"] = "255.255.255.0";
			ipv4Config["Gateway"] = "169.254.1.1";
		}
	}

	service->nameserversConfig(nameServersConfig);
	service->ipv4Config(ipv4Config);
}

void NetworkController::setIpv4Property(CmService *service, QString name, QVariant var)
{
	// Not allowed to write properties when ip configuration is not manual.
	if (!service || !var.isValid() || service->ipv4Config()["Method"] != "manual")
		return;
	QVariantMap ipv4Config = service->ipv4();

	ipv4Config[name] = var.toString();
	service->ipv4Config(ipv4Config);
}

void NetworkController::setDnsServer(CmService *service, QVariant var)
{
	if (!service)
		return;

	if (service->ipv4Config()["Method"] == "manual") {
		QStringList nameServersConfig = service->nameserversConfig();
		nameServersConfig.clear();
		nameServersConfig.append(var.toString());
		service->nameserversConfig(nameServersConfig);
	}
}
