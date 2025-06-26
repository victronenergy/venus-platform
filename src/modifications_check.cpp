#include <QDebug>
#include <QProcess>

#include "application.hpp"
#include "modifications_check.hpp"

ModificationChecks::ModificationChecks(VeQItem *modChecks, QObject *parent) :
	QObject(parent),
	mModChecks(modChecks)
{
	modChecks->itemGetOrCreate("DataPartitionFreeSpace");
	modChecks->itemGetOrCreate("FsModifiedState");
	modChecks->itemGetOrCreate("SystemHooksState");
	modChecks->itemGetOrCreate("SshKeyForRootPresent");

	mActionItem = modChecks->itemGetOrCreateAndProduce("Action", 0);
	mActionItem->getValueAndChanges(this, SLOT(onModificationChecksAction(QVariant)));
	// execute the check once to populate the values
	mActionItem->setValue(Action::actionStartCheck);
}

void ModificationChecks::onModificationChecksAction(QVariant var)
{
	int action;

	if (var.isValid()) {
		action = var.toInt();

		switch (action) {
			case Action::actionStartCheck:
				startCheck();
				break;
			case Action::actionSystemHooksEnable:
				enableSystemHooks();
				break;
			case Action::actionSystemHooksDisable:
				disableSystemHooks();
				break;
			default:
				break;
		}

		mActionItem->produceValue(Action::actionIdle);
	}
}

void ModificationChecks::startCheck()
{
	// check data partition free space
	QProcess processFreeSpace;
	processFreeSpace.start("sh", QStringList() << "-c" << "df -B1 /data | awk 'NR==2 {print $4}'");
	processFreeSpace.waitForFinished();
	QString freeSpace = processFreeSpace.readAllStandardOutput().trimmed();
	qDebug() << "[Modification checks] data partition free space:" << freeSpace;
	mModChecks->itemGet("DataPartitionFreeSpace")->produceValue(freeSpace);

	// run "/usr/sbin/fsmodified /" and save the result
	QProcess process;
	process.start("/usr/sbin/fsmodified", QStringList() << "/");
	process.waitForFinished();
	QString result = process.readAllStandardOutput().trimmed();
	qDebug() << "[Modification checks] fsmodified result:" << result;

	if (result == "clean") {
		mModChecks->itemGet("FsModifiedState")->produceValue(ModificationChecks::fsModifiedStateClean);
	} else if (result == "modified") {
		mModChecks->itemGet("FsModifiedState")->produceValue(ModificationChecks::fsModifiedStateModified);
	} else {
		mModChecks->itemGet("FsModifiedState")->produceValue(ModificationChecks::fsModifiedStateUnknown);
	}

	// create variable to check if multiple files are present
	int systemHooksState = ModificationChecks::SystemHooksState_NonePresent;
	if (QFile::exists("/data/rc.local.disabled")) {
		qDebug() << "[Modification checks] /data/rc.local.disabled present";
		systemHooksState += ModificationChecks::SystemHooksState_RcLocalDisabled;
	}
	if (QFile::exists("/data/rcS.local.disabled")) {
		qDebug() << "[Modification checks] /data/rcS.local.disabled present";
		systemHooksState += ModificationChecks::SystemHooksState_RcSLocalDisabled;
	}
	if (QFile::exists("/data/rc.local")) {
		qDebug() << "[Modification checks] /data/rc.local present";
		systemHooksState += ModificationChecks::SystemHooksState_RcLocal;
	}
	if (QFile::exists("/data/rcS.local")) {
		qDebug() << "[Modification checks] /data/rcS.local present";
		systemHooksState += ModificationChecks::SystemHooksState_RcSLocal;
	}
	if (QFile::exists("/run/venus/custom-rc")) {
		qDebug() << "[Modification checks] /run/venus/custom-rc present";
		systemHooksState += ModificationChecks::SystemHooksState_HookLoadedAtStartup;
	}
	mModChecks->itemGet("SystemHooksState")->produceValue(systemHooksState);

	// Check if ssh key for root is present
	if (QFile::exists("/data/home/root/.ssh/authorized_keys")) {
		qDebug() << "[Modification checks] ssh key for root is present";
		mModChecks->itemGet("SshKeyForRootPresent")->produceValue(1);
	} else {
		qDebug() << "[Modification checks] ssh key for root is not present";
		mModChecks->itemGet("SshKeyForRootPresent")->produceValue(0);
	}
}

// enable all third party integrations
void ModificationChecks::enableSystemHooks()
{
	// enable /data/rc.local if it exists
	if (QFile::exists("/data/rc.local.disabled")) {
		if (!QFile::exists("/data/rc.local")) {
			qDebug() << "[Modification checks] enabled /data/rc.local";
			QFile::rename("/data/rc.local.disabled", "/data/rc.local");
		} else {
			qDebug() << "[Modification checks] /data/rc.local already exists, not enabling";
		}
	}

	// enable /data/rcS.local if it exists
	if (QFile::exists("/data/rcS.local.disabled")) {
		if (!QFile::exists("/data/rcS.local")) {
			qDebug() << "[Modification checks] enabled /data/rcS.local";
			QFile::rename("/data/rcS.local.disabled", "/data/rcS.local");
		} else {
			qDebug() << "[Modification checks] /data/rcS.local already exists, not enabling";
		}
	}
}

void ModificationChecks::disableSystemHooks()
{
	// disable /data/rc.local if it exists
	if (QFile::exists("/data/rc.local")) {
		qDebug() << "[Modification checks] disabled /data/rc.local";
		// remove an existing file else rename will fail
		if (QFile::exists("/data/rc.local.disabled"))
			QFile::remove("/data/rc.local.disabled");
		QFile::rename("/data/rc.local", "/data/rc.local.disabled");
	}

	// disable /data/rcS.local if it exists
	if (QFile::exists("/data/rcS.local")) {
		qDebug() << "[Modification checks] disabled /data/rcS.local";
		// remove an existing file else rename will fail
		if (QFile::exists("/data/rcS.local.disabled"))
			QFile::remove("/data/rcS.local.disabled");
		QFile::rename("/data/rcS.local", "/data/rcS.local.disabled");
	}
}
