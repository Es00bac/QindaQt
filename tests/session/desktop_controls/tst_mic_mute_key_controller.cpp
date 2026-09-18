// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/session/desktop_controls/mic_mute_key_controller.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Session::DesktopControls::Tests;

namespace {

Audio::Snapshot readyInputSnapshot(bool muted, bool canSetMute)
{
    Audio::Snapshot snapshot;
    snapshot.schemaVersion = QindaQt::Audio::kSchemaVersion;
    snapshot.epoch = 7;
    snapshot.revision = 3;
    snapshot.availability = Audio::Availability::Ready;
    snapshot.capabilities = Audio::Capability::SetMute;
    snapshot.defaultInput = Audio::Handle{7, 99};
    Audio::Device device;
    device.handle = Audio::Handle{7, 99};
    device.kind = Audio::DeviceKind::Input;
    device.name = QStringLiteral("source0");
    device.description = QStringLiteral("Built-in microphone");
    device.muted = muted;
    device.muteKnown = true;
    device.isDefault = true;
    device.canSetMute = canSetMute;
    snapshot.inputs.append(device);
    return snapshot;
}

Audio::OperationResult resultFor(Audio::OperationStatus status,
                                 const QString &reasonCode)
{
    Audio::OperationResult result;
    result.kind = Audio::OperationKind::SetMute;
    result.status = status;
    result.initiatingEpoch = 7;
    result.initiatingRevision = 3;
    result.observedEpoch = 7;
    result.observedRevision = 4;
    result.reasonCode = reasonCode;
    return result;
}

} // namespace

class MicMuteKeyControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void toggleFlipsAndReportsMutedFeedback();
    void toggleFlipsBackToUnmuted();
    void missingSnapshotReportsUnavailableWithoutOperation();
    void unsupportedMuteReportsUnavailableWithoutOperation();
    void rejectedOperationReportsUnavailable();
    void uncertainOperationStaysQuiet();
    void noDefaultInputHandleReportsUnavailable();

private:
    FakeAudioTransport m_transport;
};

void MicMuteKeyControllerTest::init() {
    m_transport.reset();
}

void MicMuteKeyControllerTest::toggleFlipsAndReportsMutedFeedback() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readyInputSnapshot(false, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    MicMuteKeyController controller(client);
    QSignalSpy feedback(&controller, &MicMuteKeyController::micMuteFeedbackRequested);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    controller.toggleMicMute();

    QCOMPARE(unavailable.count(), 0);
    QCOMPARE(feedback.count(), 1);
    QCOMPARE(feedback.at(0).at(0).toBool(), true);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    const auto &operation = m_transport.operations().constFirst();
    QCOMPARE(operation.kind, Audio::OperationKind::SetMute);
    const Audio::Handle expectedTarget{7, 99};
    QCOMPARE(operation.primary, expectedTarget);
    QCOMPARE(operation.muted, true);
    client.stop();
}

void MicMuteKeyControllerTest::toggleFlipsBackToUnmuted() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readyInputSnapshot(true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    MicMuteKeyController controller(client);
    QSignalSpy feedback(&controller, &MicMuteKeyController::micMuteFeedbackRequested);
    controller.toggleMicMute();

    QCOMPARE(feedback.at(0).at(0).toBool(), false);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QCOMPARE(m_transport.operations().constFirst().muted, false);
    client.stop();
}

void MicMuteKeyControllerTest::
    missingSnapshotReportsUnavailableWithoutOperation() {
    Audio::AudioClient client(&m_transport);
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(!client.hasSnapshot());

    MicMuteKeyController controller(client);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    QSignalSpy feedback(&controller, &MicMuteKeyController::micMuteFeedbackRequested);
    controller.toggleMicMute();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-default-input"));
    QCOMPARE(feedback.count(), 0);
    QCOMPARE(m_transport.operations().size(), 0);
    client.stop();
}

void MicMuteKeyControllerTest::
    unsupportedMuteReportsUnavailableWithoutOperation() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readyInputSnapshot(false, false));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    MicMuteKeyController controller(client);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    controller.toggleMicMute();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("mute-unsupported"));
    QCOMPARE(m_transport.operations().size(), 0);
    client.stop();
}

void MicMuteKeyControllerTest::rejectedOperationReportsUnavailable() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readyInputSnapshot(false, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());
    m_transport.queueResult(resultFor(Audio::OperationStatus::Rejected,
                                     QStringLiteral("mute-refused")));

    MicMuteKeyController controller(client);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    controller.toggleMicMute();
    QTRY_COMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("mute-refused"));
    client.stop();
}

void MicMuteKeyControllerTest::uncertainOperationStaysQuiet() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readyInputSnapshot(false, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());
    m_transport.queueResult(resultFor(Audio::OperationStatus::Uncertain,
                                     QStringLiteral("owner-replaced")));

    MicMuteKeyController controller(client);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    controller.toggleMicMute();
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QTest::qWait(50);
    QCOMPARE(unavailable.count(), 0);
    client.stop();
}

void MicMuteKeyControllerTest::noDefaultInputHandleReportsUnavailable() {
    Audio::AudioClient client(&m_transport);
    Audio::Snapshot snapshot = readyInputSnapshot(false, true);
    snapshot.defaultInput = Audio::Handle{};
    snapshot.inputs.first().isDefault = false;
    m_transport.queueSnapshot(snapshot);
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    MicMuteKeyController controller(client);
    QSignalSpy unavailable(&controller, &MicMuteKeyController::micMuteUnavailable);
    controller.toggleMicMute();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-default-input"));
    client.stop();
}

QTEST_MAIN(MicMuteKeyControllerTest)
#include "tst_mic_mute_key_controller.moc"
