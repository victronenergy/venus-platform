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
	mErrorItem = mUsbTransferItem->itemGetOrCreateAndProduce("Error", 0);
	mInfoItem = mUsbTransferItem->itemGetOrCreateAndProduce("Info", 0);
	mNotifyItem = mUsbTransferItem->itemGetOrCreate("Notify");

	//mActionItem->produceValue(0);
	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));

}

void UsbTransferService::onActionChanged(QVariant var)
{
	// qDebug() << "[USB Transfer] action changed to:" << var;

	int action;

	// qDebug() << "[USB Transfer] working:" << working;

	if (working) {
		return;
	}

	/*
	0 - idle
	1 - create drive
	2 - export
	3 - import
	4 - delete
	*/

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

	// qDebug() << "[USB Transfer] /UsbTransfer/Tank/Action changed now to:" << var;
}

void UsbTransferService::onCreateDriveFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "[USB Transfer] Create USB drive finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		mNotifyItem->produceValue(Notification::exportSuccessful);
	} else {
		mNotifyItem->produceValue(Notification::createDriveException);
		mErrorItem->produceValue(Error::errorCreateDriveException);
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
		mErrorItem->produceValue(Error::errorDriveNotMounted);
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
			mErrorItem->produceValue(Error::errorArchiveFileDeleteFailed);
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
	qDebug() << "[USB Transfer] Export finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		// mNotifyItem->produceValue(Notification::exportSuccessful);
		runCreateDriveAction();
	} else {
		mNotifyItem->produceValue(Notification::exportException);
		mErrorItem->produceValue(Error::errorExportException);
	}

}

void UsbTransferService::runExportAction()
{
	qDebug() << "[USB Transfer] Run export";

	working = true;

	// QString usbDrivePath = "/media/sda1";
	// QString exportName = "tank-setups-export.xml";

	// Check if the USB drive is mounted
	QFileInfo usbDrive(usbDrivePath);
	if (!usbDrive.exists() || !usbDrive.isDir()) {
		qDebug() << "[USB Transfer] USB drive not mounted";
		mErrorItem->produceValue(Error::errorDriveNotMounted);
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
			mErrorItem->produceValue(Error::errorExportFileDeleteFailed);
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
	qDebug() << "[USB Transfer] Import finished. Exit code:" << exitCode << "Exit status:" << exitStatus;
	working = false;
	mActionItem->produceValue(Action::actionIdle);

	if (exitStatus == QProcess::NormalExit && exitCode == 0) {
		mNotifyItem->produceValue(Notification::importSuccessful);
	} else {
		mNotifyItem->produceValue(Notification::importException);
		mErrorItem->produceValue(Error::errorimportException);
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
		mErrorItem->produceValue(Error::errorDriveNotMounted);
		mActionItem->produceValue(Action::actionIdle);
		working = false;
		return;
	}

	// Check if the export file exists
	QFileInfo exportFile(usbDrivePath + "/" + exportName);
	if (!exportFile.exists()) {
		qDebug() << "[USB Transfer] Export file is missing";
		mErrorItem->produceValue(Error::errorExportFileMissing);
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
		mErrorItem->produceValue(Error::errorDriveNotMounted);
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
			mErrorItem->produceValue(Error::errorExportFileDeleteFailed);
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
			mErrorItem->produceValue(Error::errorArchiveFileDeleteFailed);
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
