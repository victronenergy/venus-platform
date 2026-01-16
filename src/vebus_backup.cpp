#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>

#include "application.hpp"
#include "venus_services.hpp"
#include "vebus_backup.hpp"

VebusBackupServiceRegistrator::VebusBackupServiceRegistrator(VeQItem *parentItem, VenusServices *services, QObject *parent) :
	QObject(parent)
{
	parentItemRef = parentItem;

	connect(services, SIGNAL(found(VenusService*)), SLOT(onVenusServiceFound(VenusService*)));
}

void VebusBackupService::onMk2ConnectionItemChanged(QVariant var)
{
	if (initialized || !var.isValid())
		return;


	qDebug() <<  "[Vebus_backup] Vebus connection found" << var.toString();

	working = false;
	availableBackupsListValid = false;

	mFileName.clear();
	vebusFirmwareVersionNumber.clear();
	vebusFirmwareSubVersionNumber.clear();
	vebusFirmwareVersionString.clear();
	vebusProductId.clear();

	QString con = var.toString();

	connection = con.section('/', -1);
	if (connection.isEmpty()) {
		qDebug() << "[Vebus_backup] Device name not found. Skip registration of " << connection;
		return;
	}
	VeQItem *vebusRootItem = venusPlatformParentItem->itemGetOrCreate("Vebus/Interface/" + connection);
	mAvailableBackupsItem = vebusRootItem->itemGetOrCreate("AvailableBackups");
	mIncompatibleBackupsItem = vebusRootItem->itemGetOrCreate("FirmwareIncompatibleBackups");

	mInfoItem = vebusRootItem->itemGetOrCreate("Info");
	mErrorItem = vebusRootItem->itemGetOrCreate("Error");
	mNotifyItem = vebusRootItem->itemGetOrCreate("Notify");
	mFileItem = vebusRootItem->itemGetOrCreateAndProduce("File", "");
	mActionItem = vebusRootItem->itemGetOrCreateAndProduce("Action", 0);

	mActionItem->produceValue(0);
	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));
	mFileItem->getValueAndChanges(this, SLOT(onFileNameChanged(QVariant)));

	// File index feature
	QString devicesPath = connection.startsWith("ttyUSB") ? "Vebus/Devices/1" : "Vebus/Devices/0";
	if (venusPlatformParentItem->itemGet(devicesPath) == nullptr) {
		VeQItem *deviceRootItem = venusPlatformParentItem->itemGetOrCreate(devicesPath);
		VeQItem *fileIndexItem = deviceRootItem->itemGetOrCreate("Restore/FileIndex");
		fileIndexItem->getValueAndChanges(this, SLOT(onFileIndexChanged(QVariant)));
		mRestoreActionItem = deviceRootItem->itemGetOrCreate("Restore/Action");
		mRestoreActionItem->getValueAndChanges(this, SLOT(onRestoreActionItemChanged(QVariant)));
		mRestoreNotifyItem = deviceRootItem->itemGetOrCreate("Restore/Notify");
	}

	// Connect to the mk2vsc service
	VeQItem *mk2VscRootItem = vebusInterfaceService->item()->itemParent()->itemGetOrCreate("com.victronenergy.mk2vsc");
	VeQItem *mk2VscStateItem = mk2VscRootItem->itemGetOrCreate("State");

	VeQItem *mk2DbusConnectedItem = vebusInterfaceService->item("Connected");
	mMk2DbusProductIdItem = vebusInterfaceService->item("ProductId");
	mMk2DbusFirmwareVersionItem = vebusInterfaceService->item("FirmwareVersion");
	mMk2DbusSubVersionItem = vebusInterfaceService->item("FirmwareSubVersion");

	mk2VscStateItem->getValueAndChanges(this, SLOT(onMk2VscStateChanged(QVariant)));
	mMk2DbusProductIdItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mMk2DbusFirmwareVersionItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mMk2DbusSubVersionItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mk2DbusConnectedItem->getValueAndChanges(this, SLOT(onVebusConnectedChanged(QVariant)));
	initialized = true;
}

