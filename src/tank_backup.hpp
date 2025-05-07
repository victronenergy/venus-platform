#pragma once

#include <QObject>
#include <QThread>
#include <QProcess>
#include <QVector>
#include <QPair>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "tank_backup.hpp"
#include "venus_services.hpp"

class Worker : public QObject {
    Q_OBJECT

public:
    explicit Worker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void performTask();

signals:
    void taskFinished();
};

class TankBackupService : public QObject
{
	Q_OBJECT

public:
	explicit TankBackupService(VeQItem *parentItem, QObject *parent = 0);

	enum Action {
		actionIdle = 0,
		actionCreateUsb,
		actionBackup,
		actionRestore,
		actionDelete,
	};

	enum Notification {
		createUsbSuccessful = 1,
		backupSuccessful = 2,
		restoreSuccessful = 3,
		deleteSuccessful = 4,
		createUsbException = 101,
		backupException = 102,
		restoreException = 103,
		deleteException = 104,
	};

	enum Error {
		errorNone = 0,
		errorUsbDriveNotMounted,
		errorArchiveFileDeleteFailed,
		errorBackupFileDeleteFailed,
		errorBackupFileMissing,
	};

private:
	void onCreateUsbFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runCreateUsbAction();
	void onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runBackupAction();
	void onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runRestoreAction();
	void runDeleteAction();

	bool working = false;

	VeQItem *mTankBackupItem;
	VeQItem *mActionItem;
	VeQItem *mInfoItem;
	VeQItem *mErrorItem;
	VeQItem *mNotifyItem;

private slots:
	void onActionChanged(QVariant var);
};
