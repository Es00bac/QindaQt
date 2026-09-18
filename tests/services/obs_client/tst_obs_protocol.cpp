// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_protocol.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace QindaQt::Obs;
using namespace QindaQt::Obs::Protocol;

namespace {

QJsonObject payloadOf(const QString &frame) {
    return QJsonDocument::fromJson(frame.toUtf8())
        .object()
        .value(QStringLiteral("d"))
        .toObject();
}

int opOf(const QString &frame) {
    return QJsonDocument::fromJson(frame.toUtf8())
        .object()
        .value(QStringLiteral("op"))
        .toInt(-1);
}

} // namespace

class ObsProtocolTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void decodesTheFourFramesThisClientSpeaks();
    void refusesFramesItCannotInterpret();
    void authenticationMatchesTheObsWebSocketDigest();
    void identifyCarriesTheVersionAndTheSubscriptions();
    void vendorRequestsAreWrappedInCallVendorRequest();
    void aVendorPayloadFromAnotherPluginIsNotOurs();
    void outputStatusKeepsUnreportedFieldsUnknown();
    void sceneListIsPresentedInTheOrderObsShows();
    void aConsoleMappingWithoutABridgeVersionIsAbsent();
    void theConsoleMappingCarriesF3sFields();
};

void ObsProtocolTest::decodesTheFourFramesThisClientSpeaks() {
    const auto hello = decodeFrame(QStringLiteral(
        R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1,)"
        R"("authentication":{"challenge":"c","salt":"s"}}})"));
    QVERIFY(hello.has_value());
    QCOMPARE(hello->op, OpCode::Hello);
    QCOMPARE(hello->hello.rpcVersion, 1);
    QVERIFY(hello->hello.requiresAuthentication());
    QCOMPARE(hello->hello.obsWebSocketVersion, QStringLiteral("5.6.2"));

    // An OBS with server authentication off sends no authentication block.
    const auto open = decodeFrame(QStringLiteral(
        R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1}})"));
    QVERIFY(open.has_value());
    QVERIFY(!open->hello.requiresAuthentication());

    const auto identified = decodeFrame(
        QStringLiteral(R"({"op":2,"d":{"negotiatedRpcVersion":1}})"));
    QVERIFY(identified.has_value());
    QCOMPARE(identified->op, OpCode::Identified);
    QCOMPARE(identified->negotiatedRpcVersion, 1);

    const auto event = decodeFrame(QStringLiteral(
        R"({"op":5,"d":{"eventType":"RecordStateChanged","eventIntent":64,)"
        R"("eventData":{"outputActive":true}}})"));
    QVERIFY(event.has_value());
    QCOMPARE(event->op, OpCode::Event);
    QCOMPARE(event->event.eventType, QStringLiteral("RecordStateChanged"));
    QVERIFY(event->event.data.value(QStringLiteral("outputActive")).toBool());

    const auto response = decodeFrame(QStringLiteral(
        R"({"op":7,"d":{"requestType":"StartRecord","requestId":"q-1",)"
        R"("requestStatus":{"result":false,"code":500,"comment":"busy"}}})"));
    QVERIFY(response.has_value());
    QCOMPARE(response->op, OpCode::RequestResponse);
    QCOMPARE(response->response.requestId, QStringLiteral("q-1"));
    QVERIFY(!response->response.ok);
    QCOMPARE(response->response.code, 500);
    QCOMPARE(response->response.comment, QStringLiteral("busy"));
}

void ObsProtocolTest::refusesFramesItCannotInterpret() {
    // Not JSON, not an object, no op, no payload.
    QVERIFY(!decodeFrame(QStringLiteral("not json")).has_value());
    QVERIFY(!decodeFrame(QStringLiteral("[1,2]")).has_value());
    QVERIFY(!decodeFrame(QStringLiteral(R"({"d":{}})")).has_value());
    QVERIFY(!decodeFrame(QStringLiteral(R"({"op":0})")).has_value());
    QVERIFY(!decodeFrame(QString()).has_value());
    // An opcode this client does not speak (RequestBatchResponse).
    QVERIFY(!decodeFrame(QStringLiteral(R"({"op":9,"d":{}})")).has_value());
    // Hello without a version, and with half an authentication block.
    QVERIFY(!decodeFrame(QStringLiteral(R"({"op":0,"d":{}})")).has_value());
    QVERIFY(!decodeFrame(QStringLiteral(
                             R"({"op":0,"d":{"rpcVersion":1,)"
                             R"("authentication":{"challenge":"c"}}})"))
                 .has_value());
    // A response with no id could resolve someone else's request.
    QVERIFY(!decodeFrame(QStringLiteral(
                             R"({"op":7,"d":{"requestType":"X",)"
                             R"("requestStatus":{"result":true}}})"))
                 .has_value());
    // A response with no result is not a result.
    QVERIFY(!decodeFrame(QStringLiteral(
                             R"({"op":7,"d":{"requestId":"q-1",)"
                             R"("requestStatus":{"code":100}}})"))
                 .has_value());
    // An event with no type.
    QVERIFY(!decodeFrame(QStringLiteral(R"({"op":5,"d":{"eventData":{}}})"))
                 .has_value());
}

