#include <QJsonDocument>
#include <QtTest>

#include "node_red_flow_file.hpp"

using namespace NodeRedFlowFile;

// Formatted like Node-RED with flowFilePretty, keys deliberately not sorted.
static const QByteArray prettyFlows =
	"[\n"
	"    {\n"
	"        \"id\": \"tab1\",\n"
	"        \"type\": \"tab\",\n"
	"        \"label\": \"Flow 1\",\n"
	"        \"disabled\": false,\n"
	"        \"info\": \"{\\\"not\\\": \\\"a node\\\"} \\\\\\\"\",\n"
	"        \"env\": []\n"
	"    },\n"
	"    {\n"
	"        \"id\": \"inject1\",\n"
	"        \"type\": \"inject\",\n"
	"        \"z\": \"tab1\",\n"
	"        \"props\": [{\"p\": \"payload\", \"disabled\": true}],\n"
	"        \"repeat\": 9007199254740993,\n"
	"        \"wires\": [[\"debug1\"]]\n"
	"    },\n"
	"    {\n"
	"        \"id\": \"tab2\",\n"
	"        \"type\": \"tab\",\n"
	"        \"label\": \"\",\n"
	"        \"info\": \"\"\n"
	"    }\n"
	"]\n";

class TestNodeRedFlowFile : public QObject {
	Q_OBJECT

private slots:
	void listFlows()
	{
		bool ok;
		QJsonArray flows = NodeRedFlowFile::listFlows(prettyFlows, &ok);
		QVERIFY(ok);
		QCOMPARE(flows.size(), 2);
		QCOMPARE(flows[0].toObject().value("id").toString(), "tab1");
		QCOMPARE(flows[0].toObject().value("enabled").toBool(), true);
		// An empty label falls back to the id.
		QCOMPARE(flows[1].toObject().value("label").toString(), "tab2");
		QCOMPARE(flows[1].toObject().value("enabled").toBool(), true);
	}

	void listInvalid()
	{
		bool ok;
		NodeRedFlowFile::listFlows("{\"id\": 1}", &ok);
		QVERIFY(!ok);
	}

	void replacesExistingValue()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab1", false}}, &out), Result::Ok);

		QByteArray expected = prettyFlows;
		expected.replace("\"label\": \"Flow 1\",\n        \"disabled\": false",
						 "\"label\": \"Flow 1\",\n        \"disabled\": true");
		QCOMPARE(out, expected);
	}

	void insertsMissingValue()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab2", false}}, &out), Result::Ok);

		QByteArray expected = prettyFlows;
		expected.replace("{\n        \"id\": \"tab2\"",
						 "{\n        \"disabled\": true,\n        \"id\": \"tab2\"");
		QCOMPARE(out, expected);
		QCOMPARE(NodeRedFlowFile::listFlows(out)[1].toObject().value("enabled").toBool(), false);
	}

	void insertsInCompactFile()
	{
		QByteArray compact = R"([{"id":"a","type":"tab"},{"id":"b","type":"tab","disabled":true}])";
		QByteArray out;
		QCOMPARE(setFlowsEnabled(compact, {{"a", false}, {"b", true}}, &out), Result::Ok);
		QCOMPARE(out, R"([{"disabled":true,"id":"a","type":"tab"},{"id":"b","type":"tab","disabled":false}])");
	}

	void leavesOtherNodesAlone()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab1", false}, {"tab2", false}}, &out), Result::Ok);

		// The disabled member of a nested object is not touched and large numbers keep
		// all their digits.
		QVERIFY(out.contains("\"props\": [{\"p\": \"payload\", \"disabled\": true}]"));
		QVERIFY(out.contains("9007199254740993"));
		QVERIFY(out.contains("\"info\": \"{\\\"not\\\": \\\"a node\\\"} \\\\\\\"\","));
		QVERIFY(!QJsonDocument::fromJson(out).isNull());
	}

	void unchanged()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab1", true}, {"tab2", true}}, &out), Result::Unchanged);
	}

	void invalidRequest()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled(prettyFlows, {}, &out), Result::InvalidRequest);
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab1", "false"}}, &out), Result::InvalidRequest);
	}

	void unknownFlow()
	{
		QByteArray out;
		// The whole request is rejected, also the valid part.
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"tab1", false}, {"inject1", false}}, &out), Result::UnknownFlow);
		QCOMPARE(setFlowsEnabled(prettyFlows, {{"nope", false}}, &out), Result::UnknownFlow);
	}

	void invalidFile()
	{
		QByteArray out;
		QCOMPARE(setFlowsEnabled("[{", {{"tab1", false}}, &out), Result::InvalidFile);
		QCOMPARE(setFlowsEnabled("{}", {{"tab1", false}}, &out), Result::InvalidFile);
	}
};

QTEST_APPLESS_MAIN(TestNodeRedFlowFile)

#include "tst_node_red_flow_file.moc"
