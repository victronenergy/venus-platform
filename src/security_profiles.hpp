#pragma once

#include <QObject>
#include <QVariant>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

class SecurityProfiles : public QObject {
	Q_OBJECT

public:
	SecurityProfiles(VeQItem *pltService, VeQItemSettings *settings, QObject *parent = nullptr);

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
};
