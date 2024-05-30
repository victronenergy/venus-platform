#define QT_NO_DEBUG_STREAM

#include <QDir>

#include "application.hpp"
#include "mqtt.hpp" // FIXME, bridge
#include "security_profiles.hpp"

static QString sha26Script = "/usr/bin/calc-gui-v2-wasm-sha26.sh";

// The security level can be lowered to allow convenien, but less
// secure features.
enum SecurityProfile {
	SECURITY_PROFILE_SECURED, // (default)
	SECURITY_PROFILE_WEAK,
	SECURITY_PROFILE_UNSECURED
};

// Setting to determine if MQTT is available on normal network sockets as
// well as the websockets used by Venus itself. The security level
// determines if unencrypted connected are allowed as well.
enum MqttAccess : int {
	MQTT_ACCESS_OFF, // (default)
	MQTT_ACCESS_ON
};

enum VrmPortal {
	VRM_PORTAL_OFF,
	VRM_PORTAL_READ_ONLY,
	VRM_PORTAL_FULL // (default)
};

VrmTunnelSetup::VrmTunnelSetup(VeQItem *pltService, VeQItemSettings *settings,
							   VenusServices *venusServices, QObject *parent) :
		QObject(parent),
		mPltService(pltService)
{
	// NOTE: The venus-access service actually starts the remote tunnel in case
	// remote support is enabled, hence that doesn't need to be done here.
	mVrmPortal = settings->root()->itemGetOrCreate("Settings/Network/VrmPortal");
	mVrmPortal->getValueAndChanges(this, SLOT(checkVrmTunnel()));

	// Is gui-v1 the only option
	if (QDir("/service/gui").exists()) {
		mIsGuiv1Running = true;

	// or if configurable, is it running?
	} else {
		VeQItem *item = settings->root()->itemGetOrCreate("Settings/Gui/RunningVersion");
		item->getValueAndChanges(this, SLOT(onRunningGuiVersionObtained(QVariant)));
	}

	// Even if gui-v1 is running, it only needs a tunnel when also VNC is anabled and
	// Full VRM portal access.
	mVncEnabled = settings->root()->itemGetOrCreate("Settings/System/VncLocal");
	mVncEnabled->getValueAndChanges(this, SLOT(checkVrmTunnel()));

	// Look for EV chargers
	connect(venusServices, SIGNAL(found(VenusService*)), SLOT(onServiceFound(VenusService*)));

	// Large image services
	if (serviceExists("node-red-venus")) {
		mNodeRed = settings->root()->itemGetOrCreate("Settings/Services/NodeRed");
		mNodeRed->getValueAndChanges(this, SLOT(checkVrmTunnel()));
	}

	if (serviceExists("signalk-server")) {
		mSignalK = settings->root()->itemGetOrCreate("Settings/Services/SignalK");
		mSignalK->getValueAndChanges(this, SLOT(checkVrmTunnel()));
	}

	// Calculating the checksum takes a couple of seconds or so and is only needed
	// for development / testing. So only run it if the rootfs is writable and
	// postpone it a bit, so it isn't done early during boot.
	if (QFileInfo("/var/www/venus/gui-beta").isWritable()) {
		mChecksumTimer.setInterval(30000);
		mChecksumTimer.setSingleShot(true);
		connect(&mChecksumTimer, SIGNAL(timeout()), SLOT(calculateWasmChecksum()));
		mChecksumTimer.start();
	}
}

void VrmTunnelSetup::checkVrmTunnel()
{
	bool doTunnel = false;
	bool ok;

	// If the VRM mode is “Full” the tunnel should be disabled.. Otherwise only
	// enable the tunnel if there is a service which requires it.
	if (mVrmPortal->getLocalValue().toInt(&ok) != VRM_PORTAL_FULL || !ok) {
		if (ok)
			qDebug() << "[tunnel] VRM portal is not set to full";
		goto setTunnel;
	}

	// Gui-v1 with console on VRM enabled.
	if (mIsGuiv1Running && mVncEnabled->getLocalValue().toInt(&ok) == 1 && ok) {
		qDebug() << "[tunnel] tunnel for gui-v1 + remote VNC";
		doTunnel = true;

	// Optional / large image services.
	} else if ( (mNodeRed && mNodeRed->getLocalValue().toInt(&ok) == 1) ||
					(mSignalK && mSignalK->getLocalValue().toInt(&ok) == 1)
				) {
		qDebug() << "[tunnel] tunnel for large image service";
		doTunnel = true;

	// EV Charging Station its admin page depends on the tunnel
	} else if (mEvChargerFound) {
		qDebug() << "[tunnel] tunnel for EV charger";
		doTunnel = true;

	// GUI-v2 development build delivery of WASM
	} else if (!mWasmChecksum.isEmpty()) {
		qDebug() << "[tunnel] wasm checksum differs";
		doTunnel = true;
	}

setTunnel:
	qDebug() << "[tunnel] do tunnel:" << doTunnel;
	mPltService->itemGetOrCreateAndProduce("ConnectVrmTunnel", doTunnel);
}

