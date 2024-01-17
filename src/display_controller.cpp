#include "display_controller.hpp"

DisplayController::DisplayController(VeQItemSettings *settings, QObject *parent)
	: QObject{parent},
	  mSettings(settings)
{
	// Read current settings from system
	mMachineRuntimeDir.setPath("/etc/venus");
	mBacklightDevice = getFeature("backlight_device");
	mMaxBrightness = readIntFromFile(mBacklightDevice + "/max_brightness", 100);
	mAutoBrightness = readIntFromFile(mBacklightDevice + "/auto_brightness", -1);

	// Create dbus settings. Do not create settings when capabilities do not exist.
	if(getHasBacklight()) {
		VeQItem *item = settings->add("Gui/Brightness", mMaxBrightness, 1, mMaxBrightness);
		item->getValueAndChanges(this, SLOT(brightnessSettingChanged(QVariant)));
	}
	if(getHasAutoBrightness()) {
		VeQItem *item = settings->add("Gui/AutoBrightness", 1, 0, 1);
		item->getValueAndChanges(this, SLOT(autoBrightnessSettingChanged(QVariant)));
	}
}

void DisplayController::brightnessSettingChanged(QVariant var)
{
	if (var.isValid()) {
		setBrightness(var.toInt());
	}
}

void DisplayController::autoBrightnessSettingChanged(QVariant var)
{
	if (var.isValid())
		setAutoBrightness(var.toBool());
}

void DisplayController::setBrightness(int brightness)
{
	if (!getHasBacklight())
		return;

	if (brightness == mBrightness)
		return;

	mBrightness = brightness;

	// Convert to kernel brightness range
	writeToFile(mBacklightDevice + "/brightness", brightness);
}

void DisplayController::setAutoBrightness(bool enable)
{
	if (!getHasAutoBrightness())
		return;

	if (enable == mAutoBrightness)
		return;

	mAutoBrightness = enable;
	writeToFile(mBacklightDevice + "/auto_brightness", enable);
}

QStringList DisplayController::getFeatureList(QString const &name, bool lines)
{
	QStringList ret;
	QFile file(mMachineRuntimeDir.filePath(name));

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return ret;

	QString line;
	while (!file.atEnd()) {
		line = file.readLine();
		if (lines) {
			line = line.trimmed();
			if (!line.isEmpty())
				ret.append(line);
		} else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
			ret.append(line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts));
#else
			ret.append(line.split(QRegExp("\\s+"), QString::SkipEmptyParts));
#endif
		}
	}

	return ret;
}

QString DisplayController::getFeature(QString const &name, bool optional)
{
	QStringList list = getFeatureList(name);

	if (!optional && list.count() != 1) {
		qCritical() << "required machine feature " + name + " does not exist";
		exit(EXIT_FAILURE);
	}

	return (list.count() >= 1 ? list[0] : QString());
}

int DisplayController::readIntFromFile(QString const &name, int def)
	{
		QFile file(name);
		bool ok;

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return def;

		QString line = file.readLine();
		int val = line.trimmed().toInt(&ok, 0);
		if (!ok)
			return def;

		return val;
	}

bool DisplayController::writeToFile(QString filename, int value)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;

	QTextStream out(&file);
	out << QString::number(value);
	file.close();

	return true;
}
