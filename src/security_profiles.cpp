#include "application.hpp"
#include "mqtt.hpp" // FIXME, bridge
#include "security_profiles.hpp"

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

SecurityProfiles::SecurityProfiles(VeQItem *pltService, VeQItemSettings *settings, QObject *parent) :
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

	vrmLoggerHttpsEnabled = settings->root()->itemGetOrCreate("/Settings/Vrmlogger/HttpsEnabled");
	vrmLoggerHttpsEnabled->getValue();

	mFlashMq = new DaemonToolsService("/service/flashmq", this);
	mFlashMq->setSveCtlArgs(QStringList() << "-s" << "flashmq");

	// Since gui-v2 ws requires it, always make the wss mqtt socket available.
	// XXX: But what if there is no password set?
	// XXX: Only start it by default when running gui-v2?
	mFlashMq->install();

	// RPC commands are only needed by full access. Mqtt on LAN perhaps as well, but not gui-v2.
	mMqttRpc = new DaemonToolsService("/service/mqtt-rpc", this);
	mMqttRpc->setSveCtlArgs(QStringList() << "-s" << "mqtt-rpc");
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
	if (mMqtt == enabled)
		return;
	mMqtt = enabled;

	if (!enabled)
		enableMqttOnLan(false);

	if (enabled) {
		mMqttRpc->install();
	} else {
		mMqttRpc->remove();
	}
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
		qDebug() << "[Firewall] Allow local MQTT over SSL, port 8883";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "8883");
	} else {
		qDebug() << "[Firewall] Disallow local MQTT over SSL, port 8883";
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
		qDebug() << "[Firewall] Allow insecure access to MQTT, port 1883 / 9001";
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "1883");
		Application::spawn("firewall", QStringList() << "allow" << "tcp" << "9001");
	} else {
		qDebug() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
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