void VrmTunnelSetup::onWasmChecksumDone(int exitCode)
{
	if (exitCode == 0) {
		QString checksum = mCheckSha26Proc->readAllStandardOutput().trimmed();
		qDebug() << "[tunnel] the wasm hash is" << checksum;

		bool equal;
		QFile fh("/var/www/venus/gui-beta/venus-gui-v2.wasm.sha256");
		if (fh.open(QIODevice::ReadOnly)) {
			QString orig = QString(fh.readLine()).section(" ", 0, 0);
			equal = orig == checksum;
		} else {
			equal = false;
		}

		// If the wasm checksum differs, remember it and setup a tunnel if needed.
		if (!equal) {
			mWasmChecksum = checksum;
			checkVrmTunnel();
		}

	} else {
		qCritical() << "[tunnel] Calculcate wasm checksum failed with exitCode" << exitCode;
	}

	mCheckSha26Proc->deleteLater();
	mCheckSha26Proc = nullptr;
}

void VrmTunnelSetup::calculateWasmChecksum()
{
	if (mCheckSha26Proc) {
		qDebug() << "[tunnel]" << __FUNCTION__ << "already busy";
		return;
	}

	mCheckSha26Proc = new QProcess(this);
	connect(mCheckSha26Proc, SIGNAL(finished(int)), SLOT(onWasmChecksumDone(int)));
	qDebug() << "[tunnel] do check wasm checksum";
	mCheckSha26Proc->start(sha26Script);
}

void VrmTunnelSetup::onRunningGuiVersionObtained(QVariant const &var)
{
	qDebug() << "[tunnel] gui version" << var;

	bool ok;
	bool isGuiv1Running = var.toInt(&ok) == 1 && ok;

	if (mIsGuiv1Running != isGuiv1Running) {
		mIsGuiv1Running = isGuiv1Running;
		checkVrmTunnel();
		emit guiv1RunningChanged();
	}
}

void VrmTunnelSetup::onServiceFound(VenusService *service)
{
	// qDebug() << service->getName() << int(service->getType());
	if (service->getType() == VenusServiceType::EV_CHARGER) {
		mEvChargerFound = true;
		checkVrmTunnel();
	}
}

SecurityProfiles::SecurityProfiles(VeQItem *pltService, VeQItemSettings *settings,
								 VenusServices *venusServices, QObject *parent) :
	QObject(parent)
{
	// If flashmq is down, RegisterOnVrm won't be called either.
	pltService->itemGetOrCreate("Mqtt")->itemAddChild("RegisterOnVrm", new VeQItemMqttBridgeRegistrar());

	// NOTE: the setting is added system-wide in /etc/venus/settings, since several
	// programs / scripts depend on it.
	VeQItem *item = settings->root()->itemGetOrCreate("Settings/System/SecurityProfile");
	item->getValueAndChanges(this, SLOT(onSecurityProfileChanged(QVariant)));

	item = settings->root()->itemGetOrCreate("Settings/Services/MqttLocal");
	item->getValueAndChanges(this, SLOT(onMqttAccessChanged(QVariant)));

	item = settings->root()->itemGetOrCreate("Settings/Network/VrmPortal");
	item->getValueAndChanges(this, SLOT(onVrmPortalChange(QVariant)));

	vrmLoggerHttpsEnabled = settings->root()->itemGetOrCreate("Settings/Vrmlogger/HttpsEnabled");
	vrmLoggerHttpsEnabled->getValue();

	enableMqttOnLan(false); // Disable LAN socket access by default, unless explicitly enabled.
	mFlashMq = new DaemonToolsService("/service/flashmq", this);
	mFlashMq->setSveCtlArgs(QStringList() << "-s" << "flashmq");

	// Since gui-v2 ws requires it, always make the wss mqtt socket available.
	// The webserver will block access / instruct the user to set a Security Profile if not yet done.
	mFlashMq->install();

	// RPC commands are only needed by full access. Mqtt on LAN perhaps as well, but not gui-v2.
	mMqttRpc = new DaemonToolsService("/service/mqtt-rpc", this);
	mMqttRpc->setSveCtlArgs(QStringList() << "-s" << "mqtt-rpc");

	mTunnelSetup = new VrmTunnelSetup(pltService, settings, venusServices, this);
}

