// SPDX-License-Identifier: GPL-3.0-or-later
// The OBS record state machine over the desktop's obs-websocket client and
// its transport seam (no OBS, no socket), plus the connection gate.
#include "record_connection.h"
#include "record_controller.h"

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/streaming_preferences/streaming_preferences.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Obs;
using namespace QindaQt::Screenshot;

namespace {

class FakeTransport final : public ObsTransport {
    Q_OBJECT
public:
    QStringList sent;
    QStringList opened;
    bool socketOpen = false;
    int code = 0;

    void open(const QString &url) override { opened.append(url); }
    [[nodiscard]] int closeCode() const override { return code; }
    void close() override { socketOpen = false; }
    void sendText(const QString &text) override { sent.append(text); }
    [[nodiscard]] bool isOpen() const override { return socketOpen; }

    void connectNow()
    {
        socketOpen = true;
        Q_EMIT connected();
    }
    void deliver(const QString &text) { Q_EMIT textReceived(text); }
    void reachReady()
    {
        connectNow();
        deliver(QStringLiteral(R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1}})"));
        deliver(QStringLiteral(R"({"op":2,"d":{"negotiatedRpcVersion":1}})"));
    }
    [[nodiscard]] QString lastRequestType() const { return requestAt(sent.size() - 1).first; }
    [[nodiscard]] QString idFor(const QString &type) const
    {
        for (qsizetype index = sent.size() - 1; index >= 0; --index) {
            const auto request = requestAt(index);
            if (request.first == type)
                return request.second;
        }
        return {};
    }

private:
    [[nodiscard]] QPair<QString, QString> requestAt(qsizetype index) const
    {
        if (index < 0 || index >= sent.size())
            return {};
        const QJsonObject d = QJsonDocument::fromJson(sent.at(index).toUtf8()).object()
                                  .value(QStringLiteral("d")).toObject();
        return {d.value(QStringLiteral("requestType")).toString(),
                d.value(QStringLiteral("requestId")).toString()};
    }
};

QString eventFrame(const QString &type, const QJsonObject &data)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("op"), 5},
                                  {QStringLiteral("d"), QJsonObject{{QStringLiteral("eventType"), type},
                                                                    {QStringLiteral("eventData"), data}}}})
            .toJson(QJsonDocument::Compact));
}

QString recordState(bool active, const QString &state, const QString &path = {})
{
    QJsonObject data{{QStringLiteral("outputActive"), active}, {QStringLiteral("outputState"), state}};
    if (!path.isEmpty())
        data.insert(QStringLiteral("outputPath"), path);
    return eventFrame(QStringLiteral("RecordStateChanged"), data);
}

QString responseFrame(const QString &type, const QString &id, bool ok, const QString &comment = {},
                      const QJsonObject &data = {})
{
    QJsonObject d{{QStringLiteral("requestType"), type},
                  {QStringLiteral("requestId"), id},
                  {QStringLiteral("requestStatus"),
                   QJsonObject{{QStringLiteral("result"), ok},
                               {QStringLiteral("code"), ok ? 100 : 500},
                               {QStringLiteral("comment"), comment}}}};
    if (!data.isEmpty())
        d.insert(QStringLiteral("responseData"), data);
    return QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("op"), 7}, {QStringLiteral("d"), d}})
                                 .toJson(QJsonDocument::Compact));
}

class FakePreferences final : public QindaQt::Services::StreamingPreferences::StreamingPreferences {
public:
    bool loaded = true;
    bool autoConnectValue = true;
    int port = 4455;
    [[nodiscard]] bool isLoaded() const override { return loaded; }
    [[nodiscard]] int webSocketPort() const override { return port; }
    [[nodiscard]] bool autoConnect() const override { return autoConnectValue; }
    [[nodiscard]] bool startObsAtLogin() const override { return false; }
    bool setWebSocketPort(int) override { return false; }
    bool setAutoConnect(bool) override { return false; }
    bool setStartObsAtLogin(bool) override { return false; }
};

class FakeSecrets final : public ObsSecretStore {
public:
    std::optional<QString> stored;
    mutable int reads = 0;
    [[nodiscard]] std::optional<QString> password(QString *) const override
    {
        ++reads;
        return stored;
    }
    bool setPassword(const QString &, QString *) override { return false; }
};

} // namespace

class RecordControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void withoutAClientItSaysWhy();
    void unavailableReasonsAreTheAppletsSentences();
    void startIsPendingUntilObsConfirms();
    void pausedIsStillRecordingAndStopReportsTheFile();
    void stopReplyCarriesThePathWhenTheEventDoesNot();
    void refusedAndUnconfirmedRequestsAreReported();
    void gateOpensOnlyForAConfirmedSetUpObs();
};

