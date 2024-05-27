#include <QDir>

#include "application.hpp"
#include "json.h"
#include "mqtt.hpp" // FIXME, bridge
#include "security_profiles.hpp"

static QString sha26Script = "/usr/bin/calc-gui-v2-wasm-sha26.sh";

// The security level can be lowered to allow convenien, but less
// secure features.
enum SecurityProfile {
	SECURITY_PROFILE_SECURED, // (default)
	SECURITY_PROFILE_WEAK,
	SECURITY_PROFILE_UNSECURED,
	SECURITY_PROFILE_LAST = SECURITY_PROFILE_UNSECURED
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

// These are events, not states like above. Changing the Security Profile will
// reconfigure the webserver and reset the served websockets. Waiting 3 seconds
// or so should be roughly enough before attempting to reconnect. A password
// change should force existing users to login again with the new password.
enum NetworkConfigEvent {
	NETWORK_CONFIG_NO_EVENT,
	/// this should trigger a re-authentication, there isn't a delay here
	NETWORK_CONFIG_PASSWORD_CHANGED,
	/// Changing https/http config is a webserver reload. That also disconnect
	/// wss e.g. so the actual action is delayed, so it is communicated that it
	/// is going to happen. It is actually done 2 seconds after announcing it.
	NETWORK_CONFIG_SECURITY_PROFILE_CHANGED,
};

SecurityApi::SecurityApi(VeQItem *pltService, VeQItemSettings *settings) :
	VeQItemAction()
{
	mVrmLoggerHttpsEnabled = settings->root()->itemGetOrCreate("Settings/Vrmlogger/HttpsEnabled");
	mVrmLoggerHttpsEnabled->getValue();

	mSecurityProfile = settings->root()->itemGetOrCreate("Settings/System/SecurityProfile");
	mSecurityProfile->getValue();

	mPendingServiceRestart = pltService->itemGetOrCreateAndProduce("Network/ConfigChanged", NETWORK_CONFIG_NO_EVENT);
};

int SecurityApi::setValue(const QVariant &value)
{
	bool ok;
	QVariantMap map;
	QVariant password;
	QVariant data;
	QVariant securityProfile;
	NetworkConfigEvent configEvent = NETWORK_CONFIG_NO_EVENT;

	ok = value.isValid();
	if (!ok) {
		qWarning() << "[Api]" << uniqueId() << "invalid value";
		goto out;
	}

	data = QtJson::parse(value.toString(), ok);
	if (!ok) {
		qWarning() << uniqueId() << "[Api] unable to parse the json";
		goto out;
	}

	map = data.toMap();

	// It can't hurt to double check that the user actually has permissions to call this.

	password = map.value("SetPassword");
	if (password.isValid()) {

		bool hadPasswordFile = SecurityProfiles::hasPasswordFile();
		QProcess cmd;
		// qDebug() << "[Api] Changing password to " << password.toString();
		cmd.start("/sbin/ve-set-passwd", QStringList() << password.toString());
		if (!cmd.waitForFinished() || cmd.exitCode() != 0) {
			qCritical() << "[Api] Changing the password failed";
			ok = false;
			goto out;
		}

		// Since the password changed, make sure users need to enter credentials again.
		Application::invalidateAuthenticatedSessions();
		configEvent = NETWORK_CONFIG_PASSWORD_CHANGED; // Trigger users to login again

		// Announce on the network as well that wss:// mqtt logins now work.
		if (!hadPasswordFile)
			SecurityProfiles::restartUpnp();
	}

	password = map.value("SetRootPassword");
	if (password.isValid()) {
		ok = Application::setRootPassword(password.toString());
	}

	securityProfile = map.value("SetSecurityProfile");
	if (securityProfile.isValid()) {
		uint value = securityProfile.toUInt(&ok);
		if (!ok || value > SECURITY_PROFILE_LAST) {
			ok = false;
			qCritical() << "[Api] Invalid security profile" << securityProfile.toString();
			goto out;
		}

		qCritical() << "[Api] Changing security profile to" << value;
		switch (value)
		{
		case SECURITY_PROFILE_SECURED:
			// - A password is required in secured mode. It is up to the UI to make sure that is
			//   actually done, since for example checking password strength is easier there.
			//   If that should be double checked, it should be part of the API, since this is
			//   invoked after the profile has already been changed.

			// https for VRM logger is required in secure mode.
			mVrmLoggerHttpsEnabled->setValue(1);
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

		mSecurityProfile->setValue(value);

		/*
		 * Force reauthentication / or reload nginx, since it changes config depending on
		 * the security level.
		 *
		 * There is a problem here how to (reliably) inform the users about this
		 * change, since the communication itself might happen over a websocket
		 * served by the webserver which is going to be restarted.
		 *
		 * Since there might be multiple, indirect recipients, for example:
		 *
		 * venus-platorm  -> flashmq    -> gui-v2
		 *                              -> venus-html-app
		 *                              -> VRM (gui-v2 wasm)
		 *                              -> VRM (realtime updates)
		 *                              -> NodeRed
		 *                              -> SignalK
		 *
		 * Don't attempt to check if all of them handled a notification succesfully,
		 * just report it and wait a bit before actually adjusting services. Worst
		 * case an user must reload a page for things to work correctly again. It is
		 * not changed ofter anyway...
		 */

		configEvent = NETWORK_CONFIG_SECURITY_PROFILE_CHANGED;
		QTimer *timer = new QTimer(this);
		connect(timer, SIGNAL(timeout()), SLOT(restartWebserver()));
		timer->setSingleShot(true);
		timer->start(2000);
	}

	// Bump the restart number, to make sure it always changes.
	if (configEvent != NETWORK_CONFIG_NO_EVENT) {
		mPendingServiceRestart->produceValue(configEvent);

		QTimer *timer = new QTimer(this);
		timer->setSingleShot(true);
		connect(timer, SIGNAL(timeout()), this, SLOT(resetConfigEvent()));
		connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater()));
		timer->start(1000);
	}

out:
	produceValue(QString(), State::Synchronized, true);
	return ok ? 0 : -1;
}

