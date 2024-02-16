#pragma once

#include <QDBusAbstractInterface>
#include <QDBusPendingReply>

class CmTechnologyInterface : public QDBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()	{ return "net.connman.Technology"; }
	CmTechnologyInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

public slots:
	inline QDBusPendingReply<> Scan()
	{
		return asyncCall("Scan");
	}

	inline QDBusPendingReply<> SetProperty(const QString &name, const QDBusVariant &value)
	{
		return asyncCall("SetProperty", name, QVariant::fromValue(QDBusVariant(value)));
	}

signals:
	void PropertyChanged(const QString &name, const QDBusVariant &value);
};