void SecurityProfiles::onSecurityProfileChanged(QVariant const &var)
{
	if (var.isValid()) {

		// only when changed, not startup...
		if (mSecurityProfile.isValid() && mSecurityProfile != var) {

			switch (var.toInt())
			{
			case SECURITY_PROFILE_SECURED:
				// - A password is required in secured mode. It is up to the UI to make sure that is
				//   actually done, since for example checking password strength is easier there.
				//   If that should be double checked, it should be part of the API, since this is
				//   invoked after the profile has already been changed.

				// https for VRM logger is required in secure mode.
				vrmLoggerHttpsEnabled->setValue(1);
				break;

			case SECURITY_PROFILE_WEAK:
				// In weak mode it is up to the user.
				break;

			case SECURITY_PROFILE_UNSECURED:
				{
					qDebug() << "Removing password protection since access level is set to insecure!";
					QFile passwd("/data/conf/vncpassword.txt");
					passwd.open(QIODevice::WriteOnly);
					passwd.resize(0);
					break;
				}
			}

			// Reload nginx, since it changes config depending on security level.
			// This is only needed when it actually changed, not during startup.
			Application::spawn("svc", QStringList() << "-t" << "/service/nginx/");
		}
	}

	mSecurityProfile = var;
	checkMqtt();
}

void SecurityProfiles::onMqttAccessChanged(QVariant const &var)
{
	if (mMqttAccess != var) {
		bool wasValid = mMqttAccess.isValid();

		mMqttAccess = var;
		checkMqtt();

		// If MqttLocal changed, restart upnpd as well, since it announces this
		// setting. Don't do that during initial startup though.
		if (wasValid) {
			DaemonToolsService upnp("/service/simple-upnpd");
			upnp.restart();
		}
	}
}

void SecurityProfiles::onVrmPortalChange(QVariant const &var)
{
	if (var != mVrmPortal) {
		mVrmPortal = var;

		// VRM logger also depends on this, since in Off Mode it shouldn't log to VRM,
		// but since if can log to offline storage, the process shouldn't be disabled
		// from here, but vrmlogger should handle that itself.

		// The Remote tunnel also depends on this setting, but also many more.. So it
		// is handled separately in the VrmTunnelSetup class above.

		checkMqtt();
	}
}

void SecurityProfiles::enableMqtt(bool enabled)
{
	if (!mFlashMq || !mMqttRpc)
		return;

	// The flashmq start script depends on the VrmPortal setting, so restart
	// it to update accordingly.
	mFlashMq->restart();

	if (!enabled)
		enableMqttOnLan(false);

	mMqttRpc->installOrRemove(enabled);
}

void SecurityProfiles::enableMqttOnLan(bool enabled)
{
	if (mMqttOnLan == enabled)
		return;
	mMqttOnLan = enabled;

	// If secure MQTT on LAN isn't even enabled, make sure the insecure version for sure isn't.
	if (!enabled)
		enableMqttOnLanInsecure(false);

	// 1883: encrypted normal socket
	if (enabled) {
		qWarning() << "[Firewall] Allow local MQTT over SSL, port 8883";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "8883");
	} else {
		qWarning() << "[Firewall] Disallow local MQTT over SSL, port 8883";
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "8883");
	}
}

void SecurityProfiles::enableMqttOnLanInsecure(bool enabled)
{
	if (mMqttOnLanInsecure == enabled)
		return;
	mMqttOnLanInsecure = enabled;

	// 1883: plain socket
	// 9001: plain websocket
	if (enabled) {
		qWarning() << "[Firewall] Allow insecure access to MQTT, port 1883 / 9001";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "1883");
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "9001");
	} else {
		qWarning() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "1883");
		Application::spawn("firewall", QStringList() << "deny" << "tcp" << "9001");
	}
}

void SecurityProfiles::checkMqtt()
{
	// Wait till all required settings are valid...
	if (!mMqttAccess.isValid() || !mSecurityProfile.isValid() || !mVrmPortal.isValid()) {
		enableMqtt(false);
		return;
	}

	if (mMqttAccess.toInt() == MQTT_ACCESS_ON || mVrmPortal.toInt() == VRM_PORTAL_FULL) {
		enableMqtt(true);

		// LAN support for MQTT (manage firewall rules)
		if (mMqttAccess.toInt() == MQTT_ACCESS_ON) {
			enableMqttOnLan(true);

			// If the security level permits it, also enable the unsecure version.
			int securityLevel = mSecurityProfile.toInt();
			bool allowInsecure = (securityLevel == SECURITY_PROFILE_WEAK || securityLevel == SECURITY_PROFILE_UNSECURED);
			enableMqttOnLanInsecure(allowInsecure);

		} else {
			enableMqttOnLan(false);
		}
	} else {
		enableMqtt(false);
	}
}