void ObsProtocolTest::authenticationMatchesTheObsWebSocketDigest() {
    // base64(sha256(base64(sha256(password + salt)) + challenge)), computed
    // independently from obs-websocket's documented algorithm.
    QCOMPARE(authenticationString(QStringLiteral("supersecretpassword"),
                                  QStringLiteral("PZVbYpvAnZut2SS6JNJytDm9"),
                                  QStringLiteral("ztTBnnuqrqaKDzRM3xcVdbYm")),
             QStringLiteral("zZgWipvwSGrw748kHN4gNpBC1IaeiiWX3Hjkrm849Sc="));
    // No password means an unauthenticated identify, not a digest of "".
    QVERIFY(authenticationString(QString(), QStringLiteral("s"),
                                 QStringLiteral("c"))
                .isEmpty());
}

void ObsProtocolTest::identifyCarriesTheVersionAndTheSubscriptions() {
    Hello hello;
    hello.rpcVersion = 1;
    hello.salt = QStringLiteral("PZVbYpvAnZut2SS6JNJytDm9");
    hello.challenge = QStringLiteral("ztTBnnuqrqaKDzRM3xcVdbYm");
    const QString frame =
        encodeIdentify(hello, QStringLiteral("supersecretpassword"));
    QCOMPARE(opOf(frame), int(OpCode::Identify));
    const QJsonObject d = payloadOf(frame);
    QCOMPARE(d.value(QStringLiteral("rpcVersion")).toInt(), RpcVersion);
    QCOMPARE(d.value(QStringLiteral("authentication")).toString(),
             QStringLiteral("zZgWipvwSGrw748kHN4gNpBC1IaeiiWX3Hjkrm849Sc="));
    // The desktop subscribes to what it renders, not to everything.
    const int subscriptions =
        d.value(QStringLiteral("eventSubscriptions")).toInt();
    QCOMPARE(subscriptions, EventSubscription::Desktop);
    QVERIFY((subscriptions & EventSubscription::Outputs) != 0);
    QVERIFY((subscriptions & EventSubscription::Vendors) != 0);

    // An OBS that asked for no authentication gets an identify with no
    // authentication field at all.
    Hello open;
    open.rpcVersion = 1;
    const QJsonObject openPayload =
        payloadOf(encodeIdentify(open, QStringLiteral("unused")));
    QVERIFY(!openPayload.contains(QStringLiteral("authentication")));

    // A server that asked and got no password still receives an identify, so
    // its own refusal is what the user is told about.
    const QJsonObject refused =
        payloadOf(encodeIdentify(hello, QString()));
    QVERIFY(refused.contains(QStringLiteral("authentication")));
    QVERIFY(refused.value(QStringLiteral("authentication")).toString().isEmpty());
}

void ObsProtocolTest::vendorRequestsAreWrappedInCallVendorRequest() {
    const QString frame = encodeVendorRequest(QStringLiteral("qindaqt"),
                                              QStringLiteral("GetConsoleMapping"),
                                              QStringLiteral("q-7"));
    QCOMPARE(opOf(frame), int(OpCode::Request));
    const QJsonObject d = payloadOf(frame);
    QCOMPARE(d.value(QStringLiteral("requestType")).toString(),
             QStringLiteral("CallVendorRequest"));
    QCOMPARE(d.value(QStringLiteral("requestId")).toString(),
             QStringLiteral("q-7"));
    const QJsonObject inner =
        d.value(QStringLiteral("requestData")).toObject();
    QCOMPARE(inner.value(QStringLiteral("vendorName")).toString(),
             QStringLiteral("qindaqt"));
    QCOMPARE(inner.value(QStringLiteral("requestType")).toString(),
             QStringLiteral("GetConsoleMapping"));

    // A plain request keeps its own name and omits empty request data.
    const QJsonObject plain = payloadOf(encodeRequest(
        QStringLiteral("StartRecord"), QStringLiteral("q-8")));
    QCOMPARE(plain.value(QStringLiteral("requestType")).toString(),
             QStringLiteral("StartRecord"));
    QVERIFY(!plain.contains(QStringLiteral("requestData")));
}

