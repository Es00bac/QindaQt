// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using namespace QindaQt::Apps::SettingsStreaming;
using namespace QindaQt::Obs;

namespace {

class FakeTransport final : public ObsTransport {
    Q_OBJECT
public:
    QStringList sent;
    bool socketOpen = false;
    QStringList opened;

    void open(const QString &url) override { opened.append(url); }
    void close() override {
        const bool was = socketOpen;
        socketOpen = false;
        if (was) {
            Q_EMIT disconnected(QStringLiteral("closed"));
        }
    }
    void sendText(const QString &text) override { sent.append(text); }
    [[nodiscard]] bool isOpen() const override { return socketOpen; }

    void connectNow() {
        socketOpen = true;
        Q_EMIT connected();
    }
    void deliver(const QString &text) { Q_EMIT textReceived(text); }
    [[nodiscard]] QString idFor(const QString &requestType) const {
        for (auto it = sent.crbegin(); it != sent.crend(); ++it) {
            const QJsonObject d = QJsonDocument::fromJson(it->toUtf8())
                                      .object()
                                      .value(QStringLiteral("d"))
                                      .toObject();
            if (d.value(QStringLiteral("requestType")).toString() == requestType) {
                return d.value(QStringLiteral("requestId")).toString();
            }
        }
        return {};
    }
    [[nodiscard]] QStringList requestTypes() const {
        QStringList types;
        for (const QString &frame : sent) {
            const QJsonObject d = QJsonDocument::fromJson(frame.toUtf8())
                                      .object()
                                      .value(QStringLiteral("d"))
                                      .toObject();
            const QString type = d.value(QStringLiteral("requestType")).toString();
            if (!type.isEmpty()) {
                types.append(type);
            }
        }
        return types;
    }
};

class FakeSecrets final : public ObsSecretStore {
    Q_OBJECT
public:
    std::optional<QString> stored;
    QString readError;
    QString writeError;
    int writes = 0;

    [[nodiscard]] std::optional<QString> password(QString *error) const override {
        if (error != nullptr) {
            *error = readError;
        }
        return readError.isEmpty() ? stored : std::nullopt;
    }
    [[nodiscard]] bool setPassword(const QString &password,
                                   QString *error) override {
        if (error != nullptr) {
            *error = writeError;
        }
        if (!writeError.isEmpty()) {
            return false;
        }
        ++writes;
        stored = password;
        return true;
    }
};

class FakePreferences final : public StreamingPreferences {
    Q_OBJECT
public:
    int port = 4455;
    bool autoConnectValue = false;
    bool startAtLogin = false;
    bool refuseWrites = false;

    [[nodiscard]] bool isLoaded() const override { return true; }
    [[nodiscard]] int webSocketPort() const override { return port; }
    [[nodiscard]] bool autoConnect() const override { return autoConnectValue; }
    [[nodiscard]] bool startObsAtLogin() const override { return startAtLogin; }
    bool setWebSocketPort(int value) override {
        if (refuseWrites) return false;
        port = value;
        Q_EMIT preferencesChanged();
        return true;
    }
    bool setAutoConnect(bool enabled) override {
        if (refuseWrites) return false;
        autoConnectValue = enabled;
        Q_EMIT preferencesChanged();
        return true;
    }
    bool setStartObsAtLogin(bool enabled) override {
        if (refuseWrites) return false;
        startAtLogin = enabled;
        Q_EMIT preferencesChanged();
        return true;
    }
};

QString helloFrame() {
    return QStringLiteral(
        R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1}})");
}
QString identifiedFrame() {
    return QStringLiteral(R"({"op":2,"d":{"negotiatedRpcVersion":1}})");
}
QString eventFrame(const QString &type, const QJsonObject &data) {
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{
                          {QStringLiteral("op"), 5},
                          {QStringLiteral("d"),
                           QJsonObject{{QStringLiteral("eventType"), type},
                                       {QStringLiteral("eventData"), data}}}})
            .toJson(QJsonDocument::Compact));
}
QString responseFrame(const QString &type, const QString &id, bool ok,
                      const QJsonObject &data = {}, int code = 100,
                      const QString &comment = {}) {
    QJsonObject d{{QStringLiteral("requestType"), type},
                  {QStringLiteral("requestId"), id},
                  {QStringLiteral("requestStatus"),
                   QJsonObject{{QStringLiteral("result"), ok},
                               {QStringLiteral("code"), code},
                               {QStringLiteral("comment"), comment}}}};
    if (!data.isEmpty()) {
        d.insert(QStringLiteral("responseData"), data);
    }
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("op"), 7},
                                  {QStringLiteral("d"), d}})
            .toJson(QJsonDocument::Compact));
}

} // namespace

class StreamingSettingsModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void theAddressIsAlwaysLoopback();
    void connectingWithoutAStoredPasswordSaysSoInsteadOfFailing();
    void anUnreadableKeyringIsNotTreatedAsNoPassword();
    void settingUpGeneratesStoresAndWritesOnce();
    void elapsedTimeIsADashUntilObsReportsOne();
    void aDroppedFrameWarningIsOnlyShownWhenTheNumbersSupportIt();
    void aMissingBridgeAndAnEmptyConsoleAreDifferentSentences();
    void togglesWhileDisconnectedSayTheyDidNothing();
    void aRefusedRequestIsReportedWithObsOwnWords();
    void theBusMappingTableComesFromTheVendorPayload();

private:
    void reachReady(FakeTransport &transport) {
        transport.connectNow();
        transport.deliver(helloFrame());
        transport.deliver(identifiedFrame());
    }
};

void StreamingSettingsModelTest::theAddressIsAlwaysLoopback() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    // AGENT-GUARD: obs-websocket has no transport security; the route offers
    // a port, never a host.
    QCOMPARE(model.address(), QStringLiteral("ws://127.0.0.1:4455"));
    model.setWebSocketPort(4466);
    QCOMPARE(model.address(), QStringLiteral("ws://127.0.0.1:4466"));
    // An impossible port is refused rather than saved.
    model.setWebSocketPort(0);
    model.setWebSocketPort(70000);
    QCOMPARE(model.webSocketPort(), 4466);
}

void StreamingSettingsModelTest::connectingWithoutAStoredPasswordSaysSoInsteadOfFailing() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets; // nothing stored
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    model.connectToObs();
    QVERIFY(transport.opened.isEmpty());
    QVERIFY(model.statusText().contains(QStringLiteral("Set up OBS")));
    QVERIFY(!model.passwordStored());
}

void StreamingSettingsModelTest::anUnreadableKeyringIsNotTreatedAsNoPassword() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    secrets.readError = QStringLiteral("the keyring is locked");
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    model.connectToObs();
    QVERIFY(transport.opened.isEmpty());
    // AGENT-GUARD: connecting with an empty password would make OBS's
    // refusal look like a wrong password the user chose.
    QVERIFY(model.statusText().contains(QStringLiteral("keyring")));

    // And setting up must not overwrite a password it could not read.
    model.installDefaults();
    QCOMPARE(secrets.writes, 0);
    QVERIFY(model.statusText().contains(QStringLiteral("keyring")));
}

void StreamingSettingsModelTest::settingUpGeneratesStoresAndWritesOnce() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    QVERIFY(root.isValid());
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    model.refresh();
    QVERIFY(!model.defaultsInstalled());
    QCOMPARE(model.defaultsProblems().size(), 3);

    model.installDefaults();
    QCOMPARE(secrets.writes, 1);
    QVERIFY(secrets.stored.has_value());
    QCOMPARE(secrets.stored->size(), 32);
    QVERIFY(model.defaultsInstalled());
    QVERIFY(model.passwordStored());
    // OBS reads its configuration once, at start; the route says so.
    QVERIFY(model.statusText().contains(QStringLiteral("Restart OBS")));

    // Running it again keeps the same password rather than locking the user
    // out of an OBS that already has the old one.
    const QString first = *secrets.stored;
    model.installDefaults();
    QCOMPARE(secrets.writes, 1);
    QCOMPARE(*secrets.stored, first);
}

