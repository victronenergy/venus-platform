#pragma once

#include <QCoreApplication>
#include <veutil/qt/ve_qitems_dbus.hpp>

/**
 * A separate app, to be compiled as separate executable, for resetting network with the GX button.
 * The reason is that venus-platform needs to be off while it's happening, because it reacts to stuff,
 * especially when we want to do a controlled exit of localsettings (which makes venus-platform quit).
 */
class NetworkResetterApp : public QCoreApplication
{
	Q_OBJECT

	VeQItemSettings *mSettings = nullptr;
	VeQItem *mServices = nullptr;


public:
	NetworkResetterApp(int &argc, char **argv);
	void start();
};
