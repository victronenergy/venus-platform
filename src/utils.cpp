#include <veutil/qt/ve_qitem.hpp>

void setWifiHotspotAndBluetooth(VeQItemSettings *settings, const int val)
{
	if (!settings)
		return;

	VeQItem *accessPoint = settings->root()->itemGetOrCreate("Settings/Services/AccessPoint");
	VeQItem *bluetooth = settings->root()->itemGetOrCreate("/Settings/Services/Bluetooth");

	if (accessPoint) {
		qDebug() << "Setting WiFi access point to:" << val;
		accessPoint->setValue(val);
	}

	if (bluetooth) {
		qDebug() << "Setting Bluetooth to:" << val;
		bluetooth->setValue(val);
	}
}
