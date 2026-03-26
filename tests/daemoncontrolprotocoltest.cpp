#include "control/daemoncontrolprotocol.h"

#include <QtTest/QTest>

namespace {

class DaemonControlProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void requestRoundTrip();
    void responseRoundTrip();
    void rejectsInvalidRequests_data();
    void rejectsInvalidRequests();
    void rejectsInvalidResponses_data();
    void rejectsInvalidResponses();
};

} // namespace

void DaemonControlProtocolTest::requestRoundTrip()
{
    // WHAT: Verify that a daemon-control request survives serialize/parse round-tripping.
    // HOW: Build a request object, serialize it to protocol text, parse it back, and
    // compare the parsed version, request ID, and method with the original values.
    // WHY: The request protocol is the contract between local clients and the daemon, so
    // round-trip stability is necessary for reliable control messages.
    DaemonControlRequest request;
    request.requestId = QStringLiteral("abc123");
    request.method = DaemonControlMethod::GetStatus;

    DaemonControlRequest parsedRequest;
    QString errorMessage;
    QVERIFY(parseDaemonControlRequest(serializeDaemonControlRequest(request), &parsedRequest, &errorMessage));
    QCOMPARE(parsedRequest.version, 1);
    QCOMPARE(parsedRequest.requestId, request.requestId);
    QCOMPARE(parsedRequest.method, request.method);
}

void DaemonControlProtocolTest::responseRoundTrip()
{
    // WHAT: Verify that a daemon-control response survives serialize/parse round-tripping.
    // HOW: Build a successful response with result data, serialize it, parse it back, and
    // confirm that the important fields are preserved.
    // WHY: Clients depend on these responses to interpret daemon state correctly, so the
    // protocol must preserve data without loss or shape changes.
    DaemonControlResponse response;
    response.requestId = QStringLiteral("pong123");
    response.success = true;
    response.result.insert(QStringLiteral("daemon_running"), true);

    DaemonControlResponse parsedResponse;
    QString errorMessage;
    QVERIFY(parseDaemonControlResponse(serializeDaemonControlResponse(response), &parsedResponse, &errorMessage));
    QCOMPARE(parsedResponse.version, 1);
    QCOMPARE(parsedResponse.requestId, response.requestId);
    QVERIFY(parsedResponse.success);
    QVERIFY(parsedResponse.result.value(QStringLiteral("daemon_running")).toBool());
}

void DaemonControlProtocolTest::rejectsInvalidRequests_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("invalid json")
        << QByteArray("{")
        << QStringLiteral("Invalid JSON payload");
    QTest::newRow("missing version")
        << QByteArray("{\"request_id\":\"x\",\"method\":\"ping\"}\n")
        << QStringLiteral("Missing numeric protocol version");
    QTest::newRow("unsupported version")
        << QByteArray("{\"version\":2,\"request_id\":\"x\",\"method\":\"ping\"}\n")
        << QStringLiteral("Unsupported protocol version");
    QTest::newRow("missing request id")
        << QByteArray("{\"version\":1,\"method\":\"ping\"}\n")
        << QStringLiteral("Missing non-empty request_id");
    QTest::newRow("blank request id")
        << QByteArray("{\"version\":1,\"request_id\":\"   \",\"method\":\"ping\"}\n")
        << QStringLiteral("Missing non-empty request_id");
    QTest::newRow("unknown method")
        << QByteArray("{\"version\":1,\"request_id\":\"x\",\"method\":\"explode\"}\n")
        << QStringLiteral("Unsupported daemon control method");
}

void DaemonControlProtocolTest::rejectsInvalidRequests()
{
    // WHAT: Verify that malformed or unsupported request payloads are rejected.
    // HOW: Parse a table of invalid request payloads and confirm that each one fails with
    // the expected error category.
    // WHY: The daemon control API should fail loudly on broken input rather than accepting
    // ambiguous or unsupported requests.
    // NOLINTNEXTLINE(misc-const-correctness): QFETCH declares a mutable local by macro design.
    QFETCH(QByteArray, payload);
    // NOLINTNEXTLINE(misc-const-correctness): QFETCH declares a mutable local by macro design.
    QFETCH(QString, expectedError);

    DaemonControlRequest request;
    QString errorMessage;
    QVERIFY(!parseDaemonControlRequest(payload, &request, &errorMessage));
    QVERIFY2(errorMessage.contains(expectedError), qPrintable(errorMessage));
}

void DaemonControlProtocolTest::rejectsInvalidResponses_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("missing request id")
        << QByteArray("{\"version\":1,\"ok\":true,\"result\":{}}\n")
        << QStringLiteral("request_id");
    QTest::newRow("missing ok")
        << QByteArray("{\"version\":1,\"request_id\":\"x\",\"result\":{}}\n")
        << QStringLiteral("Missing boolean ok field");
    QTest::newRow("success without result")
        << QByteArray("{\"version\":1,\"request_id\":\"x\",\"ok\":true}\n")
        << QStringLiteral("Successful response is missing result object");
    QTest::newRow("failed without error")
        << QByteArray("{\"version\":1,\"request_id\":\"x\",\"ok\":false}\n")
        << QStringLiteral("Failed response is missing error text");
    QTest::newRow("blank failed error")
        << QByteArray("{\"version\":1,\"request_id\":\"x\",\"ok\":false,\"error\":\"   \"}\n")
        << QStringLiteral("Failed response is missing error text");
    QTest::newRow("unsupported version")
        << QByteArray("{\"version\":9,\"request_id\":\"x\",\"ok\":true,\"result\":{}}\n")
        << QStringLiteral("Unsupported protocol version");
}

void DaemonControlProtocolTest::rejectsInvalidResponses()
{
    // WHAT: Verify that malformed or unsupported response payloads are rejected.
    // HOW: Parse a table of invalid response payloads and confirm that each one fails with
    // the expected error category.
    // WHY: Client code depends on strict response validation to avoid trusting broken daemon
    // replies as if they were valid control-plane state.
    // NOLINTNEXTLINE(misc-const-correctness): QFETCH declares a mutable local by macro design.
    QFETCH(QByteArray, payload);
    // NOLINTNEXTLINE(misc-const-correctness): QFETCH declares a mutable local by macro design.
    QFETCH(QString, expectedError);

    DaemonControlResponse response;
    QString errorMessage;
    QVERIFY(!parseDaemonControlResponse(payload, &response, &errorMessage));
    QVERIFY2(errorMessage.contains(expectedError), qPrintable(errorMessage));
}

QTEST_APPLESS_MAIN(DaemonControlProtocolTest)

#include "daemoncontrolprotocoltest.moc"
