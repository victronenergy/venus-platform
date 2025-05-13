#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include "application.hpp"
#include "tank_backup.hpp"
#include "venus_services.hpp"


TankBackupService::TankBackupService(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	qDebug() << "[Tank Backup] Service constructor";

	mTankBackupItem = parentItem->itemGetOrCreate("BackupRestore/Tank");

	mActionItem = mTankBackupItem->itemGetOrCreateAndProduce("Action", 0);
	mErrorItem = mTankBackupItem->itemGetOrCreateAndProduce("Error", 0);
	mInfoItem = mTankBackupItem->itemGetOrCreateAndProduce("Info", 0);
	mNotifyItem = mTankBackupItem->itemGetOrCreate("Notify");

	//mActionItem->produceValue(0);
	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));

}

void TankBackupService::onActionChanged(QVariant var)
{
	qDebug() << "[Tank Backup] action changed to:" << var;

	int action;

	qDebug() << "[Tank Backup] working:" << working;

	if (working) {
		return;
	}

	/*
	0 - idle
	1 - create USB
	2 - backup
	3 - restore
	4 - delete
	*/

	if (var.isValid()) {
		action = var.toInt();

		switch (action) {
			case Action::actionIdle:
				break;
			case Action::actionCreateUsb:
				runCreateUsbAction();
				break;
			case Action::actionBackup:
				runBackupAction();
				break;
			case Action::actionRestore:
				runRestoreAction();
				break;
			case Action::actionDelete:
				runDeleteAction();
				break;
			default:
				break;
		}
	}

	qDebug() << "[Tank Backup] /BackupRestore/Tank/Action changed now to:" << var;
}

void TankBackupService::onCreateUsbFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[Tank backup] Create USB finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::createUsbSuccessful);
		}
	} else {
		mNotifyItem->produceValue(Notification::createUsbException);
	}
	mErrorItem->produceValue(exitCode);
}

void TankBackupService::runCreateUsbAction()
{
	qDebug() << "[Tank backup] Run create USB";

	working = true;

	QString usbDrivePath = "/media/sda1";
	QString archiveName = "venus-runtime-tank-settings-auto-restore.tgz";

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[Tank backup] USB drive not mounted";
		mErrorItem->produceValue(Error::errorUsbDriveNotMounted);
		mActionItem->produceValue(Action::actionIdle);
		return;
	}

	// Check if the archive file already exists and if yes, delete it
	QFileInfo archiveFile(usbDrivePath + "/" + archiveName);
	if (archiveFile.exists()) {
		qDebug() << "[Tank backup] Archive file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + archiveName)) {
			qDebug() << "[Tank backup] Failed to delete existing archive file";
			mErrorItem->produceValue(Error::errorArchiveFileDeleteFailed);
			mActionItem->produceValue(Action::actionIdle);
			return;
		}
	}

	// Create the archive file in separate thread
	QProcess *backupProcess = new QProcess();
	QObject::connect(backupProcess, &QProcess::finished, this, &TankBackupService::onCreateUsbFinished);

	QStringList arguments({
		"-czf",
		usbDrivePath + "/" + archiveName,
		"-C",
		"/opt/victronenergy/tank-backup",
		"rc",
	});

	backupProcess->start("/bin/tar", arguments);

}

void TankBackupService::onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[Tank backup] Backup finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::backupSuccessful);
		}
	} else {
		mNotifyItem->produceValue(Notification::backupException);
	}
	mErrorItem->produceValue(exitCode);
}

void TankBackupService::runBackupAction()
{
	qDebug() << "[Tank backup] Run backup";

	working = true;

	QString usbDrivePath = "/media/sda1";
	QString backupName = "tank-backup.xml";

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[Tank backup] USB drive not mounted";
		mErrorItem->produceValue(Error::errorUsbDriveNotMounted);
		mActionItem->produceValue(Action::actionIdle);
		return;
	}

	// Check if the backup file already exists and if yes, delete it
	QFileInfo backupFile(usbDrivePath + "/" + backupName);
	if (backupFile.exists()) {
		qDebug() << "[Tank backup] Backup file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + backupName)) {
			qDebug() << "[Tank backup] Failed to delete existing backup file";
			mErrorItem->produceValue(Error::errorBackupFileDeleteFailed);
			mActionItem->produceValue(Action::actionIdle);
			return;
		}
	}

	// Create the backup file in separate thread
	QProcess *backupProcess = new QProcess();
	QObject::connect(backupProcess, &QProcess::finished, this, &TankBackupService::onBackupFinished);

	QStringList arguments({
		"/opt/victronenergy/tank-backup/rc/tank-backup-restore.py",
		"--backup",
	});

	backupProcess->start("/usr/bin/python", arguments);
	return;
}


void TankBackupService::onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[Tank backup] Restore finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::restoreSuccessful);
		}
	} else {
		mNotifyItem->produceValue(Notification::restoreException);
	}
	mErrorItem->produceValue(exitCode);
}

void TankBackupService::runRestoreAction()
{
	qDebug() << "[Tank backup] Run restore";

	working = true;

	QString usbDrivePath = "/media/sda1";
	QString backupName = "tank-backup.xml";

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[Tank backup] USB drive not mounted";
		mErrorItem->produceValue(Error::errorUsbDriveNotMounted);
		mActionItem->produceValue(Action::actionIdle);
		return;
	}

	// Check if the backup file exists
	QFileInfo backupFile(usbDrivePath + "/" + backupName);
	if (!backupFile.exists()) {
		qDebug() << "[Tank backup] Backup file is missing";
		mErrorItem->produceValue(Error::errorBackupFileMissing);
		mActionItem->produceValue(Action::actionIdle);
		return;
	}

	// Create the backup file in separate thread
	QProcess *restoreProcess = new QProcess();
	QObject::connect(restoreProcess, &QProcess::finished, this, &TankBackupService::onRestoreFinished);

	QStringList arguments({
		"/opt/victronenergy/tank-backup/rc/tank-backup-restore.py",
		"--restore",
	});

	restoreProcess->start("/usr/bin/python", arguments);
	return;
}

void TankBackupService::runDeleteAction()
{
	qDebug() << "[Tank backup] Delete backup";

	working = true;

	QString usbDrivePath = "/media/sda1";
	QString backupName = "tank-backup.xml";

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[Tank backup] USB drive not mounted";
		mErrorItem->produceValue(Error::errorUsbDriveNotMounted);
		mActionItem->produceValue(Action::actionIdle);
		return;
	}

	// Check if the backup file already exists and if yes, delete it
	QFileInfo backupFile(usbDrivePath + "/" + backupName);
	if (backupFile.exists()) {
		qDebug() << "[Tank backup] Backup file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + backupName)) {
			qDebug() << "[Tank backup] Failed to delete existing backup file";
			mErrorItem->produceValue(Error::errorBackupFileDeleteFailed);
			mActionItem->produceValue(Action::actionIdle);
			return;
		}
	}

	working = false;
	mActionItem->produceValue(Action::actionIdle);
	mNotifyItem->produceValue(Notification::deleteSuccessful);

	return;
}
