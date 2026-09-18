// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_protocol.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Obs;

namespace {

// The socket, replaced by a list. Every frame the client sends is recorded
// and every frame it receives is handed to it directly, so a row drives a
// whole session without a port, a server, or a wait.
class FakeTransport final : public ObsTransport {
    Q_OBJECT
public:
    QStringList sent;
    QStringList opened;
    int closes = 0;
    bool socketOpen = false;

    void open(const QString &url) override {
        opened.append(url);
        // The client must not assume open() connects synchronously; a row
        // calls connect() when it wants the edge.
    }
    void close() override {
        ++closes;
        const bool wasOpen = socketOpen;
        socketOpen = false;
        if (wasOpen) {
            Q_EMIT disconnected(QStringLiteral("closed"));
        }
    }
    void sendText(const QString &text) override { sent.append(text); }
    [[nodiscard]] bool isOpen() const override { return socketOpen; }

    // Test-side edges.
    void connectNow() {
        socketOpen = true;
        Q_EMIT connected();
    }
    void dropNow(const QString &reason = QStringLiteral("lost")) {
        socketOpen = false;
        Q_EMIT disconnected(reason);
    }
    void deliver(const QString &text) { Q_EMIT textReceived(text); }

    [[nodiscard]] QString lastSent() const {
        return sent.isEmpty() ? QString() : sent.last();
    }
    [[nodiscard]] QStringList sentRequestTypes() const {
        QStringList types;
        for (const QString &frame : sent) {
            const QJsonObject d = QJsonDocument::fromJson(frame.toUtf8())
                                      .object()
                                      .value(QStringLiteral("d"))
                                      .toObject();
            const QString type =
                d.value(QStringLiteral("requestType")).toString();
            if (!type.isEmpty()) {
                types.append(type);
            }
        }
        return types;
    }
    // The id the client generated for the last frame of `requestType`.
    [[nodiscard]] QString idFor(const QString &requestType) const {
        for (auto it = sent.crbegin(); it != sent.crend(); ++it) {
            const QJsonObject d = QJsonDocument::fromJson(it->toUtf8())
                                      .object()
                                      .value(QStringLiteral("d"))
                                      .toObject();
            if (d.value(QStringLiteral("requestType")).toString() ==
                requestType) {
                return d.value(QStringLiteral("requestId")).toString();
            }
        }
        return {};
    }
};

QString helloFrame(bool withAuthentication = false) {
    QJsonObject d{{QStringLiteral("obsWebSocketVersion"), QStringLiteral("5.6.2")},
                  {QStringLiteral("rpcVersion"), 1}};
    if (withAuthentication) {
        d.insert(QStringLiteral("authentication"),
                 QJsonObject{{QStringLiteral("challenge"), QStringLiteral("c")},
                             {QStringLiteral("salt"), QStringLiteral("s")}});
    }
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("op"), 0},
                                  {QStringLiteral("d"), d}})
            .toJson(QJsonDocument::Compact));
}

QString identifiedFrame(int version = 1) {
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{
                          {QStringLiteral("op"), 2},
                          {QStringLiteral("d"),
                           QJsonObject{{QStringLiteral("negotiatedRpcVersion"),
                                        version}}}})
            .toJson(QJsonDocument::Compact));
}

QString responseFrame(const QString &requestType, const QString &requestId,
                      bool ok, const QJsonObject &data = {}, int code = 100) {
    QJsonObject d{
        {QStringLiteral("requestType"), requestType},
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("requestStatus"),
         QJsonObject{{QStringLiteral("result"), ok},
                     {QStringLiteral("code"), code}}},
    };
    if (!data.isEmpty()) {
        d.insert(QStringLiteral("responseData"), data);
    }
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("op"), 7},
                                  {QStringLiteral("d"), d}})
            .toJson(QJsonDocument::Compact));
}

QString eventFrame(const QString &eventType, const QJsonObject &data) {
    return QString::fromUtf8(
        QJsonDocument(
            QJsonObject{{QStringLiteral("op"), 5},
                        {QStringLiteral("d"),
                         QJsonObject{{QStringLiteral("eventType"), eventType},
                                     {QStringLiteral("eventData"), data}}}})
            .toJson(QJsonDocument::Compact));
}

ClientTiming fastTiming() {
    ClientTiming timing;
    timing.reconnectMilliseconds = {10, 20, 40};
    timing.requestTimeoutMilliseconds = 60;
    timing.statisticsIntervalMilliseconds = 25;
    return timing;
}

} // namespace

class ObsClientTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void identifiesAndThenReadsEverythingOnce();
    void anObsThatWantsAPasswordWeDoNotHaveSaysSo();
    void anUnsupportedRpcVersionIsNamedNotRetriedBlindly();
    void aMalformedFrameDegradesAndClosesTheSocket();
    void eventsKeepTheSnapshotCurrentWithoutPolling();
    void aReplyToAnUnknownRequestIsIgnored();
    void operationsReportSentOrNotSent();
    void aLostConnectionClearsTheLiveStateAndBacksOff();
    void aTimedOutRequestAnswersTheCaller();
    void theConsoleMappingComesFromTheVendorNotFromSourceNames();
    void statisticsArePolledOnlyWhileAnOutputIsRunning();

private:
    void reachReady(FakeTransport &transport, ObsClient &client,
                    bool withAuthentication = false) {
        client.start(QStringLiteral("ws://127.0.0.1:4455"),
                     withAuthentication ? QStringLiteral("pw") : QString());
        transport.connectNow();
        transport.deliver(helloFrame(withAuthentication));
        transport.deliver(identifiedFrame());
        QCOMPARE(client.state(), ConnectionState::Ready);
    }
};

void ObsClientTest::identifiesAndThenReadsEverythingOnce() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    QSignalSpy states(&client, &ObsClient::stateChanged);

    client.start(QStringLiteral("ws://127.0.0.1:4455"), QString());
    QCOMPARE(transport.opened, QStringList{QStringLiteral("ws://127.0.0.1:4455")});
    QCOMPARE(client.state(), ConnectionState::Connecting);

    transport.connectNow();
    // Connected is not usable: OBS has not said hello yet.
    QCOMPARE(client.state(), ConnectionState::Authenticating);
    QVERIFY(transport.sent.isEmpty());

    transport.deliver(helloFrame());
    // The identify goes out before anything else.
    QCOMPARE(transport.sent.size(), 1);
    QCOMPARE(QJsonDocument::fromJson(transport.lastSent().toUtf8())
                 .object()
                 .value(QStringLiteral("op"))
                 .toInt(),
             1);
    QCOMPARE(client.state(), ConnectionState::Authenticating);

    transport.deliver(identifiedFrame());
    QCOMPARE(client.state(), ConnectionState::Ready);
    QCOMPARE(client.reasonCode(), QString());

    // One read of everything the snapshot holds, and no repetition: nothing
    // in this client polls.
    const QStringList types = transport.sentRequestTypes();
    QCOMPARE(types.count(QStringLiteral("GetVersion")), 1);
    QCOMPARE(types.count(QStringLiteral("GetRecordStatus")), 1);
    QCOMPARE(types.count(QStringLiteral("GetStreamStatus")), 1);
    QCOMPARE(types.count(QStringLiteral("GetVirtualCamStatus")), 1);
    QCOMPARE(types.count(QStringLiteral("GetSceneList")), 1);
    QCOMPARE(types.count(QStringLiteral("GetInputList")), 1);
    QCOMPARE(types.count(QStringLiteral("CallVendorRequest")), 1);

    // Those reads are the client's own; a surface is not told about them.
    QSignalSpy operations(&client, &ObsClient::operationFinished);
    transport.deliver(responseFrame(
        QStringLiteral("GetRecordStatus"),
        transport.idFor(QStringLiteral("GetRecordStatus")), true,
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputDuration"), 5000}}));
    QCOMPARE(operations.size(), 0);
    QVERIFY(client.snapshot().record.active);
    QCOMPARE(client.snapshot().record.durationMs, 5000);
    QVERIFY(states.size() >= 3); // connecting, authenticating, ready
}

void ObsClientTest::anObsThatWantsAPasswordWeDoNotHaveSaysSo() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    client.start(QStringLiteral("ws://127.0.0.1:4455"), QString());
    transport.connectNow();
    transport.deliver(helloFrame(true));

    // The reason is named while the identify is still in flight, so the user
    // is told "OBS wants a password" rather than watching a silent retry.
    QCOMPARE(client.reasonCode(),
             QString::fromLatin1(ReasonCodes::AuthRequired));
    // And it identifies anyway, so OBS's own refusal is the truth.
    QCOMPARE(transport.sent.size(), 1);
    const QJsonObject d = QJsonDocument::fromJson(transport.lastSent().toUtf8())
                              .object()
                              .value(QStringLiteral("d"))
                              .toObject();
    QVERIFY(d.contains(QStringLiteral("authentication")));
    QVERIFY(d.value(QStringLiteral("authentication")).toString().isEmpty());
}

