// SPDX-License-Identifier: GPL-3.0-or-later

// The Voice applet's projection, its request admission, and the controller
// that joins them. The contract this file defends is one sentence: an action
// the projection enables must be dispatchable, and one it disables must be
// inert.

#include <qindaqt/shell/voice_applet/voice_applet_controller.h>
#include <qindaqt/shell/voice_applet/voice_applet_presentation.h>
#include <qindaqt/shell/voice_applet/voice_request_state.h>

#include <QtTest/QtTest>

using namespace QindaQt::Services::Voice;
using namespace QindaQt::Shell::VoiceApplet;

namespace {

Snapshot readySnapshot(SessionState state = SessionState::Idle)
{
    Snapshot snapshot;
    snapshot.revision = 3;
    snapshot.state = state;
    snapshot.enabled = true;
    snapshot.capabilities = kKnownCapabilities;
    snapshot.lastRoute = DeliveryRoute::InputMethod;
    snapshot.providerId = QStringLiteral("elevenlabs");
    snapshot.providerLabel = QStringLiteral("ElevenLabs Scribe");
    snapshot.languageCode = QStringLiteral("en");
    snapshot.microphoneLabel = QStringLiteral("Yeti");
    snapshot.dictationShortcut = QStringLiteral("F5");
    snapshot.commandShortcut = QStringLiteral("F6");
    snapshot.lastText = QStringLiteral("the quick brown fox");
    snapshot.reasonCode = QStringLiteral("ok");
    snapshot.providers = {ProviderDescriptor{.id = QStringLiteral("elevenlabs"),
                                             .label = QStringLiteral("ElevenLabs"),
                                             .available = true}};
    return snapshot;
}

bool actionEnabled(const VoiceAppletModel &model, const char *id)
{
    for (const ActionModel &action : model.actions) {
        if (action.id == QLatin1String(id)) {
            return action.enabled;
        }
    }
    return false;
}

bool actionOffered(const VoiceAppletModel &model, const char *id)
{
    for (const ActionModel &action : model.actions) {
        if (action.id == QLatin1String(id)) {
            return true;
        }
    }
    return false;
}

// A seam with no bus and no provider: every answer is written by the test.
class StubVoiceClient final : public VoiceClientInterface {
    Q_OBJECT
public:
    ClientState clientState() const noexcept override { return state; }
    QString reasonCode() const override { return reason; }
    bool hasSnapshot() const noexcept override { return present; }
    Snapshot snapshot() const override { return current; }
    quint32 levelPercent() const noexcept override { return level; }
    void refresh() override { ++refreshes; }

    quint64 submit(OperationKind kind, const QString &providerId, bool enable) override
    {
        submissions.append(OperationRequest{.kind = kind,
                                            .requestId = nextId,
                                            .expectedRevision = current.revision,
                                            .providerId = providerId,
                                            .enable = enable});
        return nextId == 0 ? 0 : nextId++;
    }

    void publish(const Snapshot &snapshot)
    {
        current = snapshot;
        present = true;
        state = ClientState::Ready;
        Q_EMIT snapshotChanged(snapshot);
    }

    void lose(const QString &code)
    {
        present = false;
        current = {};
        state = ClientState::Unavailable;
        reason = code;
        Q_EMIT stateChanged(state, code);
    }

    void complete(OperationKind kind, OperationStatus status, quint64 requestId,
                  const QString &code = QStringLiteral("ok"))
    {
        Q_EMIT operationCompleted(
            requestId, OperationResult{.kind = kind,
                                       .status = status,
                                       .requestId = requestId,
                                       .initiatingRevision = current.revision,
                                       .observedRevision = current.revision,
                                       .reasonCode = code});
    }

    ClientState state = ClientState::Unavailable;
    QString reason = QStringLiteral("service-unavailable");
    Snapshot current;
    bool present = false;
    quint32 level = 0;
    int refreshes = 0;
    quint64 nextId = 1;
    QList<OperationRequest> submissions;
};

} // namespace

class VoiceAppletTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unavailableWithoutReadAuthority();
    void controlWithoutReadIsNotControl();
    void projectsAReadyProvider();
    void capturingProjectsThePartial();
    void levelIsZeroOutsideCapture();
    void unsupportedActionsAreNotOffered();
    void captureActionsSwapWithCaptureState();
    void undoIsDisabledWithoutALastDictation();
    void retryIsOnlyEnabledAfterAFailure();
    void transcriptPreferenceSuppressesEveryWord();

    void admissionRefusesWithoutControl();
    void admissionRefusesASecondRecording();
    void admissionRefusesWhenDisarmed();
    void uncertaintyIsNeverSuccess();

    void controllerDispatchesAnOfferedAction();
    void controllerRefusesAnUnofferedActionId();
    void controllerRefusesAnUnknownActionId();
    void controllerReportsBusyWhileAnIntentIsPending();
    void controllerResolvesAPendingIntentWhenTheProviderGoesAway();
    void controllerReportsAnUncertainResult();
    void controllerRefreshesWhenExpanded();
    void controllerRoutesAreUnavailableWithoutALauncher();
    void controllerOpensTheRoutesItWasGiven();
    void controllerAppliesTheTranscriptPreference();
};

void VoiceAppletTest::unavailableWithoutReadAuthority()
{
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 40, false,
        true, true);
    QCOMPARE(model.phase, ServicePhase::Unavailable);
    QVERIFY(model.actions.isEmpty());
    QCOMPARE(model.iconName, QStringLiteral("audio-input-microphone-muted"));
    QVERIFY(!model.diagnostic.isEmpty());
}

void VoiceAppletTest::controlWithoutReadIsNotControl()
{
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 0, false,
        true, true);
    QVERIFY(!model.controlGranted);
}

void VoiceAppletTest::projectsAReadyProvider()
{
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 0, true,
        true, true);
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.stateId, QStringLiteral("idle"));
    QCOMPARE(model.providerLabel, QStringLiteral("ElevenLabs Scribe"));
    QCOMPARE(model.routeLabel.isEmpty(), false);
    QCOMPARE(model.transcriptText, QStringLiteral("the quick brown fox"));
    QVERIFY(!model.transcriptIsPartial);
    QVERIFY(model.diagnostic.isEmpty());
    QVERIFY(model.accessibleDescription.contains(QStringLiteral("F5")));
}

void VoiceAppletTest::capturingProjectsThePartial()
{
    Snapshot snapshot = readySnapshot(SessionState::Listening);
    snapshot.partialText = QStringLiteral("the quick");
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, snapshot, 55, true, true, true);
    QVERIFY(model.capturing);
    QVERIFY(model.transcriptIsPartial);
    QCOMPARE(model.transcriptText, QStringLiteral("the quick"));
    QCOMPARE(model.levelPercent, 55u);
    QCOMPARE(model.iconName, QStringLiteral("audio-input-microphone-high"));
}

void VoiceAppletTest::levelIsZeroOutsideCapture()
{
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 90, true,
        true, true);
    QCOMPARE(model.levelPercent, 0u);
}

void VoiceAppletTest::unsupportedActionsAreNotOffered()
{
    Snapshot snapshot = readySnapshot();
    snapshot.capabilities = CapabilityNone;
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, snapshot, 0, true, true, true);
    QVERIFY(actionOffered(model, "dictate"));
    QVERIFY(actionOffered(model, "finish"));
    QVERIFY(!actionOffered(model, "command"));
    QVERIFY(!actionOffered(model, "undo"));
    QVERIFY(!actionOffered(model, "copy"));
    QVERIFY(!actionOffered(model, "retry"));
}

void VoiceAppletTest::captureActionsSwapWithCaptureState()
{
    const VoiceAppletModel idle = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 0, true,
        true, true);
    QVERIFY(actionEnabled(idle, "dictate"));
    QVERIFY(!actionEnabled(idle, "finish"));
    QVERIFY(!actionEnabled(idle, "cancel"));

    const VoiceAppletModel live = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true,
        readySnapshot(SessionState::Listening), 0, true, true, true);
    QVERIFY(!actionEnabled(live, "dictate"));
    QVERIFY(actionEnabled(live, "finish"));
    QVERIFY(actionEnabled(live, "cancel"));
}

