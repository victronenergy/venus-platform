#include <veutil/qt/ve_qitem.hpp>

#include <fstream>

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

int getSystemUptimeInSeconds()
{
	std::ifstream uptimeFile("/proc/uptime");
	double uptime = -1.0;

	if (!uptimeFile.is_open())
		return -1;

	if (!(uptimeFile >> uptime))
		return -1;

	return static_cast<int>(uptime);
}
