#include <QCoreApplication>

#include <veutil/qt/ve_qitems_dbus.hpp>
#include <veutil/qt/canbus_monitor.hpp>

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

protected slots:
	void onLocalSettingsStateChanged(VeQItem::State state);
	void onLocalSettingsTimeout();
	void onCanInterfacesChanged();
	void mqttLocalChanged(QVariant var);
	void mqttLocalInsecureChanged(QVariant var);
	void mk3UpdateAllowedChanged(QVariant var);
	void demoSettingChanged(QVariant var);

private:
	void manageDaemontoolsServices();
	void mqttCheckLocalInsecure();
	void init();

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;
	Updater *mUpdater;

	VeQItem *mMqttLocalInsecure = nullptr;
	VeQItem *mMqttLocal = nullptr;
	bool mMqttLocalWasValid = false;

	VeQItem *mService;
};
