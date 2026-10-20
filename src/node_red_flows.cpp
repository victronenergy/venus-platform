#include <sys/stat.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include "node_red_flow_file.hpp"
#include "node_red_flows.hpp"

static const int stopTimeoutMs = 30000;

NodeRedFlows::NodeRedFlows(VeQItem *nodeRedItem, DaemonToolsService *nodeRed, const QString &userDir) :
	QObject(),
	mNodeRed(nodeRed),
	mUserDir(userDir),
	mFlowFile(userDir + "/flows.json")
{
	VeQItem *flows = nodeRedItem->itemGetOrCreate("Flows");
	mListItem = flows->itemGetOrCreate("List");
	flows->itemAddChild("SetEnabled", new NodeRedFlowsSetEnabledItem(this));

	// Node-RED replaces flows.json on every deploy, which drops a watch on the file
	// itself. Watch the directory instead and coalesce the burst of changes.
	mUpdateTimer.setSingleShot(true);
	mUpdateTimer.setInterval(500);
	connect(&mUpdateTimer, &QTimer::timeout, this, &NodeRedFlows::update);
	connect(&mWatcher, &QFileSystemWatcher::directoryChanged, &mUpdateTimer, qOverload<>(&QTimer::start));

	mStopTimer.setInterval(100);
	connect(&mStopTimer, &QTimer::timeout, this, &NodeRedFlows::checkStopped);

	update();
}

// The user dir doesn't exist before the first start of Node-RED, so watch its
// parent until it is created.
void NodeRedFlows::watch()
{
	QString parent = QFileInfo(mUserDir).absolutePath();

	if (QDir(mUserDir).exists()) {
		if (mWatcher.directories().contains(parent))
			mWatcher.removePath(parent);
		if (!mWatcher.directories().contains(mUserDir))
			mWatcher.addPath(mUserDir);
	} else if (QDir(parent).exists() && !mWatcher.directories().contains(parent)) {
		mWatcher.addPath(parent);
	}
}

void NodeRedFlows::update()
{
	watch();

	QString value;
	QFile file(mFlowFile);

	if (!file.exists()) {
		value = "[]";
	} else if (file.open(QFile::ReadOnly)) {
		bool ok;
		QJsonArray flows = NodeRedFlowFile::listFlows(file.readAll(), &ok);
		if (ok)
			value = QString(QJsonDocument(flows).toJson(QJsonDocument::Compact));
		else
			qWarning() << "[NodeRedFlows]" << mFlowFile << "is not a valid flows file";
	} else {
		qWarning() << "[NodeRedFlows] could not open" << mFlowFile;
	}

	// Keep the last known flows when the file can't be read.
	if (value.isNull()) {
		if (mListItem->getValue().isValid())
			return;
		value = "[]";
	}

	if (mListItem->getValue() != value)
		mListItem->produceValue(value);
}

static int toErrorCode(NodeRedFlowFile::Result result)
{
	switch (result) {
	case NodeRedFlowFile::Result::Ok:
	case NodeRedFlowFile::Result::Unchanged:
		return 0;
	case NodeRedFlowFile::Result::InvalidRequest:
		return -1;
	case NodeRedFlowFile::Result::InvalidFile:
		return -2;
	case NodeRedFlowFile::Result::UnknownFlow:
		return -3;
	}
	return -2;
}

static bool readFile(const QString &path, QByteArray *data)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly))
		return false;
	*data = file.readAll();
	return true;
}

