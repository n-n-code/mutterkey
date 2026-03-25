#include "control/daemoncontrolprotocol.h"

#include <QtTest/QTest>

class DaemonControlProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void requestRoundTrip();
    void responseRoundTrip();
    void rejectsUnknownMethod();
    void rejectsMissingRequestId();
};

void DaemonControlProtocolTest::requestRoundTrip()
{
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

void DaemonControlProtocolTest::rejectsUnknownMethod()
{
    DaemonControlRequest request;
    QString errorMessage;
    QVERIFY(!parseDaemonControlRequest("{\"version\":1,\"request_id\":\"x\",\"method\":\"explode\"}\n", &request, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Unsupported daemon control method")));
}

void DaemonControlProtocolTest::rejectsMissingRequestId()
{
    DaemonControlResponse response;
    QString errorMessage;
    QVERIFY(!parseDaemonControlResponse("{\"version\":1,\"ok\":true,\"result\":{}}\n", &response, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("request_id")));
}

QTEST_APPLESS_MAIN(DaemonControlProtocolTest)

#include "daemoncontrolprotocoltest.moc"
