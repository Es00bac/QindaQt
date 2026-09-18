// SPDX-License-Identifier: GPL-3.0-or-later
#include "obs_applet_controller.h"

#include <qindaqt/services/obs_client/obs_client.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Obs;
using namespace QindaQt::Shell::ObsApplet;

namespace {

class FakeTransport final : public ObsTransport {
    Q_OBJECT
public:
    QStringList sent;
    bool socketOpen = false;

    void open(const QString &) override {}
    void close() override { socketOpen = false; }
    void sendText(const QString &text) override { sent.append(text); }
    [[nodiscard]] bool isOpen() const override { return socketOpen; }

    void connectNow() {
        socketOpen = true;
        Q_EMIT connected();
    }
    void deliver(const QString &text) { Q_EMIT textReceived(text); }
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
};

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
                      const QString &comment = {}) {
    return QString::fromUtf8(
        QJsonDocument(
            QJsonObject{
                {QStringLiteral("op"), 7},
                {QStringLiteral("d"),
                 QJsonObject{
                     {QStringLiteral("requestType"), type},
                     {QStringLiteral("requestId"), id},
                     {QStringLiteral("requestStatus"),
                      QJsonObject{{QStringLiteral("result"), ok},
                                  {QStringLiteral("code"), ok ? 100 : 500},
                                  {QStringLiteral("comment"), comment}}}}}})
            .toJson(QJsonDocument::Compact));
}

} // namespace

class ObsAppletControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void withoutAClientEveryControlSaysWhyItDidNothing();
    void togglesDispatchTheOppositeOfWhatObsReports();
    void aRefusedRequestIsShownInTheUsersWords();
    void sceneSelectionSkipsTheSceneThatIsAlreadyLive();

private:
    void reachReady(FakeTransport &transport) {
        transport.connectNow();
        transport.deliver(QStringLiteral(
            R"({"op":0,"d":{"obsWebSocketVersion":"5.6.2","rpcVersion":1}})"));
        transport.deliver(
            QStringLiteral(R"({"op":2,"d":{"negotiatedRpcVersion":1}})"));
    }
};

void ObsAppletControllerTest::withoutAClientEveryControlSaysWhyItDidNothing() {
    // The shell builds the controller even without the control capability,
    // so the panel the user configured still has its chip.
    ObsAppletController controller(nullptr);
    QVERIFY(!controller.controlAvailable());
    QSignalSpy feedback(&controller, &ObsAppletController::feedbackChanged);

    controller.toggleRecording();
    QCOMPARE(feedback.size(), 1);
    QVERIFY(!controller.feedback().isEmpty());
    controller.toggleStreaming();
    controller.toggleVirtualCamera();
    controller.selectScene(QStringLiteral("Live"));
    // AGENT-GUARD: never a silent no-op.
    QVERIFY(!controller.feedback().isEmpty());
}

void ObsAppletControllerTest::togglesDispatchTheOppositeOfWhatObsReports() {
    FakeTransport transport;
    ObsClient client(transport);
    ObsAppletController controller(&client);
    reachReady(transport);
    QVERIFY(controller.controlAvailable());

    controller.toggleRecording();
    QCOMPARE(transport.requestTypes().last(), QStringLiteral("StartRecord"));

    // OBS says it started; the toggle now means stop.
    transport.deliver(eventFrame(QStringLiteral("RecordStateChanged"),
                                 QJsonObject{{QStringLiteral("outputActive"), true}}));
    QVERIFY(controller.recording());
    controller.toggleRecording();
    QCOMPARE(transport.requestTypes().last(), QStringLiteral("StopRecord"));

    controller.toggleVirtualCamera();
    QCOMPARE(transport.requestTypes().last(), QStringLiteral("StartVirtualCam"));
    controller.toggleStreaming();
    QCOMPARE(transport.requestTypes().last(), QStringLiteral("StartStream"));
}

void ObsAppletControllerTest::aRefusedRequestIsShownInTheUsersWords() {
    FakeTransport transport;
    ObsClient client(transport);
    ObsAppletController controller(&client);
    reachReady(transport);

    controller.toggleVirtualCamera();
    transport.deliver(responseFrame(
        QStringLiteral("StartVirtualCam"),
        transport.idFor(QStringLiteral("StartVirtualCam")), false,
        QStringLiteral("Failed to start the virtual camera")));
    QCOMPARE(controller.feedback(),
             QStringLiteral("Failed to start the virtual camera"));

    // A success clears it rather than leaving a stale complaint.
    controller.toggleRecording();
    transport.deliver(responseFrame(QStringLiteral("StartRecord"),
                                    transport.idFor(QStringLiteral("StartRecord")),
                                    true));
    QVERIFY(controller.feedback().isEmpty());
}

void ObsAppletControllerTest::sceneSelectionSkipsTheSceneThatIsAlreadyLive() {
    FakeTransport transport;
    ObsClient client(transport);
    ObsAppletController controller(&client);
    reachReady(transport);
    transport.deliver(
        eventFrame(QStringLiteral("CurrentProgramSceneChanged"),
                   QJsonObject{{QStringLiteral("sceneName"), QStringLiteral("Live")}}));
    QCOMPARE(controller.currentScene(), QStringLiteral("Live"));

    const qsizetype before = transport.sent.size();
    controller.selectScene(QStringLiteral("Live"));
    controller.selectScene(QString());
    QCOMPARE(transport.sent.size(), before);

    controller.selectScene(QStringLiteral("Break"));
    QCOMPARE(transport.requestTypes().last(),
             QStringLiteral("SetCurrentProgramScene"));
}

QTEST_MAIN(ObsAppletControllerTest)
#include "tst_obs_applet_controller.moc"
