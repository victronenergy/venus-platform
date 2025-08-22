#include <veutil/qt/ve_qitem.hpp>
#include <QFile>
#include <thread>

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

bool serviceRunning(QString const &svc, bool *ok) {
	*ok = false;
	QString statusFile = QString("/service/%1/supervise/status").arg(svc);

	QFile file(statusFile);

	if (!file.exists())
		return false;

	if (!file.open(QIODevice::ReadOnly))
		return false;

	const QByteArray statusData = file.readAll();
	file.close();

	if (statusData.size() < 16)
		return false;

	quint32 pid = 0;

	// https://github.com/daemontools/daemontools/blob/master/src/svstat.c
	pid = static_cast<unsigned char>(statusData[15]);
	pid <<= 8; pid += static_cast<unsigned char>(statusData[14]);
	pid <<= 8; pid += static_cast<unsigned char>(statusData[13]);
	pid <<= 8; pid += static_cast<unsigned char>(statusData[12]);

	*ok = true;
	return pid != 0;
}

/**
 * Note that this blocks, so only use when you know you can afford that.
 */
bool stopServiceAndWaitForExit(const QString &svc, const std::chrono::seconds timeout)
{
	bool stopped = false;

	for (auto limit = std::chrono::steady_clock::now() + timeout ; std::chrono::steady_clock::now() < limit; )
	{
		bool ok = false;
		bool running = serviceRunning(svc, &ok);

		if (ok && !running) {
			qDebug() << svc << "exit detected";
			stopped = true;
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return stopped;
}