VebusBackupService::VebusBackupService(VeQItem *parentItem, VenusService *service, QObject *parent) :
	QObject(parent)
{
	initialized = false;
	mRestoreNotifyItem = nullptr;
	mRestoreActionItem = nullptr;
	vebusInterfaceService = service;
	venusPlatformParentItem = parentItem;
	mMk2DbusMk2ConnectionItem = service->item("/Interfaces/Mk2/Connection");
	mMk2DbusMk2ConnectionItem->getValueAndChanges(this, SLOT(onMk2ConnectionItemChanged(QVariant)));
	// Wait until the connection item is valid
}

void VebusBackupService::onActionChanged(QVariant var)
{
	if (working) {
		return;
	}

	/*
	0 - idle
	1 - backup
	2 - restore
	3 - delete */

	if (var.isValid()) {
		int action = var.toInt();

		// Info indicates what we are doing, and should be invalid when we are idle.
		// Conversely, notify pops up at the end and should stay put until we transition
		// out of Idle.
		if (action == Action::idle) {
			mInfoItem->produceValue(QVariant());
		} else {
			mNotifyItem->produceValue(QVariant());
		}

		switch (action) {
		case Action::idle:
			break;
		case Action::backup:
			runBackupAction();
			break;
		case Action::restore:
			runRestoreAction();
			break;
		case Action::fileDelete:
			deleteBackupFile();
			break;
		default:
			break;
		}
	}
}

void VebusBackupService::onMk2VscStateChanged(QVariant var)
{
	if (var.isValid()) {
		// Forward the state
		mInfoItem->produceValue(var.toInt());
	}
}

void VebusBackupService::onFileNameChanged(QVariant var)
{
	if (var.isValid()) {
		mFileName = var.toString();
	} else {
		mFileName = "";
	}
}

void VebusBackupService::onFileIndexChanged(QVariant var)
{
	int idx = var.toInt();
	if (idx < mBackupFiles.size()) {
		// This also fires onFileNameChanged
		mFileItem->produceValue(mBackupFiles[idx].remove(QRegularExpression("-[^.-]*?\\.rv(sc|ms)$")));
	} else {
		mFileItem->produceValue("");
	}
}

void VebusBackupService::onRestoreActionItemChanged(QVariant var)
{
	if (var.isValid() && var.toInt() != 0) {
		mActionItem->produceValue(Action::restore);
	}
}

void VebusBackupService::updateAvailableBackups()
{
	bool firmwareMatch;
	QProcess proc;

	if (availableBackupsListValid) {
		return;
	}

	if (!getProductIdAndVersions()) {
		return;
	}

	QString conFilter = "*-" + connection;
	QDir directory(backupDir);
	mBackupFiles  = directory.entryList(QStringList() << conFilter + ".rvsc" << conFilter + ".rvms",
		QDir::Files, QDir::Name);

	QJsonArray backupFilesArray;
	QJsonArray incompBackupFilesArray;

	// Regular expression for extracting both firmware version and subversion number
	static QRegularExpression regex(R"(Firmware version\s*=\s*(\d+)(?:\s*\nFirmware subversion number\s*=\s*(\d+))?)");

	foreach(QString fileName, mBackupFiles) {
		// Check if files are valid by listing the contents
		QStringList arguments({"-L","-f",backupDir + fileName});
		proc.start(mk2vscProg, arguments);

		proc.waitForFinished();
		if (proc.exitStatus() == QProcess::NormalExit) {
			if (proc.exitCode() == 0) {
				QString output = proc.readAllStandardOutput();

				// VE.Bus firmware versions and product id should be known now.
				// Only add firmware compatible backup files to AvailableBackups

				QRegularExpressionMatchIterator matchIterator = regex.globalMatch(output);

				if (!matchIterator.isValid()) {
					break;
				}

				int sectionNumber = 0;
				firmwareMatch = false;
				while (matchIterator.hasNext()) {
					QRegularExpressionMatch match = matchIterator.next();
					QString firmwareVersion = match.captured(1);
					QString firmwareSubversion = match.captured(2).isEmpty() ? "0" : match.captured(2); // Handle missing subversion

					sectionNumber++;

					if(checkFirmwareVersionCompatibility(firmwareVersion, firmwareSubversion)) {
						backupFilesArray.append(fileName);
						firmwareMatch = true;
						break;
					}
				}

				if (!firmwareMatch) {
					incompBackupFilesArray.append(fileName);
				}
			}
			else {
				qDebug() << "[vebus_backup] Discarding corrupt file " << fileName;
			}
		}
		proc.close();
	}

	if (backupFilesArray.size() > 0) {
		mAvailableBackupsItem->produceValue(
			QString::fromUtf8(QJsonDocument(backupFilesArray).toJson(QJsonDocument::Compact)));
	} else {
		mAvailableBackupsItem->produceValue("");
	}

	if (incompBackupFilesArray.size() > 0) {
		mIncompatibleBackupsItem->produceValue(
			QString::fromUtf8(QJsonDocument(incompBackupFilesArray).toJson(QJsonDocument::Compact)));
	} else {
		mIncompatibleBackupsItem->produceValue("");
	}

	availableBackupsListValid = true;
}