void ObsClientTest::anUnsupportedRpcVersionIsNamedNotRetriedBlindly() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    client.start(QStringLiteral("ws://127.0.0.1:4455"), QString());
    transport.connectNow();
    transport.deliver(QString::fromUtf8(
        QJsonDocument(
            QJsonObject{
                {QStringLiteral("op"), 0},
                {QStringLiteral("d"),
                 QJsonObject{{QStringLiteral("rpcVersion"), 0}}}})
            .toJson(QJsonDocument::Compact)));
    QCOMPARE(client.reasonCode(), QString::fromLatin1(ReasonCodes::RpcVersion));
    QCOMPARE(transport.closes, 1);
    QVERIFY(transport.sent.isEmpty());

    // The same for an Identified that negotiated a version we do not speak.
    FakeTransport other;
    ObsClient second(other, fastTiming());
    second.start(QStringLiteral("ws://127.0.0.1:4455"), QString());
    other.connectNow();
    other.deliver(helloFrame());
    other.deliver(identifiedFrame(7));
    QCOMPARE(second.reasonCode(), QString::fromLatin1(ReasonCodes::RpcVersion));
}

void ObsClientTest::aMalformedFrameDegradesAndClosesTheSocket() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);

    transport.deliver(QStringLiteral("{not a frame"));
    QCOMPARE(client.state(), ConnectionState::Degraded);
    QCOMPARE(client.reasonCode(), QString::fromLatin1(ReasonCodes::Malformed));
    QVERIFY(transport.closes >= 1);

    // A frame only a client sends is equally a server fault.
    FakeTransport other;
    ObsClient second(other, fastTiming());
    reachReady(other, second);
    other.deliver(QStringLiteral(R"({"op":6,"d":{"requestType":"X"}})"));
    QCOMPARE(second.reasonCode(), QString::fromLatin1(ReasonCodes::Malformed));
}

