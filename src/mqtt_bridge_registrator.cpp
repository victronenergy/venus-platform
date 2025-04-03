#include "mqtt_bridge_registrator.hpp"

VeQItemMqttBridgeRegistrator::VeQItemMqttBridgeRegistrator(VeQItem *pltService) :
	VeQItemAction(),
	mPltService(pltService)
{
	mNetworkManager = new QNetworkAccessManager(this);
	connect(mNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onRequestFinished(QNetworkReply*)));
	setupSsl();
	registerWithVrm();
}

int VeQItemMqttBridgeRegistrator::setValue(const QVariant &value) {
	return VeQItemAction::setValue(value);
}

int VeQItemMqttBridgeRegistrator::check() {
	return 0;
}

void VeQItemMqttBridgeRegistrator::onRequestFinished(QNetworkReply *reply) {
	qDebug() << "onRequestFinished";
	if (reply->error()) {
		qDebug() << "error" << reply->errorString();
	} else {
		qDebug() << reply->readAll();
	}
}

void VeQItemMqttBridgeRegistrator::setupSsl()
{
	QFile certFile("/etc/ssl/certs/ccgx-ca.pem");
	if (!certFile.open(QIODevice::ReadOnly)) {
		qCritical() << certFile.fileName() << " not opened";
		return;
	}

	QList<QSslCertificate> certificates;
	QSslCertificate cert(&certFile, QSsl::Pem);
	certificates.append(cert);
	mSslConfig.setCaCertificates(certificates);
}

void VeQItemMqttBridgeRegistrator::registerWithVrm()
{
	qDebug() << "request";
	QString url("https://ccgxlogging.victronenergy.com/log/storemqttpassword.php");

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setHeader(QNetworkRequest::UserAgentHeader, "dbus-mqtt");
	request.setSslConfiguration(mSslConfig);

	VeQItem *item = mPltService->itemGet("Device/UniqueId");
	if (!item) {
		qCritical() << "unique id could not be retrieved";
		return;
	}

	QString uniqueId = item->getValue().toString();
	if (uniqueId.isEmpty()) {
		qCritical() << "mqtt_registrator: failed to get unique-id";
		return;
	}
	QString brokerUser = "ccgxapikey_" + uniqueId;

	QString password = readFirstLineFromFile("/data/conf/mqtt_password.txt");
	if (password.isEmpty()) {
		qCritical() << "failed to read mqtt_password";
		return;
	}
	QByteArray data = "identifier=" + QUrl::toPercentEncoding(brokerUser) + "&mqttPassword=" + QUrl::toPercentEncoding(password);
	mNetworkManager->post(request, data);
}
