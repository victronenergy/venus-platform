#pragma once

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QTimer>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

inline const QString NodeRedHome = QStringLiteral("/data/home/nodered");

// Publishes the Node-RED flows (tabs) from flows.json on Services/NodeRed/Flows/List
// and allows enabling / disabling them with Services/NodeRed/Flows/SetEnabled.
class NodeRedFlows : public QObject {
	Q_OBJECT

public:
	NodeRedFlows(VeQItem *nodeRedItem, DaemonToolsService *nodeRed,
				 const QString &userDir = NodeRedHome + "/.node-red");

	int setFlowsEnabled(const QVariant &value);

private slots:
	void update();
	void checkStopped();

private:
	void watch();
	int writeFlows(const QJsonObject &changes);

	QFileSystemWatcher mWatcher;
	QTimer mUpdateTimer;
	QTimer mStopTimer;
	QElapsedTimer mStopping;
	QJsonObject mPendingChanges;
	VeQItem *mListItem;
	DaemonToolsService *mNodeRed;
	QString mUserDir;
	QString mFlowFile;
};

class NodeRedFlowsSetEnabledItem : public VeQItemAction {
	Q_OBJECT

public:
	NodeRedFlowsSetEnabledItem(NodeRedFlows *flows) : VeQItemAction(), mFlows(flows) {}
	int setValue(const QVariant &value) override;

private:
	NodeRedFlows *mFlows;
};
