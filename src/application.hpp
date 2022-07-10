#include <QCoreApplication>

#include <velib/qt/ve_qitems_dbus.hpp>
#include <velib/qt/canbus_interfaces.hpp>

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);

protected slots:
	void onLocalSettingsStateChanged(VeQItem *item);
	void onLocalSettingsTimeout();
	void onCanInterfacesChanged();

private:
	void manageDaemontoolsServices();
	void init();

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
	CanInterfaceMonitor *mCanInterfaceMonitor;

	VeQItem *mService;
};
