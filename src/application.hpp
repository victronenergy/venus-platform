#include <QCoreApplication>

#include <velib/qt/ve_qitems_dbus.hpp>
#include <velib/qt/canbus_monitor.hpp>

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);

	static QProcess *spawn(const QString &cmd, QStringList const &args = QStringList());

protected slots:
	void onLocalSettingsStateChanged(VeQItem *item);
	void onLocalSettingsTimeout();
	void onCanInterfacesChanged();
	void mqttLocalChanged(VeQItem *item, QVariant var);
	void mqttLocalInsecureChanged(VeQItem *item, QVariant var);
	void mk3UpdateAllowedChanged(VeQItem *item, QVariant var);

private:
	void manageDaemontoolsServices();
	void mqttCheckLocalInsecure();
	void init();

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;

	VeQItem *mMqttLocalInsecure = nullptr;
	VeQItem *mMqttLocal = nullptr;
	bool mMqttLocalWasValid = false;

	VeQItem *mService;
};
