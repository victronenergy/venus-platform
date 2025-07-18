#include "mqtt_bridge_registrator.hpp"

#include "application.hpp"
#include "security_profiles.hpp"

static const char* flashMqBridgeConfigPath = "/data/conf/flashmq.d/vrm_bridge.conf";
static const char* caPath = "/etc/ssl/certs/ccgx-ca.pem";

bool VrmTokenRegistrator::generateAndOrGetPassword(QString &output)
{
	static const QString previousMosquittoConfigPath("/data/conf/mosquitto.d/vrm_bridge.conf");
	static const QString mqttPasswordFilePath("/data/conf/mqtt_password.txt");

	QString password;

	/*
	 * It can already be there for two reasons:
	 *
	 * - Newer GXs have it written on boot from EEPROM.
	 * - On Older GXs, we will have written it before by us.
	 *
	 * If the file is there, we use it at all costs, regardless of its contents. We should not overwrite it.
	 */
	if (QFile::exists(mqttPasswordFilePath)) {
		if (!quiet)
			qDebug() << "Using existing" << mqttPasswordFilePath;
		password = readFirstLineFromFile(mqttPasswordFilePath);

		if (password.isNull()) {
			qCritical() << "Reading from" << mqttPasswordFilePath << "failed.";
			return false;
		}

		output = password;
		return true;
	}

	/*
	 * As first source for the password, we check the Mosquitto config from earlier Venus versions.
	 */
	{
		QFile previous_mosquitto_config(previousMosquittoConfigPath);
		if (previous_mosquitto_config.open(QIODevice::ReadOnly | QIODevice::Text)) {
			qDebug() << "Taking MQTT password from" << previousMosquittoConfigPath;
			while (!previous_mosquitto_config.atEnd()) {
				QByteArray line_bytes = previous_mosquitto_config.readLine().trimmed();
				if (line_bytes.startsWith("remote_password")) {
					password = line_bytes.split(' ').at(1);
					break;
				}
			}
		}
	}

	if (password.isEmpty())
		password = getSecureRandomString(32);

	if (password.isEmpty())
		return false;

	qDebug() << "Writing new" << mqttPasswordFilePath;

	if (!writeFileAtomically(mqttPasswordFilePath, password))
		return false;

	if (QFile::exists(previousMosquittoConfigPath))
		QFile::remove(previousMosquittoConfigPath);

	output = password;
	return true;
}

VrmTokenRegistrator::VrmTokenRegistrator(const QString &vrmId, const QVariant &vrmPortalMode) :
	mVrmId(vrmId),
	mVrmPortalMode(vrmPortalMode)
{
	{
		QFile rpcTemplateFile("/usr/share/flashmq/venus_rpc_bridge_template.conf");
		if (rpcTemplateFile.open(QIODevice::ReadOnly)) {
			mBridgeSettingsRpcTemplate = QString::fromUtf8(rpcTemplateFile.readAll()).trimmed();
		} else {
			qCritical() << "Failed to load" << rpcTemplateFile.fileName();
		}
	}

	{
		QFile dbusTemplateFile("/usr/share/flashmq/venus_dbus_bridge_template.conf");
		if (dbusTemplateFile.open(QIODevice::ReadOnly)) {
			mBridgeSettingsDbusTemplate = QString::fromUtf8(dbusTemplateFile.readAll()).trimmed();
		} else {
			qCritical() << "Failed to load" << dbusTemplateFile.fileName();
		}
	}

	// Avoid a dead sym-link, which causes FlashMQ to error out.
	if (!QFile::exists(flashMqBridgeConfigPath))
		writeFileAtomically(flashMqBridgeConfigPath, "");

	connect(&mNetworkManager, &QNetworkAccessManager::finished, this, &VrmTokenRegistrator::onNetworkRequestFinished);
	setupSsl();

	mBrokerUser = "ccgxapikey_" + mVrmId;

	{
		QFile configFile(flashMqBridgeConfigPath);
		if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QByteArray contents = configFile.readAll();
			mCurrentConfig = QString::fromUtf8(contents);
		} else {
			qCritical() << "Reading exiting " << flashMqBridgeConfigPath << "failed.";
		}
	}
}

bool VrmTokenRegistrator::start()
{
	if (!mVrmPortalMode.isValid()) {
		qCritical() << "Cannot register VRM/MQTT token. The VRM Portal mode setting has not been given to us.";
		return false;
	}

	if (mBridgeSettingsDbusTemplate.isEmpty() || mBridgeSettingsRpcTemplate.isEmpty()) {
		qCritical() << "FlashMQ template files are not loaded. Cannot continue.";
		return false;
	}

	return registerWithVrm();
}

void VrmTokenRegistrator::stop()
{
	stopping = true;

	foreach (auto *reply, mNetworkManager.findChildren<QNetworkReply*>()) {
		if (!reply)
			continue;
		reply->abort();
	}
}

void VrmTokenRegistrator::setupSsl()
{
	QFile certFile(caPath);
	if (!certFile.open(QIODevice::ReadOnly)) {
		qCritical() << certFile.fileName() << " not opened";
		return;
	}

	QList<QSslCertificate> certificates;
	QSslCertificate cert(&certFile, QSsl::Pem);
	certificates.append(cert);
	mSslConfig.setCaCertificates(certificates);
}

