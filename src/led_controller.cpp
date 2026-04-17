#include <QDir>
#include <QFile>
#include <QTextStream>
#include "led_controller.hpp"

static QString srcDir(QString const &led, QString const file = "")
{
	return QDir("/run/leds").filePath(led) + (file.isEmpty() ? "" : QDir::separator() + file);
}

static QString destDir(QString const &led, QString const file = "")
{
	return QDir("/sys/class/leds").filePath(led) + (file.isEmpty() ? "" : QDir::separator() + file);
}

QStringList leds = {
	QString::fromUtf8("bluetooth"),
	QString::fromUtf8("status-orange"),
	QString::fromUtf8("status-green")
};

bool LedController::hasLeds()
{
	QMutableStringListIterator i(leds);
	while (i.hasNext()) {
		if (!QDir(destDir(i.next())).exists())
			i.remove();
	}
	return leds.count() != 0;
}

LedController::LedController(VeQItemSettings *settings, QObject *parent) :
	QObject(parent),
	mLedsEnabled(1),
	mTimerExpired(0)
{
	// Create files if not exist and clear them to prevent synching incorrect LED state initially.
	for (auto const &led: leds) {
		touchAndClearFile(srcDir(led, "trigger"));
		touchAndClearFile(srcDir(led, "brightness"));
	}

	// Add paths to watch to the watcher
	for (auto const &led: leds) {
		mLedWatcher.addPath(srcDir(led, "trigger"));
		mLedWatcher.addPath(srcDir(led, "brightness"));
	}

	// Monitor the "trigger" files of all the LED directories (/sys/class/leds/*)
	connect(&mLedWatcher, &QFileSystemWatcher::fileChanged, this, &LedController::updateLed);

	mTimer = new QTimer(this);
	mTimer->setSingleShot(true);
	connect(mTimer, &QTimer::timeout, this, &LedController::timerExpired);
	mTimer->start(10000);
	syncLeds(true);

	VeQItem *ledEnableSetting = settings->root()->itemGetOrCreate("Settings/LEDs/Enable");
	ledEnableSetting->getValueAndChanges(this, &LedController::ledSettingChanged);

	VeQItem *bluetoothSetting = settings->root()->itemGet("Settings/Services/Bluetooth");
	if (bluetoothSetting)
		bluetoothSetting->getValueAndChanges(this, &LedController::dbusSettingChanged);

	VeQItem *accessPointSetting = settings->root()->itemGet("Settings/Services/AccessPoint");
	if (accessPointSetting)
		accessPointSetting->getValueAndChanges(this, &LedController::dbusSettingChanged);
}

void LedController::ledSettingChanged(QVariant var)
{
	if (!var.isValid())
		return;

	mLedsEnabled = var.toInt();

	if (!mTimerExpired)
		return;

	syncLeds(mLedsEnabled);
}

void LedController::dbusSettingChanged(void)
{
	mTimer->start(10000);
	mTimerExpired = 0;

	syncLeds(true);
}

void LedController::syncLeds(bool ledsOn)
{
	if (ledsOn) {
		for (QString const &led: leds) {
			if (mForcedLeds.contains(led))
				continue;
			updateLed(srcDir(led, "trigger"));
			updateLed(srcDir(led, "brightness"));
		}
	} else {
		for (QString const &led: leds) {
			if (mForcedLeds.contains(led))
				continue;
			disableLed(destDir(led));
		}
	}
}

bool LedController::forceLed(const QString &ledName, const QString &trigger)
{
	if (!leds.contains(ledName))
		return true;

	if (mForcedLeds.contains(ledName)) {
		qCritical() << ledName << "is already forced to a state";
		return false;
	}

	QFile tiggerFile(destDir(ledName, "trigger"));
	if (!tiggerFile.open(QIODevice::WriteOnly)) {
		qCritical() << "opening" << tiggerFile.fileName() << "failed";
		return false;
	}
	tiggerFile.write(trigger.toUtf8());

	QFile brightnessFile(destDir(ledName, "brightness"));
	if (!brightnessFile.open(QIODevice::WriteOnly)) {
		qCritical() << "opening" << brightnessFile.fileName() << "failed";
		return false;
	}
	brightnessFile.write("1");

	mForcedLeds.append(ledName);

	return true;
}

void LedController::resumeNormalLedBehaviour(const QString &ledName)
{
	mForcedLeds.removeAll(ledName);
	syncLeds(mLedsEnabled);
}

void LedController::updateLed(const QString &src)
{
	// Qt note: https://doc.qt.io/qt-6/qfilesystemwatcher.html#fileChanged
	// As a safety measure, many applications save an open file by writing a new file and then deleting the old one.
	// In your slot function, you can check watcher.files().contains(path).
	// If it returns false, check whether the file still exists and then call addPath() to continue watching it.
	if (!mLedWatcher.files().contains(src))
		mLedWatcher.addPath(src);

	if (!mTimerExpired || mLedsEnabled)	{
		QString dest = src;
		dest.replace(0, 4, "/sys/class");

		QStringList parts = src.split("/", Qt::SkipEmptyParts);
		if (parts.length() == 4) {
			QString led = parts[3];
			if (mForcedLeds.contains(led))
				return;
		}

		QFile srcFile(src);
		QFile destFile(dest);

		if (!srcFile.exists()) {
			// Recreate src file when it doesn't exist.
			for (auto const &led: leds) {
				touchAndClearFile(srcDir(led, "trigger"));
				touchAndClearFile(srcDir(led, "brightness"));
			}

			// No point in syncing the files when the src files are just recreated (and empty)
			return;
		}

		srcFile.open(QIODevice::ReadOnly);

		// Destination file (/sys/class/) should exist.
		destFile.open(QIODevice::WriteOnly);
		QByteArray bytes = srcFile.readAll();

		// This is a bit weird code, but it is possible to get here, if the leds was
		// not in used, forced to be used for something else and there after restoring
		// original behaviour, so the files of touchAndClearFile are seen. Empty strings
		// are not a valid sysfs value, so the write below would always fail.
		if (QString(bytes) == "") {
			if (QFileInfo(src).fileName() != "trigger")
				return;
			bytes = "none";
		}

		destFile.write(bytes);
	}
}

void LedController::timerExpired()
{
	mTimerExpired = 1;
	// Only sync leds when they should be disabled after timeout.
	// This prevents unwanting irregular LED blinking when the led files
	// are rewritten with the same contents (in case of LEDs being enabled).
	if (!mLedsEnabled)
		syncLeds(false);
}

void LedController::disableLed(const QString &path)
{
	QFile trigFile(path + QDir::separator() + "trigger");
	QFile brightnessFile(path + QDir::separator() + "brightness");

	trigFile.open(QIODevice::WriteOnly);
	brightnessFile.open(QIODevice::WriteOnly);

	QTextStream outstream(&trigFile);
	outstream << "none";

	outstream.setDevice(&brightnessFile);
	outstream << "0";
}

void LedController::touchAndClearFile(QString const &fileName)
{
	QDir dir = QFileInfo(fileName).absoluteDir();
	if (!dir.exists())
		dir.mkpath(".");
	QFile file(fileName);
	if (!file.exists()) {
		file.open(QIODevice::WriteOnly);
		file.resize(0);
	}
}
