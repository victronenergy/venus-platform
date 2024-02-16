#include "network_controller.h"

int VeQItemJson::setValue(const QVariant &value)
{
	if(!value.isValid())
		return VeQItemAction::setValue(value);

	QVariantMap data = parseJson(value.toString());

	if(!data.empty())
		emit jsonParsed(data);

	return VeQItemAction::setValue(value);
}

QVariantMap VeQItemJson::parseJson(const QString &json)
{
	// Only unnested JSON with key:value pairs supported.
	// Simple parse algorithm:
	// Start at opening brace
	// Find key by searching for quotes
	// Find colon after closing quote
	// Extract value --> everything after ':' to ',' or '}'
	// Parse value

	QVariantMap data;
	bool esc = false;
	bool value = false;
	bool quote = false;
	bool commaOrBrace = false;
	QString str = "";
	QString keyStr;

	if((json.size() < 2) || (json[0] != '{') || (json[json.size() - 1] != '}'))
		return data;

	for (const QChar &c:json) {
		if (c == '\\') {
			esc = true;
			continue;
		}
		else if (!esc && c == '"') {
			quote = !quote;
			continue;
		}
		if(!quote) {
			if (c == ':') {
				keyStr = str;
				str = "";
				value = true;
				continue;
			}
			else if (c == ',' || c == '}')
				commaOrBrace = true;
		}
		if (commaOrBrace) {
			bool ok;
			QVariant val;
			val = str.toInt(&ok);
			if(!ok)
				val = str.toDouble(&ok);
			if(!ok)
				val = str;

			data.insert(keyStr, val);
			str = "";
			commaOrBrace = false;
		}
		else if(quote || (c != ' ' && value))
			str += c;
		if (esc)
			esc = false;
	}
	return data;
}

int VeQItemScan::setValue(const QVariant &value)
{
	CmTechnology *tech = mConnman->getTechnology("wifi");
	if(tech)
		tech->scan();

	return VeQItemAction::setValue(value);
}

NetworkController::NetworkController(VeQItem *parentItem, QObject *parent)
	: QObject{parent}, wifiService(nullptr)
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
	if(tech && tech->powered()) {
		VeQItem *wifi = mItem->itemGetOrCreate("Wifi");
		wifi->itemAddChild("Scan", new VeQItemScan(mConnman));
		wifi->itemAddChild("State", new VeQItemQuantity(-1, "", "Disconnected"));
		wifi->itemAddChild("SignalStrength", new VeQItemQuantity(2));

		services = mConnman->getServiceList("wifi");
		for(auto &s:services) {
			CmService *service = mConnman->getService(s);
			if(service && service->favorite()) {
				wifiService = service;
				connectServiceSignals(wifiService);
				updateWifiState();
				break;
			}
		}
	}

	services = mConnman->getServiceList("ethernet");
	ethernetService = mConnman->getService(services[0]);
	if(ethernetService) {
		connectServiceSignals(ethernetService);
		updateLinkLocal();
	}

	connect(mConnman, SIGNAL(serviceListChanged()), this, SLOT(buildServicesList()));
	connect(mConnman, SIGNAL(serviceChanged()), this, SLOT(buildServicesList()));
	connect(mConnman, SIGNAL(serviceAdded(QString)), this, SLOT(onServiceAdded(QString)));
	connect(mConnman, SIGNAL(serviceRemoved(QString)), this, SLOT(onServiceRemoved(QString)));
	connect(parser, SIGNAL(jsonParsed(QVariantMap)), this, SLOT(handleCommand(QVariantMap)));

	buildServicesList();
}

