#pragma once

#include <QObject>
#include <QSocketNotifier>

class NetlinkMonitor : public QObject
{
	Q_OBJECT

public:
	explicit NetlinkMonitor(QObject *parent = nullptr);
	~NetlinkMonitor();

	bool requestInitialAddresses();

signals:
	void addressAdded(const QString &iface, const QString &address);
	void addressRemoved(const QString &iface, const QString &address);

private slots:
	void readNetlink();

private:
	int mFd = -1;
	QSocketNotifier *mNotifier = nullptr;

	void handleAddrMsg(struct nlmsghdr *nlh);
};