void VrmTokenRegistrator::onNetworkRequestFinished(QNetworkReply *reply)
{
	if (stopping)
		return;

	if (!reply)
		return;

	const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (!(reply->error() == QNetworkReply::NoError && httpCode == 200 && reply->readAll().startsWith("OK:"))) {
		std::chrono::milliseconds retry(60000);
		if (!quiet)
			qDebug() << "VrmTokenRegistrator network request failed. Retrying silently every" << retry.count() << "ms.";
		QTimer::singleShot(retry, this, &VrmTokenRegistrator::registerWithVrm);
		quiet = true;
		return;
	}

	qDebug() << "VrmTokenRegistrator posted token successfully.";
	writeFlashMQConfig();
}

void VrmTokenRegistrator::writeFlashMQConfig()
{
	const VrmPortal portalSetting = static_cast<VrmPortal>(mVrmPortalMode.toInt());

	QString rpc;
	QString dbus;

	if (portalSetting == VRM_PORTAL_FULL) {
		rpc = expandFlashMQConfigTemplate(mBridgeSettingsRpcTemplate);
		rpc.append("\n"); // Ensures file is identical with what mosquitto_bridge_registrator.py did.
	}

	if (portalSetting >= VRM_PORTAL_READ_ONLY)
		dbus = expandFlashMQConfigTemplate(mBridgeSettingsDbusTemplate);

	QString newConfig("# Generated by BridgeRegistrator. Any changes will be overwritten on service start.\n\n");
	newConfig.append(rpc);
	newConfig.append(dbus);

	bool configChanged = false;

	if (newConfig != mCurrentConfig) {
		qDebug() << "The FlashMQ bridge config has changed. Writing new one.";
		if (!writeFileAtomically(flashMqBridgeConfigPath, newConfig)) {
			qCritical() << "Writing" << flashMqBridgeConfigPath << "failed.";
		}
		configChanged = true;
	} else {
		qDebug() << "The FlashMQ bridge config has not changed. Not writing new one.";
	}

	emit done(configChanged);
}

QString VrmTokenRegistrator::expandFlashMQConfigTemplate(QString str) const
{
	str.replace("%%dynamicaddress%%", getBrokerHostname());
	str.replace("%%vrmid%%", mVrmId);
	str.replace("%%remote_username%%", mBrokerUser);
	str.replace("%%remote_password%%", mBrokerPassword);
	str.replace("%%ca_file%%", caPath);
	str.append("\n\n");
	return str;
}

QString VrmTokenRegistrator::getBrokerHostname() const
{
	int sum = 0;
	const QByteArray l = mVrmId.toLower().trimmed().toUtf8();

	for (const auto &c : l) {
		uint8_t c2 = static_cast<uint8_t>(c);
		sum += c2;
	}

	int index = sum % 128;

	QString addr = QString("mqtt%1.victronenergy.com").arg(index);
	return addr;
}

bool VrmTokenRegistrator::registerWithVrm()
{
	if (!generateAndOrGetPassword(mBrokerPassword)) {
		qDebug() << "Unable to obtain or generate password to register";
		return false;
	}

	if (!quiet)
		qDebug() << "Initiating posting of MQTT/token password.";

	QString url("https://ccgxlogging.victronenergy.com/log/storemqttpassword.php");

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setHeader(QNetworkRequest::UserAgentHeader, "venus-platform");
	request.setSslConfiguration(mSslConfig);

	QByteArray data = "identifier=" + QUrl::toPercentEncoding(mBrokerUser) + "&mqttPassword=" + QUrl::toPercentEncoding(mBrokerPassword);
	mNetworkManager.post(request, data);
	return true;
}


VeQItemMqttBridgeRegistrator::VeQItemMqttBridgeRegistrator(VeQItem *pltService) :
	VeQItemAction()
{
	VeQItem *itemVrmId = pltService->itemGet("Device/UniqueId");
	if (!itemVrmId) {
		qCritical() << "VeQItemMqttBridgeRegistrator: unique id could not be retrieved";
		return;
	}

	QString uniqueId = itemVrmId->getValue().toString();
	if (uniqueId.isEmpty())	{
		qCritical() << "VeQItemMqttBridgeRegistrator: failed to get unique-id";
		return;
	}

	mVrmId = uniqueId;
}

int VeQItemMqttBridgeRegistrator::setValue(const QVariant &value)
{
	(void) value;

	if (!mVrmPortalMode.isValid()) {
		mRegistrationIsPending = true;
		return 0;
	}

	return check();
}

void VeQItemMqttBridgeRegistrator::setVrmPortalMode(const QVariant &mode)
{
	this->mVrmPortalMode = mode;

	if (mRegistrationIsPending) {
		mRegistrationIsPending = false;
		check();
	}
}

int VeQItemMqttBridgeRegistrator::check()
{
	if (registrator) {
		/*
		 * Abort any already running request instead of returning here, because when the vrm portal mode
		 * setting is flapped by the user a couple of times, we must be sure we process the final
		 * setting (because we not only register a token, we also generate a FlashMQ config depending on settings).
		 */
		registrator->stop();
		registrator.reset();
	}

	registrator.reset(new VrmTokenRegistrator(mVrmId, mVrmPortalMode));
	connect(registrator.get(), &VrmTokenRegistrator::done, this, &VeQItemMqttBridgeRegistrator::onRegistratorDone);

	if (!registrator->start()) {
		qCritical() << "Failure starting the VrmTokenRegistrator.";
		registrator.reset();
		return -1;
	}

	return 0;
}

void VeQItemMqttBridgeRegistrator::onRegistratorDone(bool configChanged)
{
	registrator.reset();

	if (configChanged)
		emit bridgeConfigChanged();
}
