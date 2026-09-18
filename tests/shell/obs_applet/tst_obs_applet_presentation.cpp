// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/obs_applet/obs_applet_presentation.h>

#include <QTest>

using namespace QindaQt::Obs;
using namespace QindaQt::Shell::ObsApplet;

namespace {

ObsSnapshot readySnapshot() {
    ObsSnapshot snapshot;
    snapshot.state = ConnectionState::Ready;
    snapshot.reasonCode.clear();
    return snapshot;
}

} // namespace

class ObsAppletPresentationTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void anAbsentObsIsNamedAsSuchAndControlsNothing();
    void aRefusedPasswordIsADifferentSentenceFromAClosedObs();
    void theGlyphStatesTheMostConsequentialThing();
    void elapsedIsADashUntilObsReportsOne();
    void theDroppedWarningNeedsNumbersToStandOn();
    void theAccessibleDescriptionNamesEveryRunningOutput();
};

void ObsAppletPresentationTest::anAbsentObsIsNamedAsSuchAndControlsNothing() {
    const AppletModel model = projectApplet(ObsSnapshot{});
    QVERIFY(!model.controlAvailable);
    QCOMPARE(model.unavailableText, QStringLiteral("OBS is not running."));
    QCOMPARE(model.summaryLabel, QStringLiteral("OBS"));
    QVERIFY(!model.recording);
    QVERIFY(!model.streaming);
    QVERIFY(!model.virtualCamera);
    // Nothing is claimed about elapsed time for an OBS that is not there.
    QCOMPARE(model.recordingElapsed, QStringLiteral("—"));
    QVERIFY(model.droppedFramesWarning.isEmpty());
}

void ObsAppletPresentationTest::aRefusedPasswordIsADifferentSentenceFromAClosedObs() {
    // AGENT-CONTRACT: the user's next action is different, so the sentence is.
    ObsSnapshot refused;
    refused.state = ConnectionState::Degraded;
    refused.reasonCode = QString::fromLatin1(ReasonCodes::AuthRejected);
    QVERIFY(projectApplet(refused).unavailableText.contains(
        QStringLiteral("password")));

    ObsSnapshot version;
    version.state = ConnectionState::Degraded;
    version.reasonCode = QString::fromLatin1(ReasonCodes::RpcVersion);
    QVERIFY(projectApplet(version).unavailableText.contains(
        QStringLiteral("control protocol")));

    ObsSnapshot malformed;
    malformed.state = ConnectionState::Degraded;
    malformed.reasonCode = QString::fromLatin1(ReasonCodes::Malformed);
    QVERIFY(projectApplet(malformed).unavailableText.contains(
        QStringLiteral("could not read")));

    ObsSnapshot absent;
    absent.state = ConnectionState::Connecting;
    absent.reasonCode = QString::fromLatin1(ReasonCodes::NotRunning);
    QCOMPARE(projectApplet(absent).unavailableText,
             QStringLiteral("OBS is not running."));
}

void ObsAppletPresentationTest::theGlyphStatesTheMostConsequentialThing() {
    ObsSnapshot idle = readySnapshot();
    const AppletModel idleModel = projectApplet(idle);
    QVERIFY(idleModel.controlAvailable);
    QCOMPARE(idleModel.summaryLabel, QStringLiteral("OBS"));
    QCOMPARE(idleModel.iconName, QStringLiteral("camera-video-symbolic"));

    ObsSnapshot camera = readySnapshot();
    camera.virtualCam.active = true;
    QCOMPARE(projectApplet(camera).summaryLabel, QStringLiteral("Virtual camera"));

    ObsSnapshot recording = camera;
    recording.record.active = true;
    QCOMPARE(projectApplet(recording).summaryLabel, QStringLiteral("Recording"));

    // AGENT-GUARD: a user who is live needs to see that first, whatever else
    // OBS is also doing.
    ObsSnapshot live = recording;
    live.stream.active = true;
    QCOMPARE(projectApplet(live).summaryLabel, QStringLiteral("Streaming"));
    QCOMPARE(projectApplet(live).iconName, QStringLiteral("media-record-symbolic"));
}

void ObsAppletPresentationTest::elapsedIsADashUntilObsReportsOne() {
    QCOMPARE(formatElapsed(-1), QStringLiteral("—"));
    QCOMPARE(formatElapsed(0), QStringLiteral("00:00:00"));
    QCOMPARE(formatElapsed(3661000), QStringLiteral("01:01:01"));

    ObsSnapshot recording = readySnapshot();
    recording.record.active = true;
    recording.record.durationMs = 125000;
    QCOMPARE(projectApplet(recording).recordingElapsed, QStringLiteral("00:02:05"));
    // An output that is not running shows a dash rather than its last value.
    QCOMPARE(projectApplet(recording).streamingElapsed, QStringLiteral("—"));
}

void ObsAppletPresentationTest::theDroppedWarningNeedsNumbersToStandOn() {
    ObsSnapshot streaming = readySnapshot();
    streaming.stream.active = true;
    // OBS reported no counts.
    QVERIFY(projectApplet(streaming).droppedFramesWarning.isEmpty());

    streaming.stream.skippedFrames = 3;
    streaming.stream.totalFrames = 1000;
    // Under the threshold: noise the user cannot act on.
    QVERIFY(projectApplet(streaming).droppedFramesWarning.isEmpty());

    streaming.stream.skippedFrames = 40;
    const QString warning = projectApplet(streaming).droppedFramesWarning;
    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains(QStringLiteral("4.0")));

    // A stream that stopped carries no warning, whatever its last counters.
    streaming.stream.active = false;
    QVERIFY(projectApplet(streaming).droppedFramesWarning.isEmpty());
}

void ObsAppletPresentationTest::theAccessibleDescriptionNamesEveryRunningOutput() {
    ObsSnapshot snapshot = readySnapshot();
    QCOMPARE(projectApplet(snapshot).accessibleDescription,
             QStringLiteral("Connected to OBS and idle."));

    snapshot.record.active = true;
    snapshot.virtualCam.active = true;
    const QString description = projectApplet(snapshot).accessibleDescription;
    QVERIFY(description.contains(QStringLiteral("recording")));
    QVERIFY(description.contains(QStringLiteral("virtual camera")));
    QVERIFY(!description.contains(QStringLiteral("streaming")));
}

QTEST_APPLESS_MAIN(ObsAppletPresentationTest)
#include "tst_obs_applet_presentation.moc"
