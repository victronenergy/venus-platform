#include <fcntl.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>

#include <veutil/qt/daemontools_service.hpp>

#include "network_reset.hpp"
#include "utils.hpp"

using namespace std::chrono;

void NetworkReset::initiate()
{
	stopRelevantServices();
	setWifiHotspotAndBluetooth(mSettings, 0);
	deletePaths();
	setSettingsToDefault();
}

void NetworkReset::stopRelevantServices()
{
	QProcess p;

	qDebug() << "Stopping connman";
	p.start("/etc/init.d/connman", {"stop"});
	p.waitForFinished();

	qDebug() << "Stopping venus-platform";
	DaemonToolsService platform("/service/venus-platform/");
	platform.stop();

	if (!platform.waitTillDown(10s)) {
		qDebug() << "Timeout waiting for Venus-platform, force-killing";
		p.start("killall", {"-9", "venus-platform"});
		p.waitForFinished();
	}
}

void NetworkReset::makeLedsIndicateReset()
{
	qDebug() << "Starting LED feedback";

	std::unordered_map<QString, QString> ledValues;

	// Cerbo GX
	ledValues["status-green"] = "blink-fast";
	ledValues["status-orange"] = "none";
	ledValues["bluetooth"] = "none";

	// Venus GX / Octo GX
	ledValues["vecape:green:ve0"] = "default-on";

	for (auto &pair : ledValues) {
		qDebug() << "Setting led" << pair.first << "to" << pair.second;

		const QString path = QString("/sys/class/leds/%1/trigger").arg(pair.first);

		QFile f(path);

		if (!f.exists())
			continue;

		f.open(QIODevice::WriteOnly);
		f.write(pair.second.toUtf8());
	}
}

void NetworkReset::setSettingsToDefault()
{
	const QStringList settingPaths{"Settings/Ble/Service/Pincode", "Settings/Services/AccessPoint",
									"Settings/Services/Bluetooth", "/Settings/System/SecurityProfile",
									"/Settings/Services/AccessPointPassword"};

	for (const QString &path : settingPaths) {

		// NOTE: This is somewhat problematic, there is nothing which assures the GetItems call
		// to localsettings has completed yet, besides the 1.5 second led delay, which will _often_
		// work, but it is quite fragile. Remove the led delay and who knows what happens.
		// There is actually things can't be done blocking here. There is no any other code running,
		// but the QItem are not suitable for that, at least not for the moment.
		VeQItem *item = mSettings->root()->itemGet(path);

		if (!item) {
			qDebug() << "not an item" << path;
			continue;
		}

		connect(item, &VeQItem::stateChanged, this, &NetworkReset::defaultValueSetStateChanged);

		QVariant defaultVal = item->itemProperty("defaultValue");
		qDebug() << "Setting " << path << "to default value."; // Not logging value, which could be sensitive.
		// NOTE: if the same value is set, this will not go to a storing state if the value is
		// the same!
		item->setValue(defaultVal);

		VeQItem::State itemState = item->getState();

		if (itemState == VeQItem::State::Storing)
			mDefaultValueSetCounter++;
	}

	mSetDefaultTimeoutTimer.start();

	qDebug() << "Waiting for acknowldegement of setting default values, or timeout of" << mSetDefaultTimeoutTimer.interval() << "ms";
}

void NetworkReset::deletePaths()
{
	const QStringList deletePaths{"/data/conf/vncpassword.txt", "/data/var/lib/bluetooth", "/data/var/lib/connman"};

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
	for (const QString &dir : syncDirs) {
		int fddir = open(dir.toLatin1().data(), O_RDONLY);
		if (fddir < 0)
			continue;
		fsync(fddir);
		close(fddir);
	}
}

void NetworkReset::initiateSettingsShutdownAndReboot()
{
	// The localsettings has an exit handler that properly saves and fsyncs settings.
	DaemonToolsService settings("/service/localsettings/");
	settings.stop();
	if (!settings.waitTillDown(10s))
		qWarning() << "localsettings did not exit? Rebooting anyway.";

	// Reboot is required, to make localsettings correct things.
	// Refactoring things to avoid that makes things complicated.
	{
		qDebug() << "Rebooting";
		QProcess p;
		p.start("/sbin/reboot");
		p.waitForFinished();
	}

	QTimer::singleShot(0, &QCoreApplication::quit);
}

void NetworkReset::defaultValueSetStateChanged(VeQItem::State state)
{
	/*
	 * Note that this mechanism doesn't detect setValue commands to the same value.
	 * That's why a timeout is used.
	 */

	// NOTE: Setting the same value won't increment mDefaultValueSetCounter.
	// Above comment at least _should_ be incorrect.

	// NOTE: this will never be called at the moment if the remove declined
	// the setValue. The signal setValueResult will be emitted, but to handle
	// that makes things far too complicated

	if (state != VeQItem::Synchronized)
		return;

	mDefaultValueSetCounter--;

	if (mDefaultValueSetCounter <= 0) {
		mSetDefaultTimeoutTimer.stop();
		initiateSettingsShutdownAndReboot();
	}
}

NetworkReset::NetworkReset(VeQItemSettings *settings, QObject *parent) : QObject{parent},
	mSettings(settings)
{
	mSetDefaultTimeoutTimer.setInterval(7000);
	mSetDefaultTimeoutTimer.setSingleShot(true);
	connect(&mSetDefaultTimeoutTimer, &QTimer::timeout, this, &NetworkReset::initiateSettingsShutdownAndReboot);
}

void NetworkReset::reset()
{
	// Blink leds fast for a while so that the user sees a response. The
	// re-enabling of the hotspot and bluetooth is fast, which would
	// otherwise stop the fast blinking again, so hence the delay.
	makeLedsIndicateReset();
	QTimer::singleShot(1500, this, &NetworkReset::initiate);

	// TODO: do we need a long-ish fallback reboot?
}