// Expects {"<flow id>": <enabled>, ...}. Returns -1 for an invalid request, -2 when
// flows.json can't be read or parsed, -3 for an unknown flow id and -4 when writing
// fails. Node-RED only reads flows.json at startup, so when it is running it is
// stopped first. That happens asynchronously: the request is accepted once it is
// validated and the result shows up in Flows/List.
int NodeRedFlows::setFlowsEnabled(const QVariant &value)
{
	QJsonParseError error;
	QJsonDocument request = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !request.isObject()) {
		qWarning() << "[NodeRedFlows] invalid request" << value;
		return -1;
	}
	QJsonObject changes = request.object();

	// Check the request against the current file first, so an invalid or no-op
	// request doesn't restart Node-RED.
	QByteArray data;
	QByteArray patched;
	if (!readFile(mFlowFile, &data))
		return -2;
	NodeRedFlowFile::Result result = NodeRedFlowFile::setFlowsEnabled(data, changes, &patched);
	if (result != NodeRedFlowFile::Result::Ok && result != NodeRedFlowFile::Result::Unchanged)
		return toErrorCode(result);

	// Node-RED is already being stopped for an earlier request, apply this one with it.
	// Even when it changes nothing in the current file, it may undo a pending change.
	if (mStopTimer.isActive()) {
		for (auto it = changes.constBegin(); it != changes.constEnd(); ++it)
			mPendingChanges.insert(it.key(), it.value());
		return 0;
	}

	if (result == NodeRedFlowFile::Result::Unchanged)
		return 0;

	if (!mNodeRed->isUp())
		return writeFlows(changes);

	qInfo() << "[NodeRedFlows] stopping Node-RED to update the flows";
	mPendingChanges = changes;
	mNodeRed->stop();
	mStopping.start();
	mStopTimer.start();
	return 0;
}

void NodeRedFlows::checkStopped()
{
	if (mNodeRed->isUp()) {
		if (mStopping.elapsed() < stopTimeoutMs)
			return;
		qCritical() << "[NodeRedFlows] Node-RED did not stop, flows are not changed";
	} else {
		// A deploy might have happened in the meantime, writeFlows applies the
		// changes to the latest file.
		writeFlows(mPendingChanges);
	}

	mStopTimer.stop();
	mPendingChanges = QJsonObject();
	mNodeRed->start();
}

int NodeRedFlows::writeFlows(const QJsonObject &changes)
{
	QByteArray data;
	QByteArray patched;
	if (!readFile(mFlowFile, &data)) {
		qCritical() << "[NodeRedFlows] could not read" << mFlowFile;
		return -2;
	}

	NodeRedFlowFile::Result result = NodeRedFlowFile::setFlowsEnabled(data, changes, &patched);
	if (result != NodeRedFlowFile::Result::Ok) {
		if (result != NodeRedFlowFile::Result::Unchanged)
			qCritical() << "[NodeRedFlows] could not apply" << changes << "to" << mFlowFile;
		return toErrorCode(result);
	}

	struct stat st;
	if (stat(QFile::encodeName(mFlowFile).constData(), &st) != 0) {
		qCritical() << "[NodeRedFlows] could not stat" << mFlowFile;
		return -4;
	}

	// venus-platform runs as root. Give the new file the owner and permissions of the
	// current one before it replaces it, so Node-RED can still deploy afterwards.
	QSaveFile file(mFlowFile);
	if (!file.open(QFile::WriteOnly)) {
		qCritical() << "[NodeRedFlows] could not write" << mFlowFile << file.errorString();
		return -4;
	}
	if (fchown(file.handle(), st.st_uid, st.st_gid) != 0 || fchmod(file.handle(), st.st_mode & 07777) != 0) {
		qCritical() << "[NodeRedFlows] could not set the owner and permissions of" << mFlowFile;
		return -4;
	}
	if (file.write(patched) != patched.size() || !file.commit()) {
		qCritical() << "[NodeRedFlows] could not write" << mFlowFile << file.errorString();
		return -4;
	}

	qInfo() << "[NodeRedFlows] updated" << mFlowFile;
	update();
	return 0;
}

int NodeRedFlowsSetEnabledItem::setValue(const QVariant &value)
{
	int ret = mFlows->setFlowsEnabled(value);
	if (ret < 0)
		return ret;

	return VeQItemAction::setValue(value);
}
