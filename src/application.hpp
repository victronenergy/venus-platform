#include <QCoreApplication>

#include <velib/qt/ve_qitems_dbus.hpp>

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);

protected slots:
	void onLocalSettingsStateChanged(VeQItem *item);
	void onLocalSettingsTimeout();
	void remoteSupportChanged(VeQItem *item, QVariant var);
	void sshLocalChanged(VeQItem *item, QVariant var);

private:
	void manageDaemontoolsServices();
	void init();

	VeQItemSettings *mSettings;
	VeQItem *mServices;
	QTimer mLocalSettingsTimeout;
};
