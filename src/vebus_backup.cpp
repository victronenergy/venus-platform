#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include "application.hpp"
#include "venus_services.hpp"
#include "vebus_backup.hpp"

VebusBackupServiceRegistrator::VebusBackupServiceRegistrator(VeQItem *parentItem, VenusServices *services, QObject *parent) :
	QObject(parent)
{
	parentItemRef = parentItem;

	connect(services, SIGNAL(found(VenusService*)), SLOT(onVenusServiceFound(VenusService*)));
}

VebusBackupService::VebusBackupService(VeQItem *parentItem, VenusService *service, QObject *parent) :
	QObject(parent)
{
	QString con;

	working = false;
	availableBackupsListValid = false;

	fileName.clear();
	vebusFirmwareVersionNumber.clear();
	vebusFirmwareSubVersionNumber.clear();
	vebusProductId.clear();

	mMk2DbusMk2ConnectionItem = service->item("Interfaces/Mk2/Connection");

	con = mMk2DbusMk2ConnectionItem->getText();

	connection = con.section('/', -1);
	if (connection.isEmpty()) {
		qDebug() << "[Vebus_backup] Device name not found. Skip registration of " << connection;
		return;
	}
	mVebusRootItem = parentItem->itemGetOrCreate("Vebus/Interface/" + connection);
	mAvailableBackupsItem = mVebusRootItem->itemGetOrCreate("AvailableBackups");
	mIncompatibleBackupsItem = mVebusRootItem->itemGetOrCreate("FirmwareIncompatibleBackups");

	mActionItem = mVebusRootItem->itemGetOrCreate("Action");
	mActionItem->produceValue(0);
	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));

	mInfoItem = mVebusRootItem->itemGetOrCreate("Info");
	mErrorItem = mVebusRootItem->itemGetOrCreate("Error");
	mNotifyItem = mVebusRootItem->itemGetOrCreate("Notify");

	mFileItem = mVebusRootItem->itemGetOrCreate("File");
	mFileItem->getValueAndChanges(this, SLOT(onFileNameChanged(QVariant)));

	// Connect to the mk2vsc service
	mMk2VscRootItem = service->item()->itemParent()->itemGetOrCreate("com.victronenergy.mk2vsc");
	mMk2VscStateItem = mMk2VscRootItem->itemGetOrCreate("State");

	mMk2DbusProductIdItem = service->item("ProductId");
	mMk2DbusConnectedItem = service->item("Connected");
	mMk2DbusFirmwareVersionItem = service->item("FirmwareVersion");
	mMk2DbusSubVersionItem = service->item("FirmwareSubVersion");

	mMk2VscStateItem->getValueAndChanges(this, SLOT(onMk2VscStateChanged(QVariant)));
	mMk2DbusProductIdItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mMk2DbusFirmwareVersionItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mMk2DbusSubVersionItem->getValueAndChanges(this, SLOT(onVebusProductIdOrVersionChanged(QVariant)));
	mMk2DbusConnectedItem->getValueAndChanges(this, SLOT(onVebusConnectedChanged(QVariant)));
}

void VebusBackupService::onActionChanged(QVariant var)
{
	int action;

	if (working) {
		return;
	}
	/*
	0 - idle
	1 - backup
	2 - restore
	3 - delete */

	if (var.isValid()) {
		action = var.toInt();

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
#if 1 // Produce state as value
		mInfoItem->produceValue(var.toInt());
#else // Produce state as text (Note GUI requires values)
		int key = var.toInt();
		bool validKey = false;

		for (const auto &pair : mk2VscStateLookup) {
			if (pair.first == key) {
				validKey = true;
				mInfoItem->produceValue(pair.second);
				break;
			}
		}

		if (!validKey) {
			mInfoItem->produceValue("mk2vsc state " + var.toString());
		}
#endif
	}
}

void VebusBackupService::onFileNameChanged(QVariant var)
{
	if (var.isValid()) {
		fileName = var.toString();
	}
}

