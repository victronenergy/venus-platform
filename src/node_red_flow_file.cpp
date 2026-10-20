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

// A minimal scanner over the raw file, used to patch it in place. It assumes valid
// JSON, so it is only used after parse() succeeded on the same data.

struct Span {
	qsizetype begin;
	qsizetype end;
};

static bool isSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static qsizetype skipSpace(const QByteArray &data, qsizetype pos)
{
	while (pos < data.size() && isSpace(data[pos]))
		pos++;
	return pos;
}

// pos is at the opening quote, returns the position after the closing quote.
static qsizetype skipString(const QByteArray &data, qsizetype pos)
{
	for (pos++; pos < data.size(); pos++) {
		if (data[pos] == '\\')
			pos++;
		else if (data[pos] == '"')
			return pos + 1;
	}
	return pos;
}

// pos is at the first character of a value, returns the position after it.
static qsizetype skipValue(const QByteArray &data, qsizetype pos)
{
	char c = data[pos];

	if (c == '"')
		return skipString(data, pos);

	if (c == '{' || c == '[') {
		int depth = 0;
		while (pos < data.size()) {
			c = data[pos];
			if (c == '"') {
				pos = skipString(data, pos);
				continue;
			}
			if (c == '{' || c == '[') {
				depth++;
			} else if (c == '}' || c == ']') {
				if (--depth == 0)
					return pos + 1;
			}
			pos++;
		}
		return pos;
	}

	// number, true, false or null
	while (pos < data.size() && !isSpace(data[pos]) && data[pos] != ',' && data[pos] != '}' && data[pos] != ']')
		pos++;
	return pos;
}

// Returns the spans of the elements of the top level array.
static QList<Span> arrayElements(const QByteArray &data)
{
	QList<Span> spans;
	qsizetype pos = skipSpace(data, 0);

	if (pos >= data.size() || data[pos] != '[')
		return spans;

	pos = skipSpace(data, pos + 1);
	while (pos < data.size() && data[pos] != ']') {
		qsizetype end = skipValue(data, pos);
		spans.append({pos, end});
		pos = skipSpace(data, end);
		if (pos < data.size() && data[pos] == ',')
			pos = skipSpace(data, pos + 1);
	}
	return spans;
}

// Looks up a direct member of the object starting at pos and returns the span of its value.
static bool findMember(const QByteArray &data, qsizetype pos, const QByteArray &key, Span *value)
{
	pos = skipSpace(data, pos + 1);
	while (pos < data.size() && data[pos] == '"') {
		qsizetype keyEnd = skipString(data, pos);
		bool match = data.mid(pos + 1, keyEnd - pos - 2) == key;

		pos = skipSpace(data, keyEnd);
		pos = skipSpace(data, pos + 1);
		qsizetype valueEnd = skipValue(data, pos);
		if (match) {
			*value = {pos, valueEnd};
			return true;
		}

		pos = skipSpace(data, valueEnd);
		if (pos < data.size() && data[pos] == ',')
			pos = skipSpace(data, pos + 1);
	}
	return false;
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

	// Only the disabled values are patched in the original text, so the rest of the
	// file, like the order of the keys and the formatting, stays as Node-RED wrote it.
	QList<Span> spans = arrayElements(data);
	if (spans.size() != nodes.size())
		return Result::InvalidFile;

	QByteArray patched = data;
	bool changed = false;

	// Patch from the end, so the offsets of the nodes before it stay valid.
	for (qsizetype i = nodes.size() - 1; i >= 0; i--) {
		if (!isFlow(nodes[i]))
			continue;

		QJsonObject tab = nodes[i].toObject();
		QString id = tab.value("id").toString();
		if (!changes.contains(id))
			continue;

		bool disabled = !changes.value(id).toBool();
		if (tab.value("disabled").toBool(false) == disabled)
			continue;

		QByteArray text = disabled ? "true" : "false";
		Span value;
		if (findMember(data, spans[i].begin, "disabled", &value)) {
			patched.replace(value.begin, value.end - value.begin, text);
		} else {
			// Add it as the first member, with the same indentation as the others.
			qsizetype first = skipSpace(data, spans[i].begin + 1);
			QByteArray indent = data.mid(spans[i].begin + 1, first - spans[i].begin - 1);
			QByteArray separator = indent.isEmpty() ? ":" : ": ";
			patched.insert(first, "\"disabled\"" + separator + text + "," + indent);
		}
		changed = true;
	}

	if (!changed)
		return Result::Unchanged;

	*out = patched;
	return Result::Ok;
}

} // namespace NodeRedFlowFile
