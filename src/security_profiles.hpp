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
		int ret = check();
		if (ret < 0)
			return ret;
		return VeQItemAction::setValue(value);
	}

	int check() {
		if (mProc)
			return -1;

		mProc = new QProcess();
		connect(mProc, SIGNAL(finished(int)), this, SLOT(onFinished()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
		connect(mProc, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(onErrorOccurred(QProcess::ProcessError)));
#else
		connect(mProc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onErrorOccurred(QProcess::ProcessError)));
#endif
		qDebug() << "[MqttBridgeRegistrar]" << "registering";
		mProc->start("mosquitto_bridge_registrator.py");

		return 0;
	}

signals:
	void bridgeConfigChanged();

private slots:
	void onFinished() {
		qDebug() << "[MqttBridgeRegistrar]" << "done";
		if (mProc->exitCode() == 100)
			emit bridgeConfigChanged();
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
	void onBridgeConfigChanged();
	void onMqttAccessChanged(QVariant const &var);
	void onSecurityProfileChanged(QVariant const &var);
	void onVrmPortalChange(QVariant const &var);

private:
	void checkMqttOnLan();
	void enableMqttBridge(bool configChanged = false);
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

	VeQItemMqttBridgeRegistrar *mMqttBridgeRegistrar;
};