void SecurityApi::restartWebserver()
{
	// This is delayed, so hopefully the users are already informed that the
	// services will temporarily be down.
	qCritical() << "[Api] restarting the webserver";
	Application::spawn("svc", QStringList() << "-t" << "/service/nginx/");

	QTimer *timer = qobject_cast<QTimer *>(sender());
	delete timer;
}

void SecurityApi::resetConfigEvent()
{
	mPendingServiceRestart->produceValue(NETWORK_CONFIG_NO_EVENT);
}

VrmTunnelSetup::VrmTunnelSetup(VeQItem *pltService, VeQItemSettings *settings,
							   VenusServices *venusServices, QObject *parent) :
		QObject(parent),
		mPltService(pltService)
{
	// NOTE: The venus-access service actually starts the remote tunnel in case
	// remote support is enabled, hence that doesn't need to be done here.
	mVrmPortal = settings->root()->itemGetOrCreate("Settings/Network/VrmPortal");
	mVrmPortal->getValueAndChanges(this, SLOT(checkVrmTunnel()));

	connect(qApp, SIGNAL(runningGuiVersionChanged()), this, SLOT(checkVrmTunnel()));
	checkVrmTunnel();

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
	if (Application::runningGuiVersion() == 1 && mVncEnabled->getLocalValue().toInt(&ok) == 1 && ok) {
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
	}

setTunnel:
	qDebug() << "[tunnel] do tunnel:" << doTunnel;
	mPltService->itemGetOrCreateAndProduce("ConnectVrmTunnel", doTunnel);
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

	pltService->itemGetOrCreate("Security")->itemAddChild("Api", new SecurityApi(pltService, settings));

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

	// handle the VNC websocket for gui-v1 remote console on LAN.
	mVncWebsocket = new DaemonToolsService("/service/websockify-c");
	mVncWebsocket->setSveCtlArgs(QStringList() << "-s" << "websockify-c");
	mVncWebsocket->setRestart(false);

	// NOTE: the setting is added system-wide in /etc/venus/settings, since several
	// programs / scripts depend on it.
	VeQItem *item = settings->root()->itemGetOrCreate("Settings/System/SecurityProfile");
	item->getValueAndChanges(this, SLOT(onSecurityProfileChanged(QVariant)));

	item = settings->root()->itemGetOrCreate("Settings/Services/MqttLocal");
	item->getValueAndChanges(this, SLOT(onMqttAccessChanged(QVariant)));

	item = settings->root()->itemGetOrCreate("Settings/Network/VrmPortal");
	item->getValueAndChanges(this, SLOT(onVrmPortalChange(QVariant)));

	mVncEnabled = settings->root()->itemGetOrCreate("Settings/System/VncLocal");
	mVncEnabled->getValueAndChanges(this, SLOT(checkVncWebsocket()));

	connect(qApp, SIGNAL(runningGuiVersionChanged()), this, SLOT(checkVncWebsocket()));
	checkVncWebsocket();
}

