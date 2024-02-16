#include <QCoreApplication>
#include <QTranslator>

#include <veutil/qt/ve_qitems_dbus.hpp>
#include <veutil/qt/canbus_monitor.hpp>

#include "alarm_item.hpp"
#include "buzzer.hpp"
#include "display_controller.hpp"
#include "led_controller.hpp"
#include "notifications.hpp"
#include "relay.hpp"
#include "network_controller.h"
#include "updater.hpp"
#include "venus_services.hpp"

QStringList getFeatureList(const QString &name, bool lines = false);
QString getFeature(QString const &name, bool optional = true);
int readIntFromFile(QString const &name, int def);
bool writeIntToFile(QString filename, int value);

// Since this class needs to be in a header file for moc, just place it
// here for now...
class VeQItemReboot : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemReboot() : VeQItemAction() {}
	int setValue(const QVariant &value) override;
};

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);

	static QProcess *spawn(const QString &cmd, QStringList const &args = QStringList());
	bool silenceBuzzer();

signals:
	void languageChanged();

protected slots:
	void onLocalSettingsStateChanged(VeQItem::State state);
	void onLocalSettingsTimeout();
	void onCanInterfacesChanged();
	void mk3UpdateAllowedChanged(QVariant var);
	void demoSettingChanged(QVariant var);
	void onLanguageChanged(QVariant var);
	void alarmChanged(QVariant var);
	void onEvccSettingChanged(QVariant var);

private:
	void manageDaemontoolsServices();
	void loadTranslation();
	void init();
	void start();

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;
	Updater *mUpdater;
	LedController *mLedController;
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
};
