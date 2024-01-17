#pragma once

#include <veutil/qt/ve_qitems_dbus.hpp>

class DisplayController : public QObject
{
	Q_OBJECT

public:
	explicit DisplayController(VeQItemSettings *settings, QObject *parent = nullptr);

private slots:
	void onBrightnessSettingChanged(QVariant);
	void onAutoBrightnessSettingChanged(QVariant);

private:
	void setBrightness(int brightness);
	void setAutoBrightness(bool enable);

	QString mBacklightDevice;
};