void NetworkController::connectServiceSignals(CmService *service)
{
	connect(service, SIGNAL(stateChanged()), this, SLOT(updateWifiState()));
	connect(service, SIGNAL(strengthChanged()), this, SLOT(updateWifiSignalStrength()));
	connect(service, SIGNAL(ipv4Changed()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(ipv4ConfigChanged()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(nameserversChanged()), this, SLOT(buildServicesList()));
	connect(service, SIGNAL(nameserversConfigChanged()), this, SLOT(buildServicesList()));
}

void NetworkController::buildServicesList()
{
	QString json = "{";

	// Loop over technologies
	QStringList technologies = mConnman->getTechnologyList();
	technologies.removeAll("bluetooth");
	technologies.removeAll("p2p");
	for(auto &t:technologies) {
		// Loop over services within technology
		QStringList services = mConnman->getServiceList(t);
		json += "\"" + t + "\"";
		json += ":{";
		for(auto &s:services) {
			CmService *service = mConnman->getService(s);
			if(service) {
				QVariantMap ipv4 = service->ipv4Config()["Method"] == "dhcp" ? service->ipv4() : service->ipv4Config();
				QStringList dns = service->nameservers();
				QStringList security = service->security();
				QVariantMap properties = service->properties();
				QVariantMap ethernet = service->ethernet();

				json += "\"" + properties["Name"].toString() + "\"";
				json += ":{";
				json += "\"Service\":\"" + s + "\",";
				json += "\"State\":\"" + getState(properties["State"].toString()) + "\",";
				if(properties.contains("Strength"))
					json += "\"Strength\":\"" + QString::number(properties["Strength"].toUInt()) + "\",";
				if(!security.empty())
					json += "\"Secured\":\"" + QString(security.contains("none") ? "no" : "yes") + "\",";
				if(t == "wifi")
					json += "\"Favorite\":\"" + QString(service->favorite() ? "yes" : "no") + "\",";
				json += "\"Address\":\"" + ipv4["Address"].toString() + "\",";
				json += "\"Gateway\":\"" + ipv4["Gateway"].toString() + "\",";
				json += "\"Method\":\"" + ipv4["Method"].toString() + "\",";
				json += "\"Netmask\":\"" + ipv4["Netmask"].toString() + "\",";
				json += "\"Mac\":\"" + ethernet["Address"].toString() + "\",";
				json += "\"Nameservers\":[";
				for(auto &d:dns) {
					json += "\"" + d + "\",";
				}
				if(dns.size())
					json.remove(json.size()-1, 1);
				json += "]},";
			}
		}
		if(services.size())
			json.remove(json.size()-1, 1);
		json += "},";
	}

	json.remove(json.size()-1, 1);
	json+= '}';
	mItem->itemGetOrCreateAndProduce("Services", json);
}

QString NetworkController::getState(const QString &state)
{
	if(state == "idle")
		return "Disconnected";
	if(state == "failure")
		return "Failure";
	if(state == "association")
		return "Connecting";
	if(state == "configuration")
		return "Retrieving IP address";
	if(state == "ready")
		return "Connected";
	if(state == "disconnect")
		return "Disconnect";
	if(state == "online")
		return "Connected";
	else
		return state;
}

void NetworkController::handleCommand(const QVariantMap &data)
{
	CmService *service = nullptr;

	if(data.contains("Service")) {
		service = mConnman->getService(data["Service"].toString());
	}
	if (data.contains("Action")) {
		if (data["Action"] == "connect") {
			if(wifiService && wifiService != service)
				wifiService->disconnect();
			mAgent = mConnman->registerAgent("/com/victronenergy/ccgx");
			mAgent->passphrase(data["Passphrase"].toString());
			wifiService = service;

			if(wifiService) {
				wifiService->connect();
				connectServiceSignals(wifiService);
			}
		}
		else if (data["Action"] == "disconnect") {
			if(wifiService) {
				wifiService->disconnect();
				wifiService = nullptr;
			}
		}
		else if (data["Action"] == "remove") {
			if(service) {
				service->remove();
				if(wifiService == service)
					wifiService = nullptr;
			}
		}
	}
	else if (service) {
		// Check for manual configuration parameters
		setServiceProperties(service, data);
	}
}

void NetworkController::onServiceAdded(const QString &path)
{
	if (!ethernetService) {
		CmService *s = mConnman->getService(path);
		if(s && s->type() == "ethernet") {
			ethernetService = s;
			connect(ethernetService, SIGNAL(stateChanged()), this, SLOT(updateLinkLocal()));
			connect(ethernetService, SIGNAL(stateChanged()), this, SLOT(buildServicesList()));
			connect(ethernetService, SIGNAL(ipv4Changed()), this, SLOT(buildServicesList()));
			updateLinkLocal();
		}
	}
}

void NetworkController::onServiceRemoved(const QString &path)
{
	if (ethernetService && path == ethernetService->path()) {
		ethernetService = nullptr;
		updateLinkLocal();
	}
	else if (wifiService && path == wifiService->path()) {
		wifiService = nullptr;
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
	QString state = wifiService?wifiService->state() : "Disconnected";
	mItem->itemGetOrCreateAndProduce("Wifi/State", getState(state));
	if (state == "ready" || state == "online" || state == "association" || state == "configuration") {
		updateWifiSignalStrength();
	}
	if (state == "ready" || state == "online") {
		mConnman->unRegisterAgent("/com/victronenergy/ccgx");
	}
}

void NetworkController::updateWifiSignalStrength()
{
	mItem->itemGetOrCreateAndProduce("Wifi/SignalStrength", wifiService ? wifiService->strength() : 0);
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

	if(method == "dhcp") {
		ipv4Config["Address"] = "255.255.255.255";
		service->ipv4Config(ipv4Config);
		ipv4Config["Method"] = "dhcp";
		nameServersConfig.clear();
	}
	else if(method == "manual") {
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
