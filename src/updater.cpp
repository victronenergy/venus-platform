#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QLocalSocket>

#include "application.hpp"
#include "updater.hpp"
#include <velib/qt/ve_qitem_utils.hpp>
#include <velib/qt/firmware_updater_data.hpp>

QString updateFile = QString::fromUtf8("/var/run/swupdate-status");
QString updateScript = QString::fromUtf8("/opt/victronenergy/swupdate-scripts/check-updates.sh");
QString versionFile = QString::fromUtf8("/var/run/versions");
QString setVersionScript = QString::fromUtf8("/opt/victronenergy/swupdate-scripts/set-version.sh");

typedef enum {
	IDLE,
	START,
	RUN,
	SUCCESS,
	FAILURE,
	DOWNLOAD,
	DONE,
} RECOVERY_STATUS;

struct progress_msg {
	unsigned int	magic;			/* Magic Number */
	RECOVERY_STATUS	status;			/* Update Status (Running, Failure) */
	unsigned int	dwl_percent;	/* % downloaded data */
	unsigned int	nsteps;			/* No. total of steps */
	unsigned int	cur_step;		/* Current step index */
	unsigned int	cur_percent;	/* % in current step */
	char			cur_image[256];	/* Name of image to be installed */
	char			hnd_name[64];	/* Name of running hanler */
};

SwuUpdateMonitor::SwuUpdateMonitor(const QString &cmd, const QStringList &args, VeQItem *progress, VeQItem *state) :
	mProgress(progress),
	mState(state)
{
	// Note: it takes several seconds before the state is reported by the check-update
	// script, so report the state directly as installing.
	mProgress->produceValue(0);
	mState->produceValue(FirmwareUpdaterData::DownloadingAndInstalling);

	QProcess *proc = Application::spawn(cmd, args);
	setParent(proc);
	mWatcher.addPath("/tmp");

	connect(&mSocket, SIGNAL(readyRead()), SLOT(onReadReady()));
	connect(&mWatcher, SIGNAL(directoryChanged(QString)), SLOT(onTmpChanged()));

	openSocket();
}

void SwuUpdateMonitor::onTmpChanged()
{
	openSocket();
	if (mSocket.isOpen())
		mWatcher.removePath("/tmp");
}

void SwuUpdateMonitor::onReadReady()
{
	struct progress_msg msg;
	qint64 length = mSocket.read((char *) &msg, sizeof(msg));
	if (length < (qint64) sizeof(msg))
		return;
	unsigned perc = msg.status == SUCCESS ? 100 : msg.dwl_percent;
	mProgress->produceValue(perc);
}

void SwuUpdateMonitor::openSocket()
{
	if (mSocket.isOpen())
		return;
	mSocket.connectToServer("/tmp/swupdateprog", QLocalSocket::ReadOnly);
}

int VeQItemCheckUpdate::setValue(const QVariant &value)
{
	qDebug() << "[Updater] checkUpdate";

	// NOTE: make sure the state is changed to checking, especially when checking
	// for offline updates, the state file can change so rapidly that there is no
	// time to read the state from it.
	mState->produceValue(FirmwareUpdaterData::Checking);
	QStringList arguments = QStringList() << "-check";
	if (mOffline)
		arguments << "-offline" << "-force";
	Application::spawn(updateScript, arguments);

	return VeQItemAction::setValue(value);
}

int VeQItemSwitchVersion::setValue(const QVariant &value)
{
	qDebug() << "Updater::switchVersion";
	Application::spawn(setVersionScript, QStringList() << "2");
	return VeQItemAction::setValue(value);
}

int VeQItemDoUpdate::setValue(const QVariant &value)
{
	QStringList arguments = QStringList() << "-update";

	if (mOffline)
		arguments << "-offline" << "-force";

	qDebug() << "[Updater] Installing firmware";
	new SwuUpdateMonitor(updateScript, arguments, mProgress, mState);

	return VeQItemAction::setValue(value);
}