void ObsProtocolTest::aVendorPayloadFromAnotherPluginIsNotOurs() {
    const QJsonObject ours{
        {QStringLiteral("vendorName"), QStringLiteral("qindaqt")},
        {QStringLiteral("requestType"), QStringLiteral("GetConsoleMapping")},
        {QStringLiteral("responseData"),
         QJsonObject{{QStringLiteral("bridgeVersion"), 1}}},
    };
    const auto unwrapped = unwrapVendorPayload(
        ours, QStringLiteral("qindaqt"), QStringLiteral("GetConsoleMapping"));
    QVERIFY(unwrapped.has_value());
    QCOMPARE(unwrapped->value(QStringLiteral("bridgeVersion")).toInt(), 1);

    // Another plugin's vendor traffic reaches this client too; reading it as
    // the console mapping would put a stranger's data in the route.
    QJsonObject theirs = ours;
    theirs.insert(QStringLiteral("vendorName"), QStringLiteral("other-plugin"));
    QVERIFY(!unwrapVendorPayload(theirs, QStringLiteral("qindaqt"),
                                 QStringLiteral("GetConsoleMapping"))
                 .has_value());

    // Our vendor, a different request.
    QJsonObject otherType = ours;
    otherType.insert(QStringLiteral("requestType"), QStringLiteral("SomethingElse"));
    QVERIFY(!unwrapVendorPayload(otherType, QStringLiteral("qindaqt"),
                                 QStringLiteral("GetConsoleMapping"))
                 .has_value());

    // A VendorEvent envelope names its type and payload differently.
    const QJsonObject event{
        {QStringLiteral("vendorName"), QStringLiteral("qindaqt")},
        {QStringLiteral("eventType"), QStringLiteral("ConsoleMappingChanged")},
        {QStringLiteral("eventData"),
         QJsonObject{{QStringLiteral("bridgeVersion"), 1}}},
    };
    QVERIFY(unwrapVendorPayload(event, QStringLiteral("qindaqt"),
                                QStringLiteral("ConsoleMappingChanged"))
                .has_value());
}

void ObsProtocolTest::outputStatusKeepsUnreportedFieldsUnknown() {
    const OutputStatus record = recordStatusFrom(
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputPaused"), false},
                    {QStringLiteral("outputDuration"), 61000}});
    QVERIFY(record.active);
    QCOMPARE(record.durationMs, 61000);
    // OBS reports no frame counts for a recording; the route must show a
    // dash, not a zero that looks measured.
    QVERIFY(!record.hasFrameCounts());
    QCOMPARE(record.droppedFraction(), -1.0);

    const OutputStatus stream = streamStatusFrom(
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputReconnecting"), true},
                    {QStringLiteral("outputSkippedFrames"), 12},
                    {QStringLiteral("outputTotalFrames"), 1200}});
    QVERIFY(stream.reconnecting);
    QVERIFY(stream.hasFrameCounts());
    QVERIFY(qFuzzyCompare(stream.droppedFraction(), 0.01));

    // A field of the wrong type is "not reported", never coerced.
    const OutputStatus odd = streamStatusFrom(
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputTotalFrames"), QStringLiteral("many")}});
    QCOMPARE(odd.totalFrames, -1);
    QVERIFY(!odd.hasFrameCounts());

    QCOMPARE(virtualCamStatusFrom(
                 QJsonObject{{QStringLiteral("outputActive"), true}})
                 .active,
             true);
}

void ObsProtocolTest::sceneListIsPresentedInTheOrderObsShows() {
    // OBS returns scenes bottom-first; the route lists them the way the user
    // sees them in OBS.
    const SceneList list = sceneListFrom(QJsonObject{
        {QStringLiteral("currentProgramSceneName"), QStringLiteral("Live")},
        {QStringLiteral("scenes"),
         QJsonArray{
             QJsonObject{{QStringLiteral("sceneName"), QStringLiteral("Break")}},
             QJsonObject{{QStringLiteral("sceneName"), QStringLiteral("Live")}},
             QJsonObject{{QStringLiteral("sceneName"), QStringLiteral("Intro")}},
         }},
    });
    QCOMPARE(list.names,
             (QStringList{QStringLiteral("Intro"), QStringLiteral("Live"),
                          QStringLiteral("Break")}));
    QCOMPARE(list.currentProgramScene, QStringLiteral("Live"));

    const QList<AudioInput> inputs = audioInputsFrom(QJsonObject{
        {QStringLiteral("inputs"),
         QJsonArray{QJsonObject{
                        {QStringLiteral("inputName"), QStringLiteral("Mic")},
                        {QStringLiteral("inputKind"),
                         QStringLiteral("pulse_input_capture")}},
                    QJsonObject{{QStringLiteral("inputKind"),
                                 QStringLiteral("no_name")}}}},
    });
    QCOMPARE(inputs.size(), 1);
    QCOMPARE(inputs.at(0).name, QStringLiteral("Mic"));
}