void StreamingSettingsModelTest::elapsedTimeIsADashUntilObsReportsOne() {
    // A zero clock reads as "it just started", which is a different claim
    // from "OBS did not tell us".
    QCOMPARE(StreamingSettingsModel::formatElapsed(-1), QStringLiteral("—"));
    QCOMPARE(StreamingSettingsModel::formatElapsed(0), QStringLiteral("00:00:00"));
    QCOMPARE(StreamingSettingsModel::formatElapsed(3661000),
             QStringLiteral("01:01:01"));

    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());
    QCOMPARE(model.recordingElapsed(), QStringLiteral("—"));

    reachReady(transport);
    transport.deliver(responseFrame(
        QStringLiteral("GetRecordStatus"),
        transport.idFor(QStringLiteral("GetRecordStatus")), true,
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputDuration"), 65000}}));
    QVERIFY(model.recording());
    QCOMPARE(model.recordingElapsed(), QStringLiteral("00:01:05"));
}

void StreamingSettingsModelTest::aDroppedFrameWarningIsOnlyShownWhenTheNumbersSupportIt() {
    FakeTransport transport;
    ClientTiming timing;
    timing.statisticsIntervalMilliseconds = 30;
    ObsClient client(transport, timing);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());
    reachReady(transport);

    // Not streaming: no warning, whatever the counters say.
    QVERIFY(model.droppedFramesWarning().isEmpty());

    transport.deliver(responseFrame(
        QStringLiteral("GetStreamStatus"),
        transport.idFor(QStringLiteral("GetStreamStatus")), true,
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputSkippedFrames"), 2},
                    {QStringLiteral("outputTotalFrames"), 1000}}));
    // 0.2% is noise the user cannot act on.
    QVERIFY(model.droppedFramesWarning().isEmpty());

    // OBS publishes no event for dropped frames, so the client re-reads the
    // status while the stream is active; that read is what moves the number.
    QTRY_VERIFY_WITH_TIMEOUT(
        transport.requestTypes().count(QStringLiteral("GetStreamStatus")) >= 2,
        3000);
    transport.deliver(responseFrame(
        QStringLiteral("GetStreamStatus"),
        transport.idFor(QStringLiteral("GetStreamStatus")), true,
        QJsonObject{{QStringLiteral("outputActive"), true},
                    {QStringLiteral("outputSkippedFrames"), 50},
                    {QStringLiteral("outputTotalFrames"), 1000}}));
    const QString warning = model.droppedFramesWarning();
    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains(QStringLiteral("5.0")));
    QVERIFY(warning.contains(QStringLiteral("50")));
    QVERIFY(warning.contains(QStringLiteral("1000")));
}

void StreamingSettingsModelTest::aMissingBridgeAndAnEmptyConsoleAreDifferentSentences() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    // Not connected at all.
    QVERIFY(model.bridgeProblem().contains(QStringLiteral("Connect to OBS")));

    reachReady(transport);
    // Connected, bridge not loaded: OBS refuses the vendor call.
    transport.deliver(responseFrame(
        QStringLiteral("CallVendorRequest"),
        transport.idFor(QStringLiteral("CallVendorRequest")), false, {}, 604));
    QVERIFY(!model.bridgePresent());
    QVERIFY(model.bridgeProblem().contains(QStringLiteral("bridge plugin is not loaded")));

    // Bridge loaded, console empty: a different sentence and a different fix.
    transport.deliver(eventFrame(
        QStringLiteral("VendorEvent"),
        QJsonObject{
            {QStringLiteral("vendorName"), QStringLiteral("qindaqt")},
            {QStringLiteral("eventType"), QStringLiteral("ConsoleMappingChanged")},
            {QStringLiteral("eventData"),
             QJsonObject{{QStringLiteral("bridgeVersion"), 1},
                         {QStringLiteral("audioState"), QStringLiteral("ready")}}}}));
    QVERIFY(model.bridgePresent());
    QVERIFY(model.bridgeProblem().contains(QStringLiteral("no buses or")));

    // Bridge loaded, audio service down: blame the console, not OBS.
    transport.deliver(eventFrame(
        QStringLiteral("VendorEvent"),
        QJsonObject{
            {QStringLiteral("vendorName"), QStringLiteral("qindaqt")},
            {QStringLiteral("eventType"), QStringLiteral("ConsoleMappingChanged")},
            {QStringLiteral("eventData"),
             QJsonObject{
                 {QStringLiteral("bridgeVersion"), 1},
                 {QStringLiteral("audioState"), QStringLiteral("unavailable")},
                 {QStringLiteral("buses"),
                  QJsonArray{QJsonObject{
                      {QStringLiteral("consoleId"), QStringLiteral("bus.a1")}}}}}}}));
    QVERIFY(model.bridgeProblem().contains(QStringLiteral("audio console")));
}