void RecordControllerTest::withoutAClientItSaysWhy()
{
    RecordController controller(nullptr);
    QCOMPARE(controller.state(), RecordController::State::Unavailable);
    QVERIFY(!controller.canToggle());
    QVERIFY(!controller.toggle());
    QVERIFY(!controller.feedback().isEmpty());
    QVERIFY(controller.elapsed().isEmpty());
}

void RecordControllerTest::unavailableReasonsAreTheAppletsSentences()
{
    FakeTransport transport;
    ObsClient client(transport);
    RecordController controller(&client);
    QCOMPARE(controller.statusText(), QStringLiteral("OBS is not running."));
    // The gate's sentence wins while the connection is not allowed to open.
    controller.setGateText(RecordConnection::gateText(RecordConnection::Gate::NoPassword));
    QVERIFY(controller.statusText().contains(QStringLiteral("Settings → Streaming")));
    controller.setGateText({});
    client.start(QStringLiteral("ws://127.0.0.1:4455"), QStringLiteral("secret"));
    transport.connectNow();
    transport.deliver(QStringLiteral(
        R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1,"authentication":{"challenge":"c","salt":"s"}}})"));
    // OBS refusing the password (close code 4009) is its own sentence.
    transport.code = 4009;
    Q_EMIT transport.disconnected(QStringLiteral("Authentication failed."));
    QVERIFY(!controller.available());
    QCOMPARE(controller.statusText(),
             QStringLiteral("OBS did not accept QindaQt's password. Open Settings → Streaming and set OBS up again."));
}