void ObsProtocolTest::aConsoleMappingWithoutABridgeVersionIsAbsent() {
    // AGENT-GUARD: absent must not look like "the console has no buses".
    QVERIFY(!consoleMappingFrom(QJsonObject{}).present());
    QVERIFY(!consoleMappingFrom(
                 QJsonObject{{QStringLiteral("buses"), QJsonArray{}}})
                 .present());
    QVERIFY(!consoleMappingFrom(
                 QJsonObject{{QStringLiteral("bridgeVersion"), 0}})
                 .present());
    const ConsoleMapping empty = consoleMappingFrom(
        QJsonObject{{QStringLiteral("bridgeVersion"), 1},
                    {QStringLiteral("buses"), QJsonArray{}},
                    {QStringLiteral("strips"), QJsonArray{}}});
    QVERIFY(empty.present());
    QCOMPARE(empty.size(), 0);
}

void ObsProtocolTest::theConsoleMappingCarriesF3sFields() {
    // Exactly the payload src/obs/module/websocket_vendor.cpp builds at
    // bd8527a0: two arrays of entries plus the bridge's lineage.
    const ConsoleMapping mapping = consoleMappingFrom(QJsonObject{
        {QStringLiteral("bridgeVersion"), 1},
        {QStringLiteral("audioState"), QStringLiteral("ready")},
        {QStringLiteral("reasonCode"), QString()},
        {QStringLiteral("epoch"), 7},
        {QStringLiteral("revision"), 42},
        {QStringLiteral("buses"),
         QJsonArray{QJsonObject{
             {QStringLiteral("consoleId"), QStringLiteral("bus.a1")},
             {QStringLiteral("code"), QStringLiteral("A1")},
             {QStringLiteral("label"), QStringLiteral("Speakers")},
             {QStringLiteral("sourceName"),
              QStringLiteral("QindaQt Bus A1 — Speakers")},
             {QStringLiteral("sourceKind"), QStringLiteral("qindaqt_console_bus")},
             {QStringLiteral("captureKind"), QStringLiteral("monitor")},
             {QStringLiteral("captureDevice"), QStringLiteral("alsa_output.x.monitor")},
             {QStringLiteral("muted"), false},
             {QStringLiteral("gainDb"), -3.5}}}},
        {QStringLiteral("strips"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("consoleId"), QStringLiteral("strip.virtual.1")},
                 {QStringLiteral("code"), QStringLiteral("Virtual 1")},
                 {QStringLiteral("sourceKind"),
                  QStringLiteral("qindaqt_console_strip")}},
             // An entry with no console id names nothing actionable.
             QJsonObject{{QStringLiteral("code"), QStringLiteral("orphan")}}}},
    });
    QVERIFY(mapping.present());
    QCOMPARE(mapping.bridgeVersion, 1);
    QCOMPARE(mapping.audioState, QStringLiteral("ready"));
    QCOMPARE(mapping.epoch, 7u);
    QCOMPARE(mapping.revision, 42u);
    QCOMPARE(mapping.buses.size(), 1);
    QCOMPARE(mapping.buses.at(0).consoleId, QStringLiteral("bus.a1"));
    QCOMPARE(mapping.buses.at(0).code, QStringLiteral("A1"));
    QCOMPARE(mapping.buses.at(0).sourceName,
             QStringLiteral("QindaQt Bus A1 — Speakers"));
    QVERIFY(qFuzzyCompare(mapping.buses.at(0).gainDb, -3.5));
    QCOMPARE(mapping.strips.size(), 1);
    QCOMPARE(mapping.strips.at(0).consoleId, QStringLiteral("strip.virtual.1"));
    QCOMPARE(mapping.size(), 2);
}

QTEST_APPLESS_MAIN(ObsProtocolTest)
#include "tst_obs_protocol.moc"