Updater::Updater(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	touchFile(updateFile);
	touchFile(versionFile);

	mUpdateWatcher.addPath(updateFile);
	mUpdateWatcher.addPath(versionFile);

	mItem = parentItem->itemGetOrCreate("Firmware");

	VeQItem *state = mItem->itemAddChild("State", new VeQItemUpdateState());
	VeQItem *progress = mItem->itemAddChild("Progress", new VeQItemQuantity(0, "%"));
	VeQItem *installed = mItem->itemGetOrCreate("Installed");
	installed->itemAddChild("Version", new VeQItemQuantity());
	installed->itemAddChild("Build", new VeQItemQuantity());

	VeQItem *backup = mItem->itemGetOrCreate("Backup");
	backup->itemAddChild("AvailableVersion", new VeQItemQuantity());
	backup->itemAddChild("AvailableBuild", new VeQItemQuantity());

	getRootfsInfoFromFile(versionFile);

	connect(&mUpdateWatcher, SIGNAL(fileChanged(QString)),
			SLOT(checkFile(QString)));

	mItem->itemGetOrCreate("Backup")->itemAddChild("Activate", new VeQItemSwitchVersion());

	VeQItem *online = mItem->itemGetOrCreate("Online");
	online->itemAddChild("AvailableVersion", new VeQItemQuantity());
	online->itemAddChild("AvailableBuild", new VeQItemQuantity());
	online->itemAddChild("Check", new VeQItemCheckUpdate(false, state));
	online->itemAddChild("Install", new VeQItemDoUpdate(false, progress, state));

	VeQItem *offline = mItem->itemGetOrCreate("Offline");
	offline->itemAddChild("AvailableVersion", new VeQItemQuantity());
	offline->itemAddChild("AvailableBuild", new VeQItemQuantity());
	offline->itemAddChild("Check", new VeQItemCheckUpdate(true, state));
	offline->itemAddChild("Install", new VeQItemDoUpdate(true, progress, state));
}

void Updater::getUpdateInfoFromFile(QString const &fileName)
{
	QFile file(fileName);

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	QString contents = QString::fromUtf8(file.readAll());
	QStringList lines = contents.split("\n");
	if (lines.length() < 4)
		return;

	// Line 0 = status, line 1 = "timestamp<space>version"
	int status = lines[0].trimmed().toInt() + FirmwareUpdaterData::Idle;

	QVariant build, version;
	getVersionInfoFromLine(lines[1], build, version);
	mItem->itemGetOrCreateAndProduce("Online/AvailableVersion", version);
	mItem->itemGetOrCreateAndProduce("Online/AvailableBuild", build);

	getVersionInfoFromLine(lines[2], build, version);
	mItem->itemGetOrCreateAndProduce("Offline/AvailableVersion", version);
	mItem->itemGetOrCreateAndProduce("Offline/AvailableBuild", build);

	// Installing starts with checking for an update first, ignore that state.
	VeQItem *state = mItem->itemGet("State");
	if (!(state->getLocalValue().toInt() == FirmwareUpdaterData::DownloadingAndInstalling &&
			status == FirmwareUpdaterData::Checking))
		state->produceValue(status);
}

void Updater::getRootfsInfoFromFile(const QString &fileName)
{
	QFile file(fileName);

	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		// Line 0 is always the running version
		// both lines in "timestamp<space>version" format
		QVariant build, version;
		getVersionInfoFromLine(file.readLine(), build, version);
		mItem->itemGetOrCreateAndProduce("Installed/Version", version);
		mItem->itemGetOrCreateAndProduce("Installed/Build", build);

		getVersionInfoFromLine(file.readLine(), build, version);
		mItem->itemGetOrCreateAndProduce("Backup/AvailableVersion", version);
		mItem->itemGetOrCreateAndProduce("Backup/AvailableBuild", build);
	}
}

void Updater::checkFile(const QString &fileName)
{
	if (fileName == updateFile)
		getUpdateInfoFromFile(fileName);
	if (fileName == versionFile)
		getRootfsInfoFromFile(fileName);
}

void Updater::touchFile(QString const &fileName)
{
	QFile file(fileName);
	if (!file.exists()) {
		file.open(QIODevice::ReadWrite);
		file.resize(0);
	}
}

void Updater::getVersionInfoFromLine(QString const &line, QVariant &build, QVariant &version)
{
	QStringList parts = line.trimmed().split(" ", QString::SkipEmptyParts);

	build = parts.length() >= 1 ? parts[0] : QVariant();
	version = parts.length() >= 2 ? parts[1] : QVariant();
}
