#pragma once

#include <QFileSystemWatcher>
#include <QAtomicInt>
#include <QTimer>
#include <veutil/qt/ve_qitem.hpp>

// The LedController acts as a latch between /run/leds/* and /sys/class/leds/*.
// When the dbus setting "LEDs/Enable" is 1, the files from /run/leds/* are mirrored to /sys/class/leds/*.
// When the dbus setting is 0, the LEDs are disabled.
class LedController : public QObject
{
	Q_OBJECT

public:
	LedController(VeQItemSettings *settings, QObject *parent);
	static bool hasLeds();

private slots:
	void dbusSettingChanged();
	void ledSettingChanged(QVariant var);
	void timerExpired();
	void updateLed(const QString &path);

private:
	void touchAndClearFile(QString const &fileName);
	void syncLeds(bool ledsOn);
	void disableLed(const QString &path);

	QFileSystemWatcher mLedWatcher;
	QAtomicInt mLedsEnabled;
	QAtomicInt mTimerExpired;
	QTimer *mTimer;
};
