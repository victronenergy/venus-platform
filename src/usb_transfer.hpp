#pragma once

#include <QObject>
#include <QThread>
#include <QProcess>
#include <QVector>
#include <QPair>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "usb_transfer.hpp"
#include "venus_services.hpp"

class UsbTransferService : public QObject
{
	Q_OBJECT

public:
	explicit UsbTransferService(VeQItem *parentItem, QObject *parent = 0);

	enum Action {
		actionIdle = 0,
		actionCreateDrive,
		actionExport,
		actionImport,
		actionDelete,
	};

	enum Notification {
		exportSuccessful = 1,
		importSuccessful = 2,
		deleteSuccessful = 3,
		exportException = 101,
		importException = 102,
		deleteException = 103,
		createDriveException = 104,
	};

	enum Error {
		errorNone = 0,
		errorDriveNotMounted,
		errorCreateDriveException,
		errorExportException,
		errorimportException,
		errorArchiveFileDeleteFailed,
		errorExportFileDeleteFailed,
		errorExportFileMissing,
	};

private:
	void onCreateDriveFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runCreateDriveAction();
	void onExportFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runExportAction();
	void onImportFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runImportAction();
	void runDeleteAction();

	bool working = false;

	VeQItem *mUsbTransferItem;
	VeQItem *mActionItem;
	VeQItem *mInfoItem;
	VeQItem *mErrorItem;
	VeQItem *mNotifyItem;

	QString usbDrivePath = "/media/sda1";
	QString archiveName = "venus-runtime-tank-setup-auto-import.tgz";
	QString exportName = "tank-setups-export.xml";

private slots:
	void onActionChanged(QVariant var);
};
