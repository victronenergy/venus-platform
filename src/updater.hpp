#pragma once

#include <QFileSystemWatcher>
#include <QFile>
#include <QLocalSocket>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

class Updater;

class VeQItemCheckUpdate : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemCheckUpdate(bool offline, VeQItem *state) :
		VeQItemAction(),
		mOffline(offline),
		mState(state)
	{}

	int setValue(const QVariant &value) override;

private:
	bool mOffline;
	VeQItem *mState;
};

class VeQItemSwitchVersion: public VeQItemAction {
	Q_OBJECT

public:
	VeQItemSwitchVersion() : VeQItemAction() {}

	int setValue(const QVariant &value) override;
};

class VeQItemDoUpdate : public VeQItemAction
{
	Q_OBJECT

public:
	VeQItemDoUpdate(bool offline, VeQItem *progress, VeQItem *state) :
		VeQItemAction(),
		mOffline(offline),
		mProgress(progress),
		mState(state)
	{}

	int setValue(const QVariant &value) override;

private:
	bool mOffline;
	VeQItem *mProgress;
	VeQItem *mState;
};

class SwuUpdateMonitor : public QObject
{
	Q_OBJECT

public:
	SwuUpdateMonitor(QString const &cmd, QStringList const&args, VeQItem *progress, VeQItem *state);

private Q_SLOTS:
	void onTmpChanged();
	void onReadReady();

private:
	void openSocket();
	QLocalSocket mSocket;
	QFileSystemWatcher mWatcher;
	VeQItem *mProgress;
	VeQItem *mState;
};

class Updater : public QObject
{
	Q_OBJECT

public:
	Updater(VeQItem *parentItem, QObject *parent = 0);

private slots:
	void checkFile(const QString &fileName);

private:
	void getUpdateInfoFromFile(QString const &fileName);
	void getRootfsInfoFromFile(QString const &fileName);
	void touchFile(QString const &fileName);
	void getVersionInfoFromLine(QString const &line, QVariant &build, QVariant &version);

	QFileSystemWatcher mUpdateWatcher;
	VeQItem *mItem;
};
