#include <QDebug>

#include <veutil/qt/ve_dbus_connection.hpp>
#include "cmtechnology.h"

const QString CmTechnology::Powered("Powered");
const QString CmTechnology::Connected("Connected");
const QString CmTechnology::Name("Name");
const QString CmTechnology::Type("Type");
const QString CmTechnology::Tethering("Tethering");

CmTechnology::CmTechnology(QObject *parent) :
	QObject(parent),
	mTechnology("net.connman", "/", VeDbusConnection::getConnection())
{
}

CmTechnology::CmTechnology(const QString& path, const QVariantMap& properties, QObject *parent) :
	QObject(parent),
	mTechnology("net.connman", path, VeDbusConnection::getConnection())
{
	mPath = path;
	mProperties = properties;

	QObject::connect(&mTechnology, SIGNAL(PropertyChanged(QString,QDBusVariant)),
					 SLOT(propertyChanged(QString,QDBusVariant)));
}

CmTechnology::~CmTechnology()
{
	disconnect(this, SLOT(propertyChanged(QString,QDBusVariant)));
}

void CmTechnology::scan()
{
	QDBusPendingReply<> reply = mTechnology.Scan();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
	QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(dbusReply(QDBusPendingCallWatcher*)));
}

void CmTechnology::propertyChanged(const QString& name, const QDBusVariant& value)
{
	if (mProperties.contains(name)) {
		mProperties[name] = value.variant();
		if (name == Powered)
			emit poweredChanged();
		else if (name == Connected)
			emit connectedChanged();
		else if (name == Name)
			emit nameChanged();
		else if (name == Type)
			emit typeChanged();
		else if (name == Tethering)
			emit tetheringChanged();
	}
}

void CmTechnology::dbusReply(QDBusPendingCallWatcher *call)
{
	QDBusPendingReply<> reply = *call;
	if (reply.isError()) {
		QDBusError replyError = reply.error();
		qCritical() << __FILE__ << "dbusReply:" << replyError.name() << replyError.message() << replyError.type();
	}
	call->deleteLater();
}