void RecordControllerTest::startIsPendingUntilObsConfirms()
{
    FakeTransport transport;
    ObsClient client(transport);
    RecordController controller(&client);
    transport.reachReady();
    QCOMPARE(controller.state(), RecordController::State::Idle);
    QVERIFY(controller.canToggle());
    QSignalSpy finished(&controller, &RecordController::toggleFinished);

    QVERIFY(controller.toggle());
    QCOMPARE(transport.lastRequestType(), QStringLiteral("StartRecord"));
    QCOMPARE(controller.state(), RecordController::State::Starting);
    QVERIFY(!controller.canToggle());
    QVERIFY(!controller.toggle()); // one request at a time
    QCOMPARE(controller.feedback(), QStringLiteral("Waiting for OBS…"));
    // The reply alone does not confirm; OBS's state event does.
    transport.deliver(responseFrame(QStringLiteral("StartRecord"), transport.idFor(QStringLiteral("StartRecord")), true));
    QCOMPARE(controller.state(), RecordController::State::Starting);
    transport.deliver(recordState(true, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STARTED")));
    QCOMPARE(controller.state(), RecordController::State::Recording);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(0).toBool(), true);
    QCOMPARE(finished.first().at(1).toBool(), true);
    QVERIFY(controller.feedback().isEmpty());
    QCOMPARE(controller.elapsed(), QStringLiteral("—")); // unknown until OBS reports it
}

void RecordControllerTest::pausedIsStillRecordingAndStopReportsTheFile()
{
    FakeTransport transport;
    ObsClient client(transport);
    RecordController controller(&client);
    transport.reachReady();
    transport.deliver(recordState(true, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STARTED")));
    QCOMPARE(controller.state(), RecordController::State::Recording);
    // OBS sets outputActive false while paused; the recording still exists.
    transport.deliver(recordState(false, QStringLiteral("OBS_WEBSOCKET_OUTPUT_PAUSED")));
    QCOMPARE(controller.state(), RecordController::State::Paused);
    QVERIFY(controller.recording());
    QCOMPARE(controller.statusText(), QStringLiteral("Recording paused"));
    QVERIFY(controller.canToggle());

    QSignalSpy saved(&controller, &RecordController::recordingSaved);
    QSignalSpy finished(&controller, &RecordController::toggleFinished);
    QVERIFY(controller.toggle());
    QCOMPARE(transport.lastRequestType(), QStringLiteral("StopRecord"));
    QCOMPARE(controller.state(), RecordController::State::Stopping);
    transport.deliver(recordState(true, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STOPPING")));
    QCOMPARE(controller.state(), RecordController::State::Stopping);
    transport.deliver(recordState(false, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STOPPED"),
                                  QStringLiteral("/home/me/Videos/2026-09-28 21-30-00.mkv")));
    QCOMPARE(controller.state(), RecordController::State::Idle);
    QCOMPARE(saved.count(), 1);
    QCOMPARE(saved.first().at(0).toString(), QStringLiteral("/home/me/Videos/2026-09-28 21-30-00.mkv"));
    QCOMPARE(controller.lastRecordingPath(), QStringLiteral("/home/me/Videos/2026-09-28 21-30-00.mkv"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(1).toBool(), false);
    // A late duplicate reply neither re-announces nor re-settles.
    transport.deliver(responseFrame(QStringLiteral("StopRecord"), transport.idFor(QStringLiteral("StopRecord")), true,
                                    {}, QJsonObject{{QStringLiteral("outputPath"),
                                                     QStringLiteral("/home/me/Videos/2026-09-28 21-30-00.mkv")}}));
    QCOMPARE(saved.count(), 1);
    QCOMPARE(finished.count(), 1);
}

void RecordControllerTest::stopReplyCarriesThePathWhenTheEventDoesNot()
{
    FakeTransport transport;
    ObsClient client(transport);
    RecordController controller(&client);
    transport.reachReady();
    transport.deliver(recordState(true, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STARTED")));
    QSignalSpy saved(&controller, &RecordController::recordingSaved);
    QVERIFY(controller.toggle());
    transport.deliver(responseFrame(QStringLiteral("StopRecord"), transport.idFor(QStringLiteral("StopRecord")), true,
                                    {}, QJsonObject{{QStringLiteral("outputPath"), QStringLiteral("/v/a.mkv")}}));
    QCOMPARE(saved.count(), 1);
    QCOMPARE(client.snapshot().lastRecordingPath, QStringLiteral("/v/a.mkv"));
    transport.deliver(recordState(false, QStringLiteral("OBS_WEBSOCKET_OUTPUT_STOPPED")));
    QCOMPARE(controller.state(), RecordController::State::Idle);
    QCOMPARE(saved.count(), 1);
}

void RecordControllerTest::refusedAndUnconfirmedRequestsAreReported()
{
    FakeTransport transport;
    ObsClient client(transport);
    RecordController controller(&client);
    controller.setConfirmTimeout(50);
    transport.reachReady();
    QSignalSpy finished(&controller, &RecordController::toggleFinished);
    QVERIFY(controller.toggle());
    transport.deliver(responseFrame(QStringLiteral("StartRecord"), transport.idFor(QStringLiteral("StartRecord")), false,
                                    QStringLiteral("No output path is configured")));
    QCOMPARE(controller.feedback(), QStringLiteral("No output path is configured"));
    QCOMPARE(controller.state(), RecordController::State::Idle);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(0).toBool(), false);

    QVERIFY(controller.toggle());
    transport.deliver(responseFrame(QStringLiteral("StartRecord"), transport.idFor(QStringLiteral("StartRecord")), true));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.last().at(0).toBool(), false);
    QVERIFY(controller.feedback().contains(QStringLiteral("not confirmed")));
    QCOMPARE(controller.state(), RecordController::State::Idle);

    // OBS going away mid-request settles it as a failure, not a hang.
    QVERIFY(controller.toggle());
    Q_EMIT transport.disconnected(QString());
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(controller.state(), RecordController::State::Unavailable);
}

void RecordControllerTest::gateOpensOnlyForAConfirmedSetUpObs()
{
    FakeTransport transport;
    ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    std::optional<int> activePort;
    RecordConnection gate(client, secrets, preferences, [&activePort] { return activePort; });

    preferences.loaded = false;
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::NotLoaded);
    preferences.loaded = true;
    preferences.autoConnectValue = false;
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::AutoConnectOff);
    preferences.autoConnectValue = true;
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::PortNotActive);
    activePort = 4460; // OBS listens elsewhere than Settings says
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::PortNotActive);
    // AGENT-GUARD: no keyring read and no socket until the config agrees.
    QCOMPARE(secrets.reads, 0);
    QVERIFY(transport.opened.isEmpty());
    activePort = 4455;
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::NoPassword);
    QCOMPARE(secrets.reads, 1);
    QVERIFY(transport.opened.isEmpty());
    secrets.stored = QStringLiteral("secret");
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::Connected);
    QCOMPARE(transport.opened, QStringList{QStringLiteral("ws://127.0.0.1:4455")});
    // A second reconcile with nothing changed does not reconnect.
    QCOMPARE(gate.reconcile(), RecordConnection::Gate::Connected);
    QCOMPARE(transport.opened.size(), 1);
    QVERIFY(RecordConnection::gateText(RecordConnection::Gate::Connected).isEmpty());
    QVERIFY(!RecordConnection::gateText(RecordConnection::Gate::AutoConnectOff).isEmpty());
}

QTEST_GUILESS_MAIN(RecordControllerTest)
#include "tst_record_controller.moc"
