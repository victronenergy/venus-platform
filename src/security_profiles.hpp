#pragma once

#include <QObject>
#include <QVariant>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "mqtt_bridge_registrator.hpp"
#include "venus_services.hpp"

// The security level can be lowered to allow convenien, but less
// secure features.
enum SecurityProfile {
	SECURITY_PROFILE_SECURED, // (default)
	SECURITY_PROFILE_WEAK,
	SECURITY_PROFILE_UNSECURED,
	SECURITY_PROFILE_INDETERMINATE,
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

class SecurityApi : public VeQItemAction
{
	Q_OBJECT

public:
	explicit SecurityApi(VeQItem *pltService, VeQItemSettings *settings);
	int setValue(const QVariant &value) override;

private slots:
	void restartWebserver();
	void resetConfigEvent();

private:
	VeQItem *mVrmLoggerHttpsEnabled;
	VeQItem *mSecurityProfile;
	VeQItem *mPendingServiceRestart;
};

class VrmTunnelSetup : public QObject
{
	Q_OBJECT

public:
	VrmTunnelSetup(VeQItem *pltService, VeQItemSettings *settings,
				   VenusServices *venusServices, QObject *parent= nullptr);

private slots:
	void checkVrmTunnel();
	void onServiceFound(VenusService *service);

private:
	void onSecurityProfileChanged(QVariant const &var);

	VeQItem *mPltService; // This service itself

	VeQItem *mVncEnabled;
	VeQItem *mVrmPortal;
	VeQItem *mNodeRed = nullptr;
	VeQItem *mSignalK = nullptr;

	bool mEvChargerFound = false;
};

class SecurityProfiles : public QObject {
	Q_OBJECT

public:
	SecurityProfiles(VeQItem *pltService, VeQItemSettings *settings,
					VenusServices *venusServices, QObject *parent = nullptr);

	QVariant getProfile() { return mSecurityProfile; }

	static bool hasPasswordFile();
	static void restartUpnp();

private slots:
	void onBridgeConfigChanged();
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqttOnLan();
	void enableMqttBridge(bool configChanged = false);
	void enableMqttOnLan(bool enabled);
	void enableMqttOnLanInsecure(bool enabled);

	QVariant mMqttAccess;
	QVariant mSecurityProfile;
	QVariant mVrmPortal;

	// These are QVariant, just to unequal fo false for startup/init
	QVariant mMqttOnLan;
	QVariant mMqttOnLanInsecure;

	DaemonToolsService *mFlashMq = nullptr;
	DaemonToolsService *mMqttRpc = nullptr;

	VrmTunnelSetup *mTunnelSetup;

	VeQItemMqttBridgeRegistrator *mMqttBridgeRegistrator = nullptr;
};