void ObsClientTest::eventsKeepTheSnapshotCurrentWithoutPolling() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);
    transport.deliver(responseFrame(
        QStringLiteral("GetSceneList"),
        transport.idFor(QStringLiteral("GetSceneList")), true,
        QJsonObject{
            {QStringLiteral("currentProgramSceneName"), QStringLiteral("Live")},
            {QStringLiteral("scenes"),
             QJsonArray{QJsonObject{{QStringLiteral("sceneName"),
                                     QStringLiteral("Break")}},
                        QJsonObject{{QStringLiteral("sceneName"),
                                     QStringLiteral("Live")}}}}}));
    transport.deliver(responseFrame(
        QStringLiteral("GetInputList"),
        transport.idFor(QStringLiteral("GetInputList")), true,
        QJsonObject{{QStringLiteral("inputs"),
                     QJsonArray{QJsonObject{{QStringLiteral("inputName"),
                                             QStringLiteral("Mic")}}}}}));
    const qsizetype before = transport.sent.size();

    // OBS driven directly by the user stays reflected here.
    transport.deliver(eventFrame(QStringLiteral("RecordStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), true}}));
    QVERIFY(client.snapshot().record.active);
    transport.deliver(eventFrame(QStringLiteral("VirtualcamStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), true}}));
    QVERIFY(client.snapshot().virtualCam.active);
    transport.deliver(
        eventFrame(QStringLiteral("CurrentProgramSceneChanged"),
                   QJsonObject{{QStringLiteral("sceneName"), QStringLiteral("Break")}}));
    QCOMPARE(client.snapshot().scenes.currentProgramScene,
             QStringLiteral("Break"));
    transport.deliver(
        eventFrame(QStringLiteral("InputMuteStateChanged"),
                   QJsonObject{{QStringLiteral("inputName"), QStringLiteral("Mic")},
                               {QStringLiteral("inputMuted"), true}}));
    QCOMPARE(client.snapshot().audioInputs.size(), 1);
    QVERIFY(client.snapshot().audioInputs.at(0).muted);
    // None of those needed a request.
    QCOMPARE(transport.sent.size(), before);

    // A scene list change carries only a delta, so the client re-reads once.
    transport.deliver(eventFrame(QStringLiteral("SceneListChanged"), QJsonObject{}));
    QCOMPARE(transport.sent.size(), before + 1);
    QCOMPARE(transport.sentRequestTypes().count(QStringLiteral("GetSceneList")), 2);
}

void ObsClientTest::aReplyToAnUnknownRequestIsIgnored() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);
    QSignalSpy operations(&client, &ObsClient::operationFinished);

    // A late frame from a previous socket, or a server fault. Resolving it
    // would answer a request this connection never made.
    transport.deliver(responseFrame(QStringLiteral("StartRecord"),
                                    QStringLiteral("someone-elses-id"), true));
    QCOMPARE(operations.size(), 0);
    QVERIFY(!client.snapshot().record.active);
}

void ObsClientTest::operationsReportSentOrNotSent() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());

    // Nothing is sent before the client is Ready, and the caller can tell.
    QCOMPARE(client.setOutputActive(OutputKind::Record, true), 0u);
    QCOMPARE(client.setCurrentProgramScene(QStringLiteral("Live")), 0u);
    QCOMPARE(client.refreshConsoleMapping(), 0u);
    QVERIFY(transport.sent.isEmpty());

    reachReady(transport, client);
    QSignalSpy operations(&client, &ObsClient::operationFinished);

    const quint64 record = client.setOutputActive(OutputKind::Record, true);
    QVERIFY(record != 0);
    QCOMPARE(transport.sentRequestTypes().last(), QStringLiteral("StartRecord"));
    const quint64 stream = client.setOutputActive(OutputKind::Stream, false);
    QCOMPARE(transport.sentRequestTypes().last(), QStringLiteral("StopStream"));
    QVERIFY(stream != record);
    client.setOutputActive(OutputKind::VirtualCam, true);
    QCOMPARE(transport.sentRequestTypes().last(),
             QStringLiteral("StartVirtualCam"));

    // An empty scene or input name names nothing OBS could act on.
    QCOMPARE(client.setCurrentProgramScene(QString()), 0u);
    QCOMPARE(client.setInputMuted(QString(), true), 0u);

    transport.deliver(responseFrame(QStringLiteral("StartRecord"),
                                    transport.idFor(QStringLiteral("StartRecord")),
                                    false, {}, 500));
    QCOMPARE(operations.size(), 1);
    const auto result =
        operations.at(0).at(0).value<ObsClient::OperationResult>();
    QCOMPARE(result.requestId, record);
    QCOMPARE(result.requestType, QStringLiteral("StartRecord"));
    QVERIFY(!result.ok);
    QCOMPARE(result.reasonCode, QStringLiteral("obs-request-failed-500"));
}

void ObsClientTest::aLostConnectionClearsTheLiveStateAndBacksOff() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);
    transport.deliver(eventFrame(QStringLiteral("RecordStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), true}}));
    QVERIFY(client.snapshot().record.active);

    const quint64 pending = client.setOutputActive(OutputKind::Stream, true);
    QVERIFY(pending != 0);
    QSignalSpy operations(&client, &ObsClient::operationFinished);

    transport.dropNow();
    // AGENT-GUARD: the top bar must not keep showing a recording that
    // stopped when OBS quit.
    QVERIFY(!client.snapshot().record.active);
    QCOMPARE(client.state(), ConnectionState::Connecting);
    // The in-flight toggle is answered, not left hanging.
    QCOMPARE(operations.size(), 1);
    QCOMPARE(operations.at(0).at(0).value<ObsClient::OperationResult>().reasonCode,
             QStringLiteral("obs-connection-lost"));

    // And it retries, with the first backoff delay.
    const qsizetype openedBefore = transport.opened.size();
    QTRY_COMPARE_WITH_TIMEOUT(transport.opened.size(), openedBefore + 1, 2000);

    // stop() ends the attempts and says OBS is not there.
    client.stop();
    QCOMPARE(client.state(), ConnectionState::Disconnected);
    const qsizetype openedAfterStop = transport.opened.size();
    QTest::qWait(80);
    QCOMPARE(transport.opened.size(), openedAfterStop);
}

void ObsClientTest::aTimedOutRequestAnswersTheCaller() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);
    QSignalSpy operations(&client, &ObsClient::operationFinished);

    const quint64 id = client.setOutputActive(OutputKind::Record, true);
    QVERIFY(id != 0);
    // OBS never answers. A surface that waited forever would leave its
    // toggle spinning.
    QTRY_COMPARE_WITH_TIMEOUT(operations.size(), 1, 2000);
    const auto result =
        operations.at(0).at(0).value<ObsClient::OperationResult>();
    QCOMPARE(result.requestId, id);
    QCOMPARE(result.reasonCode, QStringLiteral("obs-timeout"));
}

