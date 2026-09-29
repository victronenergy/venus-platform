#include <sys/stat.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include "node_red_flow_file.hpp"
#include "node_red_flows.hpp"

using namespace std::chrono_literals;

NodeRedFlows::NodeRedFlows(VeQItem *nodeRedItem, DaemonToolsService *nodeRed, VeQItem *nodeRedMode,
						   const QString &userDir) :
	QObject(),
	mNodeRed(nodeRed),
	mNodeRedMode(nodeRedMode),
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

	update();
}

// The user dir might not exist yet, e.g. before the first start of Node-RED, so
// also watch its parent to notice it being created.
void NodeRedFlows::watch()
{
	QStringList paths = {QFileInfo(mUserDir).absolutePath(), mUserDir};
	for (const QString &path: paths) {
		if (QDir(path).exists() && !mWatcher.directories().contains(path))
			mWatcher.addPath(path);
	}
}

void NodeRedFlows::update()
{
	watch();

	QVariant value;
	QFile file(mFlowFile);

	if (!file.exists()) {
		value = QString("[]");
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

	if (mListItem->getValue() != value)
		mListItem->produceValue(value);
}

void NodeRedFlows::restoreOwnership(const QString &path, uint uid, uint gid, uint mode)
{
	QByteArray name = QFile::encodeName(path);

	if (chown(name.constData(), uid, gid) != 0)
		qCritical() << "[NodeRedFlows] could not restore ownership of" << path;
	if (chmod(name.constData(), mode & 07777) != 0)
		qCritical() << "[NodeRedFlows] could not restore permissions of" << path;
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

// Expects {"<flow id>": <enabled>, ...}. Node-RED only reads flows.json at startup,
// so when it is running it is stopped, the file is patched and it is started again.
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
	if (result != NodeRedFlowFile::Result::Ok)
		return toErrorCode(result);

	bool running = mNodeRedMode->getValue().toInt() != 0;
	if (running) {
		qInfo() << "[NodeRedFlows] stopping Node-RED to update the flows";
		mNodeRed->stop();
		if (!mNodeRed->waitTillDown(30s)) {
			qCritical() << "[NodeRedFlows] Node-RED did not stop, flows are not changed";
			mNodeRed->start();
			return -4;
		}

		// A deploy might have happened in the meantime, so apply to the latest file.
		if (!readFile(mFlowFile, &data)) {
			mNodeRed->start();
			return -2;
		}
		result = NodeRedFlowFile::setFlowsEnabled(data, changes, &patched);
	}

	int ret = toErrorCode(result);
	if (result == NodeRedFlowFile::Result::Ok) {
		struct stat st;
		bool haveStat = stat(QFile::encodeName(mFlowFile).constData(), &st) == 0;

		QSaveFile file(mFlowFile);
		if (file.open(QFile::WriteOnly) && file.write(patched) == patched.size() && file.commit()) {
			// venus-platform runs as root, keep the file owned by the nodered user.
			if (haveStat)
				restoreOwnership(mFlowFile, st.st_uid, st.st_gid, st.st_mode);
			qInfo() << "[NodeRedFlows] updated" << mFlowFile;
		} else {
			qCritical() << "[NodeRedFlows] could not write" << mFlowFile << file.errorString();
			ret = -4;
		}
	}

	if (running)
		mNodeRed->start();

	update();
	return ret;
}

int NodeRedFlowsSetEnabledItem::setValue(const QVariant &value)
{
	int ret = mFlows->setFlowsEnabled(value);
	if (ret < 0)
		return ret;

	return VeQItemAction::setValue(value);
}