void VoiceAppletTest::undoIsDisabledWithoutALastDictation()
{
    Snapshot snapshot = readySnapshot();
    snapshot.lastText.clear();
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, snapshot, 0, true, true, true);
    QVERIFY(actionOffered(model, "undo"));
    QVERIFY(!actionEnabled(model, "undo"));
    QVERIFY(!actionEnabled(model, "copy"));
}

void VoiceAppletTest::retryIsOnlyEnabledAfterAFailure()
{
    const VoiceAppletModel idle = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 0, true,
        true, true);
    QVERIFY(!actionEnabled(idle, "retry"));

    Snapshot failed = readySnapshot(SessionState::Error);
    failed.reasonCode = QStringLiteral("connection-failed");
    const VoiceAppletModel model = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, failed, 0, true, true, true);
    QVERIFY(actionEnabled(model, "retry"));
    QVERIFY(model.diagnostic.contains(QStringLiteral("connection-failed")));
}

void VoiceAppletTest::transcriptPreferenceSuppressesEveryWord()
{
    Snapshot capturing = readySnapshot(SessionState::Listening);
    capturing.partialText = QStringLiteral("the quick");

    const VoiceAppletModel hidden = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, capturing, 40, true, true,
        false);
    // The panel still reports that dictation is happening...
    QVERIFY(hidden.capturing);
    QCOMPARE(hidden.levelPercent, 40u);
    QVERIFY(actionEnabled(hidden, "finish"));
    // ...and stops reporting what was said, partial and delivered alike.
    QVERIFY(hidden.transcriptText.isEmpty());
    QVERIFY(!hidden.transcriptIsPartial);

    const VoiceAppletModel idleHidden = projectVoiceApplet(
        ClientState::Ready, QStringLiteral("ready"), true, readySnapshot(), 0, true,
        true, false);
    QVERIFY(idleHidden.transcriptText.isEmpty());
    // Copy and Undo still work: they act on text the provider holds, not on
    // text the panel was told not to display.
    QVERIFY(actionEnabled(idleHidden, "copy"));
    QVERIFY(actionEnabled(idleHidden, "undo"));
}

void VoiceAppletTest::admissionRefusesWithoutControl()
{
    const RequestState state = beginVoiceRequest(readySnapshot(), true,
                                                 OperationKind::StartDictation, false);
    QCOMPARE(state.phase, RequestPhase::Failed);
    QVERIFY(!state.feedback.isEmpty());
}

void VoiceAppletTest::admissionRefusesASecondRecording()
{
    const RequestState state =
        beginVoiceRequest(readySnapshot(SessionState::Listening), true,
                          OperationKind::StartDictation, true);
    QCOMPARE(state.phase, RequestPhase::Failed);
    QVERIFY(state.feedback.contains(QStringLiteral("already in progress")));
}

void VoiceAppletTest::admissionRefusesWhenDisarmed()
{
    Snapshot snapshot = readySnapshot();
    snapshot.enabled = false;
    const RequestState state =
        beginVoiceRequest(snapshot, true, OperationKind::StartDictation, true);
    QCOMPARE(state.phase, RequestPhase::Failed);
}

void VoiceAppletTest::uncertaintyIsNeverSuccess()
{
    RequestState pending = beginVoiceRequest(readySnapshot(), true,
                                             OperationKind::StartDictation, true);
    QCOMPARE(pending.phase, RequestPhase::Pending);
    pending.requestId = 5;
    const OperationResult result{.kind = OperationKind::StartDictation,
                                 .status = OperationStatus::Uncertain,
                                 .requestId = 5,
                                 .initiatingRevision = 3,
                                 .observedRevision = 3,
                                 .reasonCode = QStringLiteral("operation-timeout")};
    const RequestState settled = applyVoiceResult(pending, result);
    QCOMPARE(settled.phase, RequestPhase::Uncertain);
    QVERIFY(settled.feedback.contains(QStringLiteral("may or may not")));

    // A result for another request never settles this one.
    OperationResult foreign = result;
    foreign.requestId = 6;
    QCOMPARE(applyVoiceResult(pending, foreign).phase, RequestPhase::Pending);
}

