#include "src/network-resetter-app.h"
#include <veutil/qt/ve_dbus_connection.hpp>
#include "networkresetter.h"

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
	NetworkResetter *resetter = new NetworkResetter(mSettings, this);
	resetter->resetNetwork();
}
