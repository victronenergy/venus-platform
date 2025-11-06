#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include "application.hpp"
#include "usb_transfer.hpp"
#include "venus_services.hpp"


UsbTransferService::UsbTransferService(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	mUsbTransferItem = parentItem->itemGetOrCreate("UsbTransfer/Tank");

	mActionItem = mUsbTransferItem->itemGetOrCreateAndProduce("Action", 0);
	mExitCodeItem = mUsbTransferItem->itemGetOrCreateAndProduce("ExitCode", 0);
	mNotifyItem = mUsbTransferItem->itemGetOrCreate("Notify");

	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));
}

void UsbTransferService::onActionChanged(QVariant var)
{
	int action;

	if (working) {
		return;
	}

	if (var.isValid()) {
		action = var.toInt();

		switch (action) {
			case Action::actionIdle:
				break;
			case Action::actionCreateDrive:
				runCreateDriveAction();
				break;
			case Action::actionExport:
				runExportAction();
				break;
			case Action::actionImport:
				runImportAction();
				break;
			case Action::actionDelete:
				runDeleteAction();
				break;
			default:
				break;
		}
	}
}

void UsbTransferService::onCreateDriveFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[USB Transfer] Create USB drive finished. Exit code:" << exitCode;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		mNotifyItem->produceValue(Notification::exportSuccessful);
	} else {
		mNotifyItem->produceValue(Notification::driveNotMountedError);
	}
}

void UsbTransferService::runCreateDriveAction()
{
	qDebug() << "[USB Transfer] Run create USB drive";

	working = true;

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[USB Transfer] USB drive not mounted";
		mNotifyItem->produceValue(Notification::driveNotMountedError);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Check if the archive file already exists and if yes, delete it
	QFileInfo archiveFile(usbDrivePath + "/" + archiveName);
	if (archiveFile.exists()) {
		qDebug() << "[USB Transfer] Archive file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + archiveName)) {
			qDebug() << "[USB Transfer] Failed to delete existing archive file";
			mNotifyItem->produceValue(Notification::archiveFileDeleteFailedError);
			mActionItem->produceValue(Action::actionIdle);
			working = false;
			return;
		}
	}

	// Create the archive file in separate thread
	QProcess *exportProcess = new QProcess();
	QObject::connect(exportProcess, &QProcess::finished, this, &UsbTransferService::onCreateDriveFinished);

	QStringList arguments({
		"-czf",
		usbDrivePath + "/" + archiveName,
		"-C",
		"/opt/victronenergy/usb-transfer",
		"rc",
	});

	exportProcess->start("/bin/tar", arguments);

}

void UsbTransferService::onExportFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[USB Transfer] Export finished. Exit code:" << exitCode;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		runCreateDriveAction();
	} else {
		mExitCodeItem->produceValue(exitCode);
		mNotifyItem->produceValue(Notification::exportException);
	}
}

void UsbTransferService::runExportAction()
{
	qDebug() << "[USB Transfer] Run export";

	working = true;

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[USB Transfer] USB drive not mounted";
		mNotifyItem->produceValue(Notification::driveNotMountedError);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Check if the export file already exists and if yes, delete it
	QFileInfo exportFile(usbDrivePath + "/" + exportName);
	if (exportFile.exists()) {
		qDebug() << "[USB Transfer] Export file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + exportName)) {
			qDebug() << "[USB Transfer] Failed to delete existing export file";
			mNotifyItem->produceValue(Notification::exportFileDeleteFailedError);
			mActionItem->produceValue(Action::actionIdle);
			working = false;
			return;
		}
	}

	// Create the export file in separate thread
	QProcess *exportProcess = new QProcess();
	QObject::connect(exportProcess, &QProcess::finished, this, &UsbTransferService::onExportFinished);

	QStringList arguments({
		"/opt/victronenergy/usb-transfer/rc/usb-transfer.py",
		"--export",
	});

	exportProcess->start("/usr/bin/python", arguments);
	return;
}


void UsbTransferService::onImportFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[USB Transfer] Import finished. Exit code:" << exitCode;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		mNotifyItem->produceValue(Notification::importSuccessful);
	} else {
		mExitCodeItem->produceValue(exitCode);
		mNotifyItem->produceValue(Notification::importException);
	}
}

void UsbTransferService::runImportAction()
{
	qDebug() << "[USB Transfer] Run import";

	working = true;

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[USB Transfer] USB drive not mounted";
		mNotifyItem->produceValue(Notification::driveNotMountedError);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Check if the export file exists
	QFileInfo exportFile(usbDrivePath + "/" + exportName);
	if (!exportFile.exists()) {
		qDebug() << "[USB Transfer] Export file is missing";
		mNotifyItem->produceValue(Notification::exportFileMissingError);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Create the export file in separate thread
	QProcess *importProcess = new QProcess();
	QObject::connect(importProcess, &QProcess::finished, this, &UsbTransferService::onImportFinished);

	QStringList arguments({
		"/opt/victronenergy/usb-transfer/rc/usb-transfer.py",
		"--import",
	});

	importProcess->start("/usr/bin/python", arguments);
	return;
}

void UsbTransferService::runDeleteAction()
{
	qDebug() << "[USB Transfer] Delete export";

	working = true;

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[USB Transfer] USB drive not mounted";
		mNotifyItem->produceValue(Notification::driveNotMountedError);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Check if the export file already exists and if yes, delete it
	QFileInfo exportFile(usbDrivePath + "/" + exportName);
	if (exportFile.exists()) {
		qDebug() << "[USB Transfer] Export file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + exportName)) {
			qDebug() << "[USB Transfer] Failed to delete existing export file";
			mNotifyItem->produceValue(Notification::exportFileDeleteFailedError);
			mActionItem->produceValue(Action::actionIdle);
			working = false;
			return;
		}
	}

	// Check if the archive file already exists and if yes, delete it
	QFileInfo archiveFile(usbDrivePath + "/" + archiveName);
	if (archiveFile.exists()) {
		qDebug() << "[USB Transfer] Archive file already exists, deleting it";
		if (!QFile::remove(usbDrivePath + "/" + archiveName)) {
			qDebug() << "[USB Transfer] Failed to delete existing archive file";
			mNotifyItem->produceValue(Notification::archiveFileDeleteFailedError);
			mActionItem->produceValue(Action::actionIdle);
			working = false;
			return;
		}
	}

	working = false;
	mActionItem->produceValue(Action::actionIdle);
	mNotifyItem->produceValue(Notification::deleteSuccessful);

	return;
}
