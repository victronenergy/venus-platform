#pragma once

#include <QObject>
#include <QProcess>
#include <QVector>
#include <QPair>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "tank_backup.hpp"
#include "venus_services.hpp"

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
		actionCreateUsbSuccessful,
		actionBackupSuccessful,
		actionRestoreSuccessful,
		actionDeleteSuccessful
	};

	enum Notification {
		backupSuccessful = 1,
		restoreSuccessful = 2,
		deleteSuccessful = 3,
		backupException = 101,
		restoreException = 102,
		deleteFailed = 103,
	};

private:
	void onCreateUsbFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runCreateUsbAction();
	void onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runBackupAction();
	void onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runRestoreAction();
	void onDeleteFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runDeleteAction();

	VeQItem *mTankBackupItem;
	VeQItem *mActionItem;
	VeQItem *mInfoItem;
	VeQItem *mErrorItem;
	VeQItem *mNotifyItem;

private slots:
	void onActionChanged(QVariant var);
};
