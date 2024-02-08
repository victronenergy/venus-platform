#include <QDir>
#include <QFile>
#include <QTextStream>
#include "led_controller.hpp"

#define SRC_DIR(x)	"/run/leds/"+x+"/"
#define DEST_DIR(x)	"/sys/class/leds/"+x+"/"

static const QString leds[3] = {QString::fromUtf8("bluetooth"), QString::fromUtf8("status-orange"), QString::fromUtf8("status-green")};

bool LedController::hasLeds()
{
	for (auto &l: leds) {
		if (QDir(DEST_DIR(l)).exists())
			return true;
	}
	return false;
}

LedController::LedController(QObject *parent) :
	QObject(parent), mLedsEnabled(1), mTimerExpired(0)
{
	// Create files if not exist and clear them to prevent synching incorrect LED state initially.
	for (auto &l: leds) {
		touchAndClearFile(SRC_DIR(l) + "trigger");
		touchAndClearFile(SRC_DIR(l) + "brightness");
	}

	// Add paths to watch to the watcher
	for (auto &l: leds) {
		mLedWatcher.addPath(SRC_DIR(l) + "trigger");
		mLedWatcher.addPath(SRC_DIR(l) + "brightness");
	}

	// Monitor the "trigger" files of all the LED directories (/sys/class/leds/*)
	connect(&mLedWatcher, SIGNAL(fileChanged(QString)), SLOT(updateLed(QString)));

	mTimer = new QTimer(this);
	mTimer->setSingleShot(true);
	connect(mTimer, SIGNAL(timeout()), this, SLOT(timerExpired()));
	mTimer->start(10000);
	syncLeds(true);
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
		for(auto &l: leds) {
			updateLed(SRC_DIR(l) + "trigger");
			updateLed(SRC_DIR(l) + "brightness");
		}
	} else {
		for (auto &l: leds)
			disableLed(DEST_DIR(l));
	}
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

		QFile srcFile(src);
		QFile destFile(dest);

		if (!srcFile.exists()) {
			// Recreate src file when it doesn't exist.
			for (auto &l: leds) {
				touchAndClearFile(SRC_DIR(l) + "trigger");
				touchAndClearFile(SRC_DIR(l) + "brightness");
			}

			// No point in syncing the files when the src files are just recreated (and empty)
			return;
		}

		srcFile.open(QIODevice::ReadOnly);

		// Destination file (/sys/class/) should exist.
		destFile.open(QIODevice::WriteOnly);
		destFile.write(srcFile.readAll());
	}
}

void LedController::timerExpired(void)
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
	QFile trigFile(path + "trigger");
	QFile brightnessFile(path + "brightness");

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
	dir.mkpath(dir.dirName());
	QFile file(fileName);
	if (!file.exists())
		file.open(QIODevice::ReadWrite);
	file.resize(0);
}
