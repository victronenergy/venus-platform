#pragma once

#include <QCoreApplication>
#include <QTranslator>

#include <veutil/qt/ve_qitems_dbus.hpp>
#include <veutil/qt/canbus_monitor.hpp>

#include "alarm_item.hpp"
#include "buzzer.hpp"
#include "display_controller.hpp"
#include "led_controller.hpp"
#include "network_controller.hpp"
#include "notifications.hpp"
#include "relay.hpp"
#include "updater.hpp"
#include "venus_services.hpp"
#include "vebus_backup.hpp"

bool serviceExists(QString const &svc);
QStringList getFeatureList(const QString &name, bool lines = false);
QString getFeature(QString const &name, bool optional = true);
int readIntFromFile(QString const &name, int def);
QString readFirstLineFromFile(QString const &name, QString def = QString());
bool writeIntToFile(QString filename, int value);
QString getSecureRandomString(int length);
bool writeFileAtomically(const QString &path, const QString &contents);

// Since this class needs to be in a header file for moc, just place it
// here for now...
class VeQItemReboot : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemReboot() : VeQItemAction() {}
	int setValue(const QVariant &value) override;

private slots:
	void doReboot();
};

class VeQItemNodeRedReset : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemNodeRedReset(DaemonToolsService *nodeRed, VeQItem *nodeRedMode);
	int setValue(const QVariant &value) override;

private:
	DaemonToolsService *mNodeRed;
	VeQItem *mNodeRedMode;
};

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);

	static QProcess *spawn(const QString &cmd, QStringList const &args = QStringList());
	static int run(QString const &cmd, const QStringList &args = QStringList());
	static bool setRootPassword(QString password);
	static void invalidateAuthenticatedSessions();
	bool silenceBuzzer();

	static int runningGuiVersion() {
		if (!mRunningGui.isValid())
			return -1;
		return mRunningGui.toInt();
	}

signals:
	void languageChanged();
	void runningGuiVersionChanged();

protected slots:
	void onAlarmChanged(QVariant var);
	void onCanInterfacesChanged();
	void onDemoSettingChanged(QVariant var);
	void onEvccSettingChanged(QVariant var);
	void onLanguageChanged(QVariant var);
	void onLocalSettingsStateChanged(VeQItem::State state);
	void onLocalSettingsTimeout();
	void onMk3UpdateAllowedChanged(QVariant var);
	void onAccessPointPasswordChanged(QVariant var);
	void onRunningGuiVersionObtained(QVariant var);
	void onRelaySettingChanged(QVariant var);
	void onServiceAdded(VeQItem *var);
	void onGensetStateChanged(VeQItem::State state);
	void onBatteryProductIdChanged(QVariant var);
	void checkDataPartitionUsedSpace();

private:
	void createItemsForFlashmq();
	void manageDaemontoolsServices();
	void loadTranslation();
	void initDaemonStartupConditions(VeQItem *service);
	void manageGeneratorStartStop();
	void manageParallelBms();
	void init();
	void start();
	void setRunningGui(QVariant version);

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;
	Updater *mUpdater;
	DisplayController *mDisplayController;
	NetworkController *mNetworkController;

	QString mLanguage;
	QTranslator mTranslator;

	VeQItem *mService;
	VenusServices *mVenusServices;

	Notifications *mNotifications;
	Buzzer *mBuzzer;
	AlarmBusitems *mAlarmBusitems;
	VeQItem *mAudibleAlarm;
	VeQItem *mAlarm;
	Relay *mRelay;

	VebusBackupServiceRegistrator *mVebusBackup;

	DaemonToolsService *mGuiSwitcher = nullptr;
	bool mOnScreenGuiv2Supported;
	QVariant mRunningGuiSetting;
	static QVariant mRunningGui;
	VeQItem *mRunningGuiItem;

	DaemonToolsService *mGeneratorStarter = nullptr;
	DaemonToolsService *mParallelBmsStarter = nullptr;
	QList<QString> mGeneratorStarterConditions;
	QList<QString> mParallelBmsConditions;

	DaemonToolsService *mNodeRed = nullptr;
};
