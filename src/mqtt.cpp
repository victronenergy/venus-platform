#include <veutil/qt/daemontools_service.hpp>

#include "application.hpp"
#include "mqtt.hpp"

Mqtt::Mqtt(VeQItem *service, VeQItemSettings *settings, QObject *parent) :
	QObject(parent)
{
	service->itemGetOrCreate("Mqtt")->itemAddChild("RegisterOnVrm", new VeQItemMqttBridgeRegistrar());

	QList<QString> mqttList = QList<QString>() << "Settings/Services/MqttLocal" << "Settings/Services/MqttVrm";
    new DaemonToolsService(settings, "/service/flashmq", mqttList, this, true, QStringList() << "-s" << "flashmq");
	new DaemonToolsService(settings, "/service/mqtt-rpc", mqttList, this, false, QStringList() << "-s" << "mqtt-rpc");

	// MQTT on LAN insecure
	mMqttLocalInsecure = settings->root()->itemGetOrCreate("Settings/Services/MqttLocalInsecure");
	mMqttLocalInsecure->getValueAndChanges(this, SLOT(mqttLocalInsecureChanged(QVariant)));

	// MQTT on LAN
	mMqttLocal = settings->root()->itemGetOrCreate("Settings/Services/MqttLocal");
	mMqttLocal->getValueAndChanges(this, SLOT(mqttLocalChanged(QVariant)));
}


void Mqtt::mqttCheckLocalInsecure()
{
	if (!mMqttLocal || !mMqttLocalInsecure)
		return;

	QVariant local = mMqttLocal->getLocalValue();
	QVariant insecure = mMqttLocalInsecure->getLocalValue();

	if (!local.isValid() || !insecure.isValid())
		return;

	// If local MQTT is disable, disable insecure access as well.
	if (!local.toBool() && insecure.toBool())
		mMqttLocalInsecure->setValue(false);
}

void Mqtt::mqttLocalChanged(QVariant var)
{
	if (!var.isValid())
		return;

	if (mMqttLocalWasValid) {
		DaemonToolsService upnp("/service/simple-upnpd");
		upnp.restart();
	}
	mMqttLocalWasValid = true;

	if (var.toBool()) {
		qDebug() << "[Firewall] Allow local MQTT over SSL, port 8883";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "8883");
	} else {
		qDebug() << "[Firewall] Disallow local MQTT over SSL, port 8883";
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "8883");
		mqttCheckLocalInsecure();
	}
}

void Mqtt::mqttLocalInsecureChanged(QVariant var)
{
	if (!var.isValid())
		return;

	if (var.toBool()) {
		qDebug() << "[Firewall] Allow insecure access to MQTT, port 1883 / 9001";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "1883");
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "9001");
		mqttCheckLocalInsecure();
	} else {
		qDebug() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "1883");
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "9001");
	}
}
