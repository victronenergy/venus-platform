#include "networkresetter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include "utils.hpp"

void NetworkResetter::initiate()
{
	stopRelevantServices();
	setWifiHotspotAndBluetooth(mSettings, 0);
	deletePaths();
	setSettingsToDefault();
}

void NetworkResetter::stopRelevantServices()
{
	{
		qDebug() << "Stopping connman";
		QProcess p;
		p.start("/etc/init.d/connman", {"stop"});
		p.waitForFinished();
	}

	{
		qDebug() << "Stopping venus-platform";
		QProcess p;
		p.start("svc", {"-d" , "/service/venus-platform/"});
		p.waitForFinished();
	}

	const bool platform_stopped = stopServiceAndWaitForExit("venus-platform");

	if (!platform_stopped)
	{
		qDebug() << "Timeout waiting for Venus-platform, force-killing";
		QProcess p;
		p.start("killall", {"-9", "venus-platform"});
		p.waitForFinished();
	}
}

void NetworkResetter::makeLEDsIndicateReset()
{
	qDebug() << "Starting LED feedback";

	std::unordered_map<QString, QString> ledValues;

	// Cerbo GX
	ledValues["status-green"] = "blink-fast";
	ledValues["status-orange"] = "none";
	ledValues["bluetooth"] = "none";

	// Venus GX / Octo GX
	ledValues["vecape:green:ve0"] = "default-on";

	for (auto &pair : ledValues)
	{
		qDebug() << "Setting led" << pair.first << "to" << pair.second;

		const QString path = QString("/sys/class/leds/%1/trigger").arg(pair.first);

		QFile f(path);

		if (!f.exists())
			continue;

		f.open(QIODevice::WriteOnly);
		f.write(pair.second.toUtf8());
	}
}

void NetworkResetter::setSettingsToDefault()
{
	const QStringList settingPaths({"Settings/Ble/Service/Pincode", "Settings/Services/AccessPoint", "Settings/Services/Bluetooth"});

	for (const QString &path : settingPaths)
	{
		VeQItem *item = mSettings->root()->itemGet(path);

		if (!item)
			continue;

		connect(item, &VeQItem::stateChanged, this, &NetworkResetter::defaultValueSetStateChanged);

		auto defaultVal = item->itemProperty("defaultValue");
		qDebug() << "Setting " << path << "to default value."; // Not logging value, which could be sensitive.
		item->setValue(defaultVal);

		auto itemState = item->getState();

		if (itemState == VeQItem::State::Storing)
			mDefaultValueSetCounter++;
	}

	mSetDefaultTimeoutTimer.start();

	qDebug() << "Waiting for acknowldegement of setting default values, or timeout of" << mSetDefaultTimeoutTimer.interval() << "ms";
}

void NetworkResetter::deletePaths()
{
	const QStringList deletePaths({"/data/conf/vncpassword.txt", "/data/var/lib/bluetooth", "/data/var/lib/connman" });

	for (const QString &s : deletePaths) {
		qDebug() << "Removing" << s;

		{
			QDir dir(s);

			if (dir.exists()) {
				if (!dir.removeRecursively())
					qCritical() << "Deleting" << s << "failed";
				continue;
			}
		}

		{
			QFile f(s);

			if (f.exists())	{
				if (!f.remove())
					qCritical() << "Deleting" << s << "failed";
			}
		}
	}

	const QStringList syncDirs({"/data/conf/", "/data/var/lib/"});
	for (const QString &dir : syncDirs)
	{
		int fddir = open(dir.toLatin1().data(), O_RDONLY);
		if (fddir < 0)
			continue;
		fsync(fddir);
		close(fddir);
	}
}

void NetworkResetter::initiateSettingsShutdownAndReboot()
{
	// The localsettings has an exit handler that properly saves and fsyncs settings.
	{
		QProcess p;
		p.start("svc", {"-d" , "/service/localsettings/"});
		p.waitForFinished();
	}

	const bool localsettings_stopped = stopServiceAndWaitForExit("localsettings");

	if (!localsettings_stopped) {
		qWarning() << "localsettings did not exit? Rebooting anyway.";
	}

	// Reboot is required, to make localsettings correct things. Refactoring things to avoid that makes things complicated.
	{
		qDebug() << "Rebooting";
		QProcess p;
		p.start("/sbin/reboot");
		p.waitForFinished();
	}

	QTimer::singleShot(0, &QCoreApplication::quit);
}

void NetworkResetter::defaultValueSetStateChanged(VeQItem::State state)
{
	/*
	 * Note that this mechanism doesn't detect setValue commands to the same value. That's
	 * why we also use a timeout.
	 */

	if (state != VeQItem::Synchronized)
		return;

	mDefaultValueSetCounter--;

	if (mDefaultValueSetCounter <= 0)
	{
		mSetDefaultTimeoutTimer.stop();
		initiateSettingsShutdownAndReboot();
	}
}


NetworkResetter::NetworkResetter(VeQItemSettings *settings, QObject *parent) : QObject{parent},
	mSettings(settings)
{
	mSetDefaultTimeoutTimer.setInterval(7000);
	mSetDefaultTimeoutTimer.setSingleShot(true);
	connect(&mSetDefaultTimeoutTimer, &QTimer::timeout, this, &NetworkResetter::initiateSettingsShutdownAndReboot);
}

void NetworkResetter::resetNetwork()
{
	// Blink leds fast for a while so that the user sees a response. The re-enabling of the hotspot and bluetooth is fast, which would
	// otherwise stop the fast blinking again, so hence the delay.
	makeLEDsIndicateReset();
	QTimer::singleShot(1500, this, &NetworkResetter::initiate);

	// TODO: do we need a long-ish fallback reboot?
}
