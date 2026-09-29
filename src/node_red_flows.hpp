#pragma once

#include <QFileSystemWatcher>
#include <QTimer>

#include <veutil/qt/daemontools_service.hpp>
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

// Publishes the Node-RED flows (tabs) from flows.json on Services/NodeRed/Flows/List
// and allows enabling / disabling them with Services/NodeRed/Flows/SetEnabled.
class NodeRedFlows : public QObject {
	Q_OBJECT

public:
	NodeRedFlows(VeQItem *nodeRedItem, DaemonToolsService *nodeRed, VeQItem *nodeRedMode,
				 const QString &userDir = "/data/home/nodered/.node-red");

	int setFlowsEnabled(const QVariant &value);

private slots:
	void update();

private:
	void watch();
	void restoreOwnership(const QString &path, uint uid, uint gid, uint mode);

	QFileSystemWatcher mWatcher;
	QTimer mUpdateTimer;
	VeQItem *mListItem;
	DaemonToolsService *mNodeRed;
	VeQItem *mNodeRedMode;
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
