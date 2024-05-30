#pragma once

#include <QObject>
#include <QVariant>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "venus_services.hpp"

class VrmTunnelSetup : QObject
{
	Q_OBJECT

public:
	VrmTunnelSetup(VeQItem *pltService, VeQItemSettings *settings,
				   VenusServices *venusServices, QObject *parent= nullptr);

	QString wasmChecksum() { return mWasmChecksum; }

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
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqtt();
	void enableMqtt(bool enabled);
	void enableMqttOnLan(bool enabled);
	void enableMqttOnLanInsecure(bool enabled);

	QVariant mMqttAccess;
	QVariant mSecurityProfile;
	QVariant mVrmPortal;

	bool mMqtt = false;
	bool mMqttOnLan = false;
	bool mMqttOnLanInsecure = false;

	VeQItem *vrmLoggerHttpsEnabled;

	DaemonToolsService *mFlashMq;
	DaemonToolsService *mMqttRpc;

	VrmTunnelSetup *tunnelSetup;
};
