#include "application.hpp"
#include "display_controller.hpp"

DisplayController::DisplayController(VeQItemSettings *settings, QObject *parent)
	: QObject{parent}
{
	// Read current settings from system
	mBacklightDevice = getFeature("backlight_device");

	VeQItem *item = settings->root()->itemGet("Settings/Gui/Brightness");
	if (item)
		item->getValueAndChanges(this, SLOT(onBrightnessSettingChanged(QVariant)));

	item = settings->root()->itemGet("Settings/Gui/AutoBrightness");
	if (item)
		item->getValueAndChanges(this, SLOT(onAutoBrightnessSettingChanged(QVariant)));
}

void DisplayController::onBrightnessSettingChanged(QVariant var)
{
	if (var.isValid())
		setBrightness(var.toInt());
}

void DisplayController::onAutoBrightnessSettingChanged(QVariant var)
{
	if (var.isValid())
		setAutoBrightness(var.toBool());
}

void DisplayController::setBrightness(int brightness)
{
	writeIntToFile(mBacklightDevice + "/brightness", brightness);
}

void DisplayController::setAutoBrightness(bool enable)
{
	writeIntToFile(mBacklightDevice + "/auto_brightness", enable);
}
