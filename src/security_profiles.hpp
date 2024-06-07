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
	explicit SecurityApi() : VeQItemAction() {};
	int setValue(const QVariant &value) override;
};

class VrmTunnelSetup : public QObject
{
	Q_OBJECT

public:
	VrmTunnelSetup(VeQItem *pltService, VeQItemSettings *settings,
				   VenusServices *venusServices, QObject *parent= nullptr);

	QString wasmChecksum() { return mWasmChecksum; }
	bool isGuiv1Running() { return mIsGuiv1Running; }

signals:
	void guiv1RunningChanged();

private slots:
	void calculateWasmChecksum();
	void checkVrmTunnel();
	void onRunningGuiVersionObtained(QVariant const &var);
	void onServiceFound(VenusService *service);
	void onWasmChecksumDone(int exitCode);

private:
	VeQItem *mPltService; // This service itself

	bool mIsGuiv1Running = false;
	VeQItem *mVncEnabled;
	VeQItem *mVrmPortal;
	VeQItem *mNodeRed = nullptr;
	VeQItem *mSignalK = nullptr;
	QProcess *mCheckSha26Proc = nullptr;

	QString mWasmChecksum; /* if different than the checksum as stored in the checksum file */
	QTimer mChecksumTimer;

	bool mEvChargerFound = false;
};

class SecurityProfiles : public QObject {
	Q_OBJECT

public:
	SecurityProfiles(VeQItem *pltService, VeQItemSettings *settings,
					VenusServices *venusServices, QObject *parent = nullptr);

private slots:
	void checkVncWebsocket();
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqtt();
	void enableMqtt(bool enabled);
	void enableMqttOnLan(bool enabled);
	void enableMqttOnLanInsecure(bool enabled);
	bool hasPasswordFile();
	bool isPasswordProtected();

	QVariant mMqttAccess;
	QVariant mSecurityProfile;
	QVariant mVrmPortal;

	// These are QVariant, just to unequal fo false for startup/init
	QVariant mMqttOnLan;
	QVariant mMqttOnLanInsecure;

	VeQItem *vrmLoggerHttpsEnabled;

	DaemonToolsService *mFlashMq = nullptr;
	DaemonToolsService *mMqttRpc = nullptr;

	VrmTunnelSetup *mTunnelSetup;

	DaemonToolsService *mVncWebsocket = nullptr;
	VeQItem *mVncEnabled = nullptr;
};
