#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Pure helpers around the Node-RED flows.json file. They only depend on QtCore,
// so they can be tested without D-Bus.
namespace NodeRedFlowFile {

enum class Result {
	Ok,
	Unchanged,
	InvalidRequest,
	InvalidFile,
	UnknownFlow
};

// Returns the flows (tabs) as [{"id": .., "label": .., "enabled": ..}], in file order.
// Sets ok to false if data is not a valid flows file.
QJsonArray listFlows(const QByteArray &data, bool *ok = nullptr);

// Applies {"<flow id>": <enabled>, ...} to the flows file in data. On Ok, out
// contains the new file contents. Unchanged means every flow already had the
// requested state, nothing needs to be written.
Result setFlowsEnabled(const QByteArray &data, const QJsonObject &changes, QByteArray *out);

} // namespace NodeRedFlowFile