void VebusBackupService::getAvailableBackups()
{
	int i = 0;
	int j = 0;
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
	QStringList backupFiles  = directory.entryList(QStringList() << conFilter + ".rvsc" << conFilter + ".rvms",QDir::Files);
	QString backupFilesString = "[";
	QString incompBackupFilesString = "[";

	// Regular expression for extracting both firmware version and subversion number
	static QRegularExpression regex(R"(Firmware version\s*=\s*(\d+)(?:\s*\nFirmware subversion number\s*=\s*(\d+))?)");

	foreach(QString fileName, backupFiles) {
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
#if 0
					qDebug() << "[vebus_backup] Info section" << sectionNumber << ":";
					qDebug() << "[vebus_backup]  Firmware version:" << firmwareVersion;
					qDebug() << "[vebus_backup]  Firmware subversion number:" << firmwareSubversion;
#endif
					sectionNumber++;

					if(checkFirmwareVersionCompatibility(firmwareVersion, firmwareSubversion)) {
						if (i > 0) {
							backupFilesString.append(", ");
						}
						backupFilesString.append("\"" + fileName + "\"");
						i++;
						firmwareMatch = true;
						break;
					}
				}

				if (!firmwareMatch) {
					if (j > 0) {
						incompBackupFilesString.append(", ");
					}
					incompBackupFilesString.append("\"" + fileName + "\"");
					j++;
				}
			}
			else {
				qDebug() << "[vebus_backup] Discarding corrupt file " << fileName;
			}
		}
		proc.close();
	}

	backupFilesString.append("]");
	if (i > 0) {
		mAvailableBackupsItem->produceValue(backupFilesString);
	} else {
		mAvailableBackupsItem->produceValue("");
	}

	incompBackupFilesString.append("]");
	if (j > 0) {
		mIncompatibleBackupsItem->produceValue(incompBackupFilesString);
	} else {
		mIncompatibleBackupsItem->produceValue("");
	}

	availableBackupsListValid = true;
}

void VebusBackupService::onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	Q_UNUSED(exitCode);
	Q_UNUSED(exitStatus);

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

	//getAvailableBackups(); // This is done by the mk2dbus connected slot
}

void VebusBackupService::runBackupAction()
{
	if (fileName.isEmpty()) {
		mActionItem->produceValue(Action::idle);
		return;
	}

	working = true;

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
	Q_UNUSED(exitCode);
	Q_UNUSED(exitStatus);

	qDebug() << "Ve.Bus restore finished";
	working = false;
	mActionItem->produceValue(Action::idle);

	if (exitStatus == QProcess::NormalExit) {
		if (exitCode == 0) {
			mNotifyItem->produceValue(Notification::restoreSuccessful);
		}
		mErrorItem->produceValue(exitCode);
	} else {
		mNotifyItem->produceValue(Notification::restoreException);
	}
}

void VebusBackupService::runRestoreAction()
{
	if (fileName.isEmpty()) {
		mActionItem->produceValue(Action::idle);
		return;
	}

	working = true;

	qDebug() << "Start Ve.Bus restore using file name" << fileName;
	working = true;
	QProcess *restoreProcess = new QProcess();
	QObject::connect(restoreProcess, &QProcess::finished, this, &VebusBackupService::onRestoreFinished);

	QStringList arguments({"-w",
						   "-c",
						   mk2vscCacheDir,
						   "-S", // Allow single unit replace
						   "-f",
						   backupDir + fileName + "-" + connection,
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
	if (fileName.isEmpty()) {
		return;
	}

	QString baseName = backupDir + fileName + "-" + connection;
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
	getAvailableBackups();
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

	if ((vebusPIdAndVersion == prodIdAndVersion) && (vebusFirmwareSubVersionNumber == subVersion)) {
		return true;
	}

	return false;
}


void VebusBackupService::onVebusProductIdOrVersionChanged(QVariant var)
{
	if (var.isValid()) {
		getAvailableBackups();
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
			getAvailableBackups();
		}
	} else {
		offline = true;
	}
}
