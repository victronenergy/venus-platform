#pragma once

#include <QObject>
#include <QVariant>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "venus_services.hpp"

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
	void checkVncWebsocket();
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqttOnLan();
	void enableMqttBridge(bool restart);
	void enableMqttOnLan(bool enabled);
	void enableMqttOnLanInsecure(bool enabled);
	bool isPasswordProtected();

	QVariant mMqttAccess;
	QVariant mSecurityProfile;
	QVariant mVrmPortal;

	// These are QVariant, just to unequal fo false for startup/init
	QVariant mMqttOnLan;
	QVariant mMqttOnLanInsecure;

	DaemonToolsService *mFlashMq = nullptr;
	DaemonToolsService *mMqttRpc = nullptr;

	VrmTunnelSetup *mTunnelSetup;

	DaemonToolsService *mVncWebsocket = nullptr;
	VeQItem *mVncEnabled = nullptr;
};
