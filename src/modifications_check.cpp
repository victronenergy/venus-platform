#include <QDebug>
#include <QProcess>

#include "application.hpp"
#include "modifications_check.hpp"

int VeQItemModificationChecksStartCheck::setValue(const QVariant &value)
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
		mModChecks->itemGet("FsModifiedState")->produceValue(0);
	} else {
		mModChecks->itemGet("FsModifiedState")->produceValue(1);
	}

	// create variable to check if multiple files are present
	int systemHooksState = 0;
	if (QFile::exists("/data/rc.local.disabled")) {
		qDebug() << "[Modification checks] /data/rc.local.disabled present";
		systemHooksState += 1;
	}
	if (QFile::exists("/data/rcS.local.disabled")) {
		qDebug() << "[Modification checks] /data/rcS.local.disabled present";
		systemHooksState += 2;
	}
	if (QFile::exists("/data/rc.local")) {
		qDebug() << "[Modification checks] /data/rc.local present";
		systemHooksState += 4;
	}
	if (QFile::exists("/data/rcS.local")) {
		qDebug() << "[Modification checks] /data/rcS.local present";
		systemHooksState += 8;
	}
	if (QFile::exists("/run/venus/custom-rc")) {
		qDebug() << "[Modification checks] /run/venus/custom-rc present";
		systemHooksState += 16;
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

	return VeQItemAction::setValue(value);
}

ModificationChecks::ModificationChecks(VeQItem *modChecks, VeQItemSettings *settings, QObject *parent) :
	QObject(parent),
	mModChecks(modChecks),
	mSettings(settings)
{
	modChecks->itemGetOrCreate("DataPartitionFreeSpace");
	modChecks->itemGetOrCreate("FsModifiedState");
	modChecks->itemGetOrCreate("SystemHooksState");
	modChecks->itemGetOrCreate("SshKeyForRootPresent");

	VeQItem *item = mSettings->root()->itemGetOrCreate("Settings/System/ModificationChecks/AllModificationsEnabled");
	item->getValueAndChanges(this, SLOT(onAllModificationsEnabledChanged(QVariant)));

	item = modChecks->itemAddChild("StartCheck", new VeQItemModificationChecksStartCheck(modChecks));
	// execute the check once to populate the values
	item->setValue(1);
}

void ModificationChecks::onAllModificationsEnabledChanged(QVariant var)
{
	if (!var.isValid())
		return;

	if (var.toBool())
		restoreModifications();
	else
		disableModifications();
}

// enable all third party integrations
void ModificationChecks::restoreModifications()
{
	// recover previous service states
	if (serviceExists("node-red-venus")) {
		int nodeRed = mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/NodeRed")->getValue().toInt();
		if (nodeRed > 0) {
			qDebug() << "[Modification checks] Node-RED was enabled, restore state: " << nodeRed;
			mSettings->root()->itemGet("Settings/Services/NodeRed")->setValue(nodeRed);
			mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/NodeRed")->setValue(0);
		}
	}

	// recover previous service states
	if (serviceExists("signalk-server") && mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/SignalK")->getValue().toInt() == 1) {
		qDebug() << "[Modification checks] SignalK was enabled, restore state";
		mSettings->root()->itemGet("Settings/Services/SignalK")->setValue(1);
		mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/SignalK")->setValue(0);
	}

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

void ModificationChecks::disableModifications()
{
	// disable all third party integrations
	// set Settings/Services/NodeRed to 0 and save previous state
	if (serviceExists("node-red-venus")) {
		int nodeRed = mSettings->root()->itemGet("Settings/Services/NodeRed")->getValue().toInt();
		if (nodeRed > 0) {
			qDebug() << "[Modification checks] Node-RED is enabled, save state and disable it";
			mSettings->root()->itemGet("Settings/Services/NodeRed")->setValue(0);
			mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/NodeRed")->setValue(nodeRed);
		}
	}

	// set Settings/Services/SignalK to 0 and save previous state
	if (serviceExists("signalk-server") && mSettings->root()->itemGet("Settings/Services/SignalK")->getValue().toInt() == 1) {
		qDebug() << "[Modification checks] SignalK was enabled, save state and disable it";
		mSettings->root()->itemGet("Settings/Services/SignalK")->setValue(0);
		mSettings->root()->itemGet("Settings/System/ModificationChecks/PreviousState/SignalK")->setValue(1);
	}

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
