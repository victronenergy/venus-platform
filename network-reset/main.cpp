#include <iostream>

#include <veutil/qt/ve_dbus_connection.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "network_reset.hpp"
#include "network-reset/main.hpp"

NetworkResetterApp::NetworkResetterApp(int &argc, char **argv) :
	QCoreApplication(argc, argv)
{
	QDBusConnection dbus = VeDbusConnection::getConnection();
	if (!dbus.isConnected()) {
		qCritical() << "DBus connection failed";
		::exit(EXIT_FAILURE);
	}

	VeQItemDbusProducer *producer = new VeQItemDbusProducer(VeQItems::getRoot(), "dbus");
	producer->open(dbus);
	mServices = producer->services();

	mSettings = new VeQItemDbusSettings(producer->services(), QString("com.victronenergy.settings"));

	VeQItem *item = mServices->itemGetOrCreate("com.victronenergy.settings");

	if (item->getState() != VeQItem::Synchronized) {
		qDebug() << "Localsettings not found";
		::exit(EXIT_FAILURE);
	}

	qDebug() << "Localsettings found";

	start();
}

void NetworkResetterApp::start()
{
	NetworkReset *networkReset = new NetworkReset(mSettings, this);
	networkReset->reset();
}

int main(int argc, char *argv[])
{
	if (argc > 1) {
		std::cout <<
			"Usage: " << argv[0] << "\n"
			"* Turns off venus-platform and connman.\n"
			"* Removes network settings.\n"
			"* Sets access points to defaults.\n"
			"* Turn off localsettings (which should also save and fsync).\n"
			"\n"
			"No arguments required.\n";

		return 1;
	}

	NetworkResetterApp app(argc, argv);
	return app.exec();
}