void StreamingSettingsModelTest::togglesWhileDisconnectedSayTheyDidNothing() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());

    model.setRecording(true);
    QCOMPARE(model.statusText(), QStringLiteral("Not connected to OBS."));
    model.setStreaming(true);
    QCOMPARE(model.statusText(), QStringLiteral("Not connected to OBS."));
    model.setVirtualCamera(true);
    QCOMPARE(model.statusText(), QStringLiteral("Not connected to OBS."));
    model.selectScene(QStringLiteral("Live"));
    QCOMPARE(model.statusText(), QStringLiteral("Not connected to OBS."));
    model.refreshBusMapping();
    QCOMPARE(model.statusText(), QStringLiteral("Not connected to OBS."));
    QVERIFY(transport.sent.isEmpty());
}

void StreamingSettingsModelTest::aRefusedRequestIsReportedWithObsOwnWords() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());
    reachReady(transport);

    model.setVirtualCamera(true);
    transport.deliver(responseFrame(
        QStringLiteral("StartVirtualCam"),
        transport.idFor(QStringLiteral("StartVirtualCam")), false, {}, 500,
        QStringLiteral("Failed to start the virtual camera")));
    QVERIFY(model.statusText().contains(QStringLiteral("virtual camera")));
    QVERIFY(model.statusText().contains(QStringLiteral("StartVirtualCam")));

    // A success clears the notice rather than leaving a stale complaint.
    model.setRecording(true);
    transport.deliver(responseFrame(QStringLiteral("StartRecord"),
                                    transport.idFor(QStringLiteral("StartRecord")),
                                    true));
    QVERIFY(model.statusText().isEmpty());
}

void StreamingSettingsModelTest::theBusMappingTableComesFromTheVendorPayload() {
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    QTemporaryDir root;
    StreamingSettingsModel model(client, secrets, preferences, root.path());
    reachReady(transport);

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
                      {QStringLiteral("label"), QStringLiteral("Speakers")},
                      {QStringLiteral("sourceName"), QStringLiteral("QindaQt Bus A1")},
                      {QStringLiteral("sourceKind"),
                       QStringLiteral("qindaqt_console_bus")},
                      {QStringLiteral("captureDevice"),
                       QStringLiteral("alsa_output.x.monitor")}}}},
                 {QStringLiteral("strips"),
                  QJsonArray{QJsonObject{
                      {QStringLiteral("consoleId"), QStringLiteral("strip.virtual.1")},
                      {QStringLiteral("code"), QStringLiteral("Virtual 1")},
                      {QStringLiteral("sourceKind"),
                       QStringLiteral("qindaqt_console_strip")}}}}}}}));

    const QVariantList rows = model.busMapping();
    QCOMPARE(rows.size(), 2);
    const QVariantMap bus = rows.at(0).toMap();
    QCOMPARE(bus.value(QStringLiteral("code")).toString(), QStringLiteral("A1"));
    QCOMPARE(bus.value(QStringLiteral("sourceName")).toString(),
             QStringLiteral("QindaQt Bus A1"));
    // The route names the kind in the user's words, from the bridge's own
    // source-type id — never by parsing the source name.
    QCOMPARE(bus.value(QStringLiteral("kind")).toString(), QStringLiteral("Bus"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("Strip"));
    QVERIFY(model.bridgeProblem().isEmpty());
}

QTEST_MAIN(StreamingSettingsModelTest)
#include "tst_streaming_settings_model.moc"
