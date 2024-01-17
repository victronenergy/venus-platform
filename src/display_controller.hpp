#pragma once

#include <veutil/qt/ve_qitems_dbus.hpp>

class DisplayController : public QObject
{
	Q_OBJECT
public:
	DisplayController(VeQItemSettings *settings, QObject *parent = nullptr);

	bool getHasBacklight() { return !mBacklightDevice.isEmpty(); }
	int getBrightness(void){return mBrightness;}
	bool getHasAutoBrightness(void){return mAutoBrightness != -1;}

private slots:
	void brightnessSettingChanged(QVariant);
	void autoBrightnessSettingChanged(QVariant);

private:
	QStringList getFeatureList(const QString &name, bool lines = false);
	QString getFeature(QString const &name, bool optional = true);
	int readIntFromFile(QString const &name, int def);
	bool writeToFile(QString filename, int value);
	void setBrightness(int percentage);
	void setAutoBrightness(bool enable);

	VeQItemSettings *mSettings;
	QDir mMachineRuntimeDir;
	QString mBacklightDevice;
	int mBrightness;
	int mMaxBrightness;
	int mAutoBrightness;
};
