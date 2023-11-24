#include <QCoreApplication>
#include <QTranslator>

#include <veutil/qt/ve_qitems_dbus.hpp>
#include <veutil/qt/canbus_monitor.hpp>
#include <src/dbus_services.h>
#include <src/notification_center.h>
#include <src/alarmbusitem.h>
#include <src/buzzer.h>
#include <src/relay.h>

#include "updater.hpp"

QStringList getFeatureList(const QString &name, bool lines = false);
QString getFeature(QString const &name, bool optional = true);

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
	void translationLoaded();

protected slots:
	void onLocalSettingsStateChanged(VeQItem::State state);
	void onLocalSettingsTimeout();
	void onCanInterfacesChanged();
	void mk3UpdateAllowedChanged(QVariant var);
	void demoSettingChanged(QVariant var);
	void alarmChanged(QVariant var);
	void languageChanged(QVariant var);

private:
	void manageDaemontoolsServices();
	void init();
	VeQItem *getServices() { return mServices; }

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;
	Updater *mUpdater;

	VeQItem *mService;
	DBusServices *mDBusServices;
	NotificationCenter *mNotificationCenter;
	AlarmBusitem *mAlarmBusitem;
	Buzzer *mBuzzer;
	VeQItem *mAudibleAlarm;
	VeQItem *mAlert;
	Relay *mRelay;
	QString mLanguage;
	QTranslator mTranslator;

	void loadTranslation();
};
