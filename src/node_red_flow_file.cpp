#include <QJsonDocument>
#include <QJsonValue>

#include "node_red_flow_file.hpp"

namespace NodeRedFlowFile {

static bool parse(const QByteArray &data, QJsonArray *nodes)
{
	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(data, &error);

	// Node-RED stores the flows as a plain array of nodes.
	if (error.error != QJsonParseError::NoError || !doc.isArray())
		return false;

	*nodes = doc.array();
	return true;
}

static bool isFlow(const QJsonValue &node)
{
	return node.isObject() && node.toObject().value("type").toString() == "tab";
}

QJsonArray listFlows(const QByteArray &data, bool *ok)
{
	QJsonArray nodes;
	QJsonArray flows;
	bool valid = parse(data, &nodes);

	if (ok)
		*ok = valid;

	for (const QJsonValue &node: std::as_const(nodes)) {
		if (!isFlow(node))
			continue;

		QJsonObject tab = node.toObject();
		QString id = tab.value("id").toString();
		QString label = tab.value("label").toString();

		flows.append(QJsonObject {
			{"id", id},
			{"label", label.isEmpty() ? id : label},
			{"enabled", !tab.value("disabled").toBool(false)}
		});
	}

	return flows;
}

Result setFlowsEnabled(const QByteArray &data, const QJsonObject &changes, QByteArray *out)
{
	if (changes.isEmpty())
		return Result::InvalidRequest;

	for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
		if (!it.value().isBool())
			return Result::InvalidRequest;
	}

	QJsonArray nodes;
	if (!parse(data, &nodes))
		return Result::InvalidFile;

	// Validate all ids first, a request is applied completely or not at all.
	QStringList flowIds;
	for (const QJsonValue &node: std::as_const(nodes)) {
		if (isFlow(node))
			flowIds.append(node.toObject().value("id").toString());
	}
	for (const QString &id: changes.keys()) {
		if (!flowIds.contains(id))
			return Result::UnknownFlow;
	}

	bool changed = false;
	for (qsizetype i = 0; i < nodes.size(); i++) {
		if (!isFlow(nodes[i]))
			continue;

		QJsonObject tab = nodes[i].toObject();
		QString id = tab.value("id").toString();
		if (!changes.contains(id))
			continue;

		bool disabled = !changes.value(id).toBool();
		if (tab.value("disabled").toBool(false) == disabled)
			continue;

		tab.insert("disabled", disabled);
		nodes[i] = tab;
		changed = true;
	}

	if (!changed)
		return Result::Unchanged;

	// Node-RED on Venus uses flowFilePretty, which indents with 4 spaces as well.
	*out = QJsonDocument(nodes).toJson(QJsonDocument::Indented);
	return Result::Ok;
}

} // namespace NodeRedFlowFile