void VebusBackupService::onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "Ve.Bus backup finished";

	working = false;
	availableBackupsListValid = false;
	offline = true;
	mActionItem->produceValue(Action::idle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::backupSuccessful);
		}
		mErrorItem->produceValue(exitCode);
	} else {
		mNotifyItem->produceValue(Notification::backupException);
	}

	//updateAvailableBackups(); // This is done by the mk2dbus connected slot
}

void VebusBackupService::runBackupAction()
{
	if (mFileName.isEmpty()) {
		mActionItem->produceValue(Action::idle);
		return;
	}

	working = true;
	QString fileName = vebusFirmwareVersionString + "-" + mFileName;

	qDebug() << "Start Ve.Bus backup to file name" << fileName;

	QProcess *backupProcess = new QProcess();
	QObject::connect(backupProcess, &QProcess::finished, this, &VebusBackupService::onBackupFinished);

	QStringList arguments({"-r",
						   "-c",
						   mk2vscCacheDir,
						   "-f",
						   backupDir + fileName + "-" + connection,
						   "-s"});

	if (offline) {
		qDebug() << "Ve.Bus backup through direct connection";
		arguments.append("/dev/" + connection);
	} else {
		qDebug() << "Ve.Bus backup through tunnel";
		arguments.append("dbus://com.victronenergy.vebus." + connection + "/Interfaces/Mk2/Tunnel");
	}

	backupProcess->start(mk2vscProg, arguments);
}


void VebusBackupService::onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus)
{

	qDebug() << "Ve.Bus restore finished";
	working = false;
	mActionItem->produceValue(Action::idle);
	if (mRestoreActionItem)
		mRestoreActionItem->produceValue(Action::idle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::restoreSuccessful);
			if (mRestoreNotifyItem)
				mRestoreNotifyItem->produceValue(Notification::restoreSuccessful);
		} else {
			if (mRestoreNotifyItem)
				mRestoreNotifyItem->produceValue(Notification::restoreException);
		}
		mErrorItem->produceValue(exitCode);
	} else {
		mNotifyItem->produceValue(Notification::restoreException);
		if (mRestoreNotifyItem)
			mRestoreNotifyItem->produceValue(Notification::restoreException);
	}
}