void ObsClientTest::theConsoleMappingComesFromTheVendorNotFromSourceNames() {
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);

    // No bridge: OBS answers the vendor call with a failure, and the mapping
    // stays absent so the route can say the plugin is missing.
    transport.deliver(responseFrame(
        QStringLiteral("CallVendorRequest"),
        transport.idFor(QStringLiteral("CallVendorRequest")), false, {}, 604));
    QVERIFY(!client.snapshot().consoleMapping.present());

    // With the bridge loaded, the vendor event carries the whole mapping.
    transport.deliver(eventFrame(
        QStringLiteral("VendorEvent"),
        QJsonObject{
            {QStringLiteral("vendorName"), QStringLiteral("qindaqt")},
            {QStringLiteral("eventType"), QStringLiteral("ConsoleMappingChanged")},
            {QStringLiteral("eventData"),
             QJsonObject{
                 {QStringLiteral("bridgeVersion"), 1},
                 {QStringLiteral("audioState"), QStringLiteral("ready")},
                 {QStringLiteral("buses"),
                  QJsonArray{QJsonObject{
                      {QStringLiteral("consoleId"), QStringLiteral("bus.a1")},
                      {QStringLiteral("code"), QStringLiteral("A1")},
                      {QStringLiteral("sourceName"),
                       QStringLiteral("QindaQt Bus A1")}}}}}}}));
    QVERIFY(client.snapshot().consoleMapping.present());
    QCOMPARE(client.snapshot().consoleMapping.buses.size(), 1);
    QCOMPARE(client.snapshot().consoleMapping.buses.at(0).consoleId,
             QStringLiteral("bus.a1"));

    // Another plugin's vendor event must not overwrite it.
    transport.deliver(eventFrame(
        QStringLiteral("VendorEvent"),
        QJsonObject{
            {QStringLiteral("vendorName"), QStringLiteral("other-plugin")},
            {QStringLiteral("eventType"), QStringLiteral("ConsoleMappingChanged")},
            {QStringLiteral("eventData"),
             QJsonObject{{QStringLiteral("bridgeVersion"), 9}}}}));
    QCOMPARE(client.snapshot().consoleMapping.bridgeVersion, 1);
    QCOMPARE(client.snapshot().consoleMapping.buses.size(), 1);
}

void ObsClientTest::statisticsArePolledOnlyWhileAnOutputIsRunning() {
    // OBS publishes events for an output starting and stopping but none for
    // elapsed time or dropped frames, so those numbers exist only in
    // GetRecordStatus / GetStreamStatus. The client re-reads them while an
    // output is active — and only then.
    FakeTransport transport;
    ObsClient client(transport, fastTiming());
    reachReady(transport, client);
    const qsizetype afterInitialReads =
        transport.sentRequestTypes().count(QStringLiteral("GetRecordStatus"));
    QCOMPARE(afterInitialReads, 1);

    // AGENT-GUARD: idle OBS must not be polled. A timer running here would
    // wake the shell forever for numbers nobody is looking at.
    QTest::qWait(120);
    QCOMPARE(transport.sentRequestTypes().count(QStringLiteral("GetRecordStatus")),
             afterInitialReads);

    transport.deliver(eventFrame(QStringLiteral("RecordStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), true}}));
    QTRY_VERIFY_WITH_TIMEOUT(
        transport.sentRequestTypes().count(QStringLiteral("GetRecordStatus")) >
            afterInitialReads,
        2000);
    // The stream is not running, so its status is not read.
    QCOMPARE(transport.sentRequestTypes().count(QStringLiteral("GetStreamStatus")), 1);

    // And it stops again when the recording does.
    transport.deliver(eventFrame(QStringLiteral("RecordStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), false}}));
    QTest::qWait(60);
    const qsizetype settled =
        transport.sentRequestTypes().count(QStringLiteral("GetRecordStatus"));
    QTest::qWait(120);
    QCOMPARE(transport.sentRequestTypes().count(QStringLiteral("GetRecordStatus")),
             settled);
}

QTEST_MAIN(ObsClientTest)
#include "tst_obs_client.moc"
