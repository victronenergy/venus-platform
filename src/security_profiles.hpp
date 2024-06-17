#pragma once

#include <QObject>
#include <QVariant>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "venus_services.hpp"

class VeQItemMqttBridgeRegistrar: public VeQItemAction {
	Q_OBJECT

public:
	VeQItemMqttBridgeRegistrar() :
		VeQItemAction()
	{}

	int setValue(const QVariant &value) override {
		if (mProc)
			return -1;

		mProc = new QProcess();
		connect(mProc, SIGNAL(finished(int)), this, SLOT(onFinished()));
		connect(mProc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onErrorOccurred(QProcess::ProcessError)));
		qDebug() << "[MqttBridgeRegistrar]" << "registering";
		mProc->start("mosquitto_bridge_registrator.py");

		return VeQItemAction::setValue(value);
	}

private slots:
	void onFinished() {
		qDebug() << "[MqttBridgeRegistrar]" << "done";
		mProc->deleteLater();
		mProc = nullptr;
	}

	void onErrorOccurred(QProcess::ProcessError error) {
		qDebug() << "[MqttBridgeRegistrar]" << "error during registration" << error;
		mProc->deleteLater();
		mProc = nullptr;
	}

private:
	QProcess *mProc = nullptr;
};

class SecurityApi : public VeQItemAction
{
	Q_OBJECT

public:
	explicit SecurityApi(VeQItem *pltService, VeQItemSettings *settings);
	int setValue(const QVariant &value) override;

private slots:
	void restartWebserver();

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

	QString wasmChecksum() { return mWasmChecksum; }

private slots:
	void calculateWasmChecksum();
	void checkVrmTunnel();
	void onServiceFound(VenusService *service);
	void onWasmChecksumDone(int exitCode);

private:
	void onSecurityProfileChanged(QVariant const &var);

	VeQItem *mPltService; // This service itself

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

	QVariant getProfile() { return mSecurityProfile; }

private slots:
	void checkVncWebsocket();
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqtt(bool restart = false);
	void enableMqtt(bool enabled, bool restart = false);
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

	DaemonToolsService *mFlashMq = nullptr;
	DaemonToolsService *mMqttRpc = nullptr;

	VrmTunnelSetup *mTunnelSetup;

	DaemonToolsService *mVncWebsocket = nullptr;
	VeQItem *mVncEnabled = nullptr;
};
