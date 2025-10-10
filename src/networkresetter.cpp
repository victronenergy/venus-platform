#include "networkresetter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <unistd.h>
#include <fcntl.h>
#include "application.hpp"

void NetworkResetter::initiate()
{
	setWifiHotspotAndBluetooth(mSettings, 0);
	stopRelevantServices();
}

void NetworkResetter::stopRelevantServices()
{
	qDebug() << "Stopping connman";
	mStopProcesses.emplace_back(this);
	connect(&mStopProcesses.back(), &QProcess::finished, this, &NetworkResetter::onStopperFinished);
	mStopProcesses.back().start("/etc/init.d/connman", {"stop"}); // TODO: check if this commands wait until it's stopped.

	/*
	 * NOTE: If there are ever services that are stopped with svc, check them with serviceRunning, like we do
	 * elsewhere, because it's asynchronous.
	 */
}

void NetworkResetter::makeLEDsIndicateReset()
{
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

		auto defaultVal = item->itemProperty("defaultValue");
		qDebug() << "Setting " << path << "to default value."; // Not logging value, which could be sensitive.
		item->setValue(defaultVal);
	}
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
	qDebug() << "Stopping localsettings";
	QProcess *settingsStopper = new QProcess(this);
	settingsStopper->start("svc", {"-d", "/service/localsettings/"});

	mWaitForLocalSettingsEndPoint = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	mLocalSettingsChecker.start();
}

void NetworkResetter::onStopperFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	(void) exitCode; (void)exitStatus;

	const bool allDone = std::all_of(mStopProcesses.begin(), mStopProcesses.end(), [](QProcess &p) {
		return p.state() == QProcess::ProcessState::NotRunning;
	});

	if (!allDone)
		return;

	deletePaths();
	setSettingsToDefault();
	initiateSettingsShutdownAndReboot(); // TODO: this needs to be done after the setting to default was responded to.
}

void NetworkResetter::onlocalSettingsCheckerTimeout()
{
	bool ok = false;
	const bool running = serviceRunning("localsettings", &ok);

	if (!ok) {
		qWarning() << "Failure to check running status of localSettings. Falling back to timeout before rebooting.";
	}

	if (running) {
		qDebug() << "LocalSettings still running";
	}

	if ((ok && !running) || std::chrono::steady_clock::now() > mWaitForLocalSettingsEndPoint) {
		mLocalSettingsChecker.stop();

		qDebug() << "LocalSettings stopped. Rebooting";
		QProcess *settingsStopper = new QProcess(this);
		settingsStopper->start("/sbin/reboot");
	}
}

NetworkResetter::NetworkResetter(VeQItemSettings *settings, QObject *parent) : QObject{parent},
	mSettings(settings)
{
	mLocalSettingsChecker.setInterval(200);
	connect(&mLocalSettingsChecker, &QTimer::timeout, this, &NetworkResetter::onlocalSettingsCheckerTimeout);
}

void NetworkResetter::resetNetwork()
{
	// Blink leds fast for a while so that the user sees a response. The re-enabling of the hotspot and bluetooth is fast, which would
	// otherwise stop the fast blinking again, so hence the delay.
	makeLEDsIndicateReset();
	QTimer::singleShot(1500, this, &NetworkResetter::initiate);

	// TODO: do we need a long-ish fallback reboot?
}
