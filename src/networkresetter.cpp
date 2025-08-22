#include "networkresetter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include "application.hpp"

void NetworkResetter::initiate()
{
	setWifiHotspotAndBluetooth(mSettings, 0);
	stopAllServices();
}

void NetworkResetter::stopAllServices()
{
	// TODO: the python version stopped venus platform, but that's ourselves. What to do?

	/* These are already stopped by writing 0 the the veitem
	qDebug() << "Stopping /service/hostapd";
	mStopProcesses.emplace_back(this);
	connect(&mStopProcesses.back(), &QProcess::finished, this, &NetworkResetter::onStopperFinished);
	mStopProcesses.back().start("/usr/bin/svc", {"-d", "/service/hostapd"});

	qDebug() << "Stopping /service/vesmart-server";
	mStopProcesses.emplace_back(this);
	connect(&mStopProcesses.back(), &QProcess::finished, this, &NetworkResetter::onStopperFinished);
	mStopProcesses.back().start("/usr/bin/svc", {"-d", "/service/vesmart-server"});
	*/

	qDebug() << "Stopping connman";
	mStopProcesses.emplace_back(this);
	connect(&mStopProcesses.back(), &QProcess::finished, this, &NetworkResetter::onStopperFinished);
	mStopProcesses.back().start("/etc/init.d/connman", {"stop"});

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

	qDebug() << "Starting connman";
	QProcess *connmanStarter = new QProcess(this);
	connect(connmanStarter, &QProcess::finished, this, &NetworkResetter::onConnmanStarting);
	connmanStarter->start("/etc/init.d/connman", {"start"});
}

void NetworkResetter::onConnmanStarting(int exitCode, QProcess::ExitStatus exitStatus)
{
	(void) exitCode; (void)exitStatus;

	QObject *p = sender();
	p->deleteLater();

	// TODO: really exit venus platform?
	qApp->exit();
}

NetworkResetter::NetworkResetter(VeQItemSettings *settings, QObject *parent) : QObject{parent},
	mSettings(settings)
{

}

void NetworkResetter::resetNetwork()
{
	// Blink leds fast for a while so that the user sees a response. The re-enabling of the hotspot and bluetooth is fast, which would
	// otherwise stop the fast blinking again, so hence the delay.
	makeLEDsIndicateReset();
	QTimer::singleShot(1500, this, &NetworkResetter::initiate);
}