void VoiceAppletTest::controllerDispatchesAnOfferedAction()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QVERIFY(controller.invokeAction(QStringLiteral("dictate")));
    QCOMPARE(client.submissions.size(), 1);
    QCOMPARE(client.submissions.at(0).kind, OperationKind::StartDictation);
    QVERIFY(controller.operationPending());
}

void VoiceAppletTest::controllerRefusesAnUnofferedActionId()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    Snapshot snapshot = readySnapshot();
    snapshot.capabilities = CapabilityNone;
    client.publish(snapshot);
    // "undo" is a real id, but this provider never offered it.
    QVERIFY(!controller.invokeAction(QStringLiteral("undo")));
    QVERIFY(client.submissions.isEmpty());
    QVERIFY(controller.feedbackPresent());
}

void VoiceAppletTest::controllerRefusesAnUnknownActionId()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QVERIFY(!controller.invokeAction(QStringLiteral("format-the-disk")));
    QVERIFY(client.submissions.isEmpty());
}

void VoiceAppletTest::controllerReportsBusyWhileAnIntentIsPending()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QVERIFY(controller.invokeAction(QStringLiteral("dictate")));
    QVERIFY(!controller.invokeAction(QStringLiteral("copy")));
    QCOMPARE(client.submissions.size(), 1);
    QVERIFY(controller.feedback().contains(QStringLiteral("previous request")));
}

void VoiceAppletTest::controllerResolvesAPendingIntentWhenTheProviderGoesAway()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QVERIFY(controller.invokeAction(QStringLiteral("dictate")));
    client.lose(QStringLiteral("service-unavailable"));
    QVERIFY(!controller.operationPending());
    QVERIFY(controller.feedback().contains(QStringLiteral("interrupted")));
    QCOMPARE(controller.phase(), QStringLiteral("unavailable"));
}

void VoiceAppletTest::controllerReportsAnUncertainResult()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QVERIFY(controller.invokeAction(QStringLiteral("dictate")));
    const quint64 requestId = client.submissions.at(0).requestId;
    client.complete(OperationKind::StartDictation, OperationStatus::Uncertain, requestId,
                    QStringLiteral("operation-timeout"));
    QVERIFY(!controller.operationPending());
    QVERIFY(controller.feedback().contains(QStringLiteral("may or may not")));
    // Nothing is retried on the controller's own initiative.
    QCOMPARE(client.submissions.size(), 1);
}

void VoiceAppletTest::controllerRefreshesWhenExpanded()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    controller.setExpanded(true);
    QCOMPARE(client.refreshes, 1);
    controller.setExpanded(true);
    QCOMPARE(client.refreshes, 1);
    controller.setExpanded(false);
    QCOMPARE(client.refreshes, 1);
}

void VoiceAppletTest::controllerRoutesAreUnavailableWithoutALauncher()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    QVERIFY(!controller.canOpenSettings());
    QVERIFY(!controller.canOpenConsole());
    QVERIFY(!controller.openSettings());
    QVERIFY(controller.feedbackPresent());
}

void VoiceAppletTest::controllerOpensTheRoutesItWasGiven()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    int settingsOpened = 0;
    controller.setSettingsLaunch([&settingsOpened] {
        ++settingsOpened;
        return true;
    });
    controller.setConsoleLaunch([] { return false; });
    QVERIFY(controller.canOpenSettings());
    QVERIFY(controller.openSettings());
    QCOMPARE(settingsOpened, 1);
    QVERIFY(!controller.openConsole());
    QVERIFY(controller.feedback().contains(QStringLiteral("could not be opened")));
}

void VoiceAppletTest::controllerAppliesTheTranscriptPreference()
{
    StubVoiceClient client;
    VoiceAppletController controller(&client, true, true);
    client.publish(readySnapshot());
    QCOMPARE(controller.transcriptText(), QStringLiteral("the quick brown fox"));

    controller.setTranscriptVisible(false);
    QVERIFY(controller.transcriptText().isEmpty());
    QVERIFY(!controller.transcriptIsPartial());

    controller.setTranscriptVisible(true);
    QCOMPARE(controller.transcriptText(), QStringLiteral("the quick brown fox"));
}

QTEST_MAIN(VoiceAppletTest)
#include "tst_voice_applet.moc"