void SecurityProfiles::onSecurityProfileChanged(QVariant const &var)
{
	// Note: changing the Security Profile should be done though the SecurityApi
	mSecurityProfile = var;
	checkMqttOnLan();
	checkVncWebsocket();
}

void SecurityProfiles::onVrmPortalChange(QVariant const &var)
{
	if (var != mVrmPortal) {
		bool wasValid = mVrmPortal.isValid();
		mVrmPortal = var;

		// VRM logger also depends on this, since in Off Mode it shouldn't log to VRM,
		// but since if can log to offline storage, the process shouldn't be disabled
		// from here, but vrmlogger should handle that itself.

		// The Remote tunnel also depends on this setting, but also many more.. So it
		// is handled separately in the VrmTunnelSetup class above.

		// Flashmq depends on the settings as well, since off, read-only and full change
		// the bridge configuration.
		enableMqttBridge(wasValid);
	}
}

void SecurityProfiles::enableMqttBridge(bool restart)
{
	if (!mFlashMq || !mMqttRpc)
		return;

	// The flashmq start script depends on the VrmPortal setting, so restart
	// it to update accordingly. NOTE: this should no longer be needed, since
	// flashmq should be able to reload dynamically nowadays.
	if (restart)
		mFlashMq->restart();

	bool enabled = mVrmPortal.toInt() == VRM_PORTAL_FULL;
	mMqttRpc->installOrRemove(enabled);
}

void SecurityProfiles::restartUpnp()
{
	DaemonToolsService upnp("/service/simple-upnpd");
	upnp.restart();
}

void SecurityProfiles::onMqttAccessChanged(QVariant const &var)
{
	if (mMqttAccess != var) {
		bool wasValid = mMqttAccess.isValid();

		mMqttAccess = var;
		checkMqttOnLan();

		// If MqttLocal changed, restart upnpd as well, since it announces this
		// setting. Don't do that during initial startup though.
		if (wasValid)
			restartUpnp();
	}
}

void SecurityProfiles::checkMqttOnLan()
{
	// Wait till all required settings are valid...
	if (!mMqttAccess.isValid() || !mSecurityProfile.isValid()) {
		enableMqttOnLan(false);
		return;
	}

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
		Application::run("firewall", QStringList() << "allow" << "tcp" << "8883");
	} else {
		qWarning() << "[Firewall] Disallow local MQTT over SSL, port 8883";
		Application::run("firewall", QStringList() << "deny" << "tcp" << "8883");
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
		Application::run("firewall", QStringList() << "allow" << "tcp" << "1883");
		Application::run("firewall", QStringList() << "allow" << "tcp" << "9001");
	} else {
		qWarning() << "[Firewall] Disallow insecure access to MQTT, port 1883 / 9001";
		Application::run("firewall", QStringList() << "deny" << "tcp" << "1883");
		Application::run("firewall", QStringList() << "deny" << "tcp" << "9001");
	}
}

bool SecurityProfiles::hasPasswordFile()
{
	return QFile("/data/conf/vncpassword.txt").exists();
}

bool SecurityProfiles::isPasswordProtected()
{
	QFile passwd("/data/conf/vncpassword.txt");
	if (!passwd.open(QIODevice::ReadOnly))
		return false;

	return !QString::fromUtf8(passwd.readLine()).trimmed().isEmpty();
}

void SecurityProfiles::checkVncWebsocket()
{
	if (!mVncEnabled || !mVncWebsocket)
		return;

	// VNC over the websocket is needed for gui-v1 with VNC enabled. Futhermore it
	// must be allowed; If no password file exists, it is disabled otherwise enabled.
	// It exists for compatibility reasons. VncLocal = 0, VnvInternet = 1 and no password
	// used to be a valid setup. New devices should always have a password or explicitly
	// make create an empty password file.

	bool vncEnabled = mVncEnabled->getLocalValue().toBool();
	bool hasPassword = hasPasswordFile();
	bool vncWsEnabled = vncEnabled && hasPassword && Application::runningGuiVersion() == 1;

	qDebug() << "VNC enabled: " << vncEnabled << "vncOnLanAllowed: " << hasPassword <<
				"vncWsEnabled: " << vncWsEnabled << "gui-version" << Application::runningGuiVersion();
	mVncWebsocket->installOrRemove(vncWsEnabled);
}