void VebusBackupService::runRestoreAction()
{
	if (mFileName.isEmpty()) {
		mActionItem->produceValue(Action::idle);
		return;
	}

	working = true;
	if (mRestoreNotifyItem)
		mRestoreNotifyItem->produceValue(QVariant());

	qDebug() << "Start Ve.Bus restore using file name" << mFileName;
	working = true;
	QProcess *restoreProcess = new QProcess();
	QObject::connect(restoreProcess, &QProcess::finished, this, &VebusBackupService::onRestoreFinished);

	QStringList arguments({"-w",
						   "-c",
						   mk2vscCacheDir,
						   "-S", // Allow single unit replace
						   "-f",
						   backupDir + mFileName + "-" + connection,
						   "-s"});

	if (offline) {
		qDebug() << "Ve.Bus restore through direct connection";
		arguments.append("/dev/" + connection);
	} else {
		qDebug() << "Ve.Bus restore through tunnel";
		arguments.append("dbus://com.victronenergy.vebus." + connection + "/Interfaces/Mk2/Tunnel");
	}

	restoreProcess->start(mk2vscProg, arguments);
}

void VebusBackupService::deleteBackupFile()
{
	if (mFileName.isEmpty()) {
		return;
	}

	QString baseName = backupDir + mFileName + "-" + connection;
	QStringList extensions = {"rvsc", "rvms"};

	for (const QString &ext : extensions) {
		QString filePath = baseName + "." + ext;
		if (QFile::exists(filePath)) {
			if (QFile::remove(filePath)) {
				qDebug() << "Deleted:" << filePath;
				availableBackupsListValid = false;
				mNotifyItem->produceValue(Notification::deleteSuccessful);
			} else {
				qDebug() << "Failed to delete:" << filePath;
				mNotifyItem->produceValue(Notification::deleteFailed);
			}
		}
	}

	mActionItem->produceValue(Action::idle);
	updateAvailableBackups();
}

void VebusBackupServiceRegistrator::onVenusServiceFound(VenusService *service)
{
	switch (service->getType())
	{
	case VenusServiceType::MULTI:
		new VebusBackupService(parentItemRef, service, this);
		break;
	default:
		;
	}
}

bool VebusBackupService::getProductIdAndVersions()
{
	QVariant versionVar = mMk2DbusFirmwareVersionItem->getValue();
	if (!versionVar.isValid()) {
		return false;
	}
	vebusFirmwareVersionNumber = QString::number(versionVar.toUInt(), 16);
	// GetText will return firmware version with subversion, e.g. 556.S99.
	// If GetText is not available, we use the version number.
	vebusFirmwareVersionString = mMk2DbusFirmwareVersionItem->getText();
	if (vebusFirmwareVersionString.isEmpty()) {
		vebusFirmwareVersionString = vebusFirmwareVersionNumber;
	}

	QVariant subVersionVar = mMk2DbusSubVersionItem->getValue();
	if (!subVersionVar.isValid()) {
		return false;
	}
	vebusFirmwareSubVersionNumber = QString::number(subVersionVar.toUInt(), 16);

	QVariant prodIdVar = mMk2DbusProductIdItem->getValue();
	if (!prodIdVar.isValid()) {
		return false;
	}
	vebusProductId = QString::number(prodIdVar.toUInt(), 16);

	return true;
}

bool VebusBackupService::checkFirmwareVersionCompatibility(const QString& prodIdAndVersion, const QString& subVersion)
{
	QString vebusPIdAndVersion;

	vebusPIdAndVersion = vebusProductId + vebusFirmwareVersionNumber;

	return ((vebusPIdAndVersion == prodIdAndVersion) && (vebusFirmwareSubVersionNumber == subVersion));
}

void VebusBackupService::onVebusProductIdOrVersionChanged(QVariant var)
{
	if (var.isValid()) {
		updateAvailableBackups();
	} else {
		availableBackupsListValid = false;
		vebusProductId.clear();
	}
}

void VebusBackupService::onVebusConnectedChanged(QVariant var)
{
	if (working) {
		return;
	}

	if(var.isValid()) {
		offline = false;
		if(var.toInt() != 0) {
			updateAvailableBackups();
		}
	} else {
		offline = true;
	}
}
