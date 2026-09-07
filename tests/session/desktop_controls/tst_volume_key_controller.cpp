// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/session/desktop_controls/volume_key_controller.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Session::DesktopControls::Tests;

namespace {

Audio::Snapshot readySnapshot(double volume, bool muted, bool canSetVolume,
                              bool canSetMute)
{
    Audio::Snapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.epoch = 7;
    snapshot.revision = 3;
    snapshot.availability = Audio::Availability::Ready;
    snapshot.capabilities = Audio::Capability::SetVolume | Audio::Capability::SetMute;
    snapshot.defaultOutput = Audio::Handle{7, 42};
    Audio::Device device;
    device.handle = Audio::Handle{7, 42};
    device.kind = Audio::DeviceKind::Output;
    device.name = QStringLiteral("sink0");
    device.description = QStringLiteral("Built-in audio");
    device.volume = volume;
    device.volumeKnown = true;
    device.muted = muted;
    device.muteKnown = true;
    device.isDefault = true;
    device.canSetVolume = canSetVolume;
    device.canSetMute = canSetMute;
    snapshot.outputs.append(device);
    return snapshot;
}

Audio::OperationResult resultFor(Audio::OperationStatus status,
                                 const QString &reasonCode)
{
    Audio::OperationResult result;
    result.kind = Audio::OperationKind::SetVolume;
    result.status = status;
    result.initiatingEpoch = 7;
    result.initiatingRevision = 3;
    result.observedEpoch = 7;
    result.observedRevision = 4;
    result.reasonCode = reasonCode;
    return result;
}

} // namespace

class VolumeKeyControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void raiseVolumeStepsDefaultOutputByFivePercent();
    void lowerVolumeClampsAtZero();
    void raiseVolumeClampsAtFull();
    void toggleMuteFlipsAndReportsMutedFeedback();
    void missingSnapshotReportsUnavailableWithoutOperation();
    void unsupportedVolumeReportsUnavailableWithoutOperation();
    void rejectedOperationReportsUnavailable();
    void uncertainOperationStaysQuiet();
    void noDefaultOutputHandleReportsUnavailable();

private:
    FakeAudioTransport m_transport;
};

void VolumeKeyControllerTest::init() {
    // One shared transport per binary: drop scripted state between cases so
    // a previous case's operation never leaks into the next assertion.
    m_transport.reset();
}

void VolumeKeyControllerTest::raiseVolumeStepsDefaultOutputByFivePercent() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.5, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy feedback(&controller, &VolumeKeyController::volumeFeedbackRequested);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    controller.raiseVolume();

    QCOMPARE(unavailable.count(), 0);
    QCOMPARE(feedback.count(), 1);
    QCOMPARE(feedback.at(0).at(0).toInt(), 55);
    QCOMPARE(feedback.at(0).at(1).toBool(), false);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    const auto &operation = m_transport.operations().constFirst();
    QCOMPARE(operation.kind, Audio::OperationKind::SetVolume);
    const Audio::Handle expectedTarget{7, 42};
    QCOMPARE(operation.primary, expectedTarget);
    QCOMPARE(operation.volume, 0.55);
    client.stop();
}

void VolumeKeyControllerTest::lowerVolumeClampsAtZero() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.02, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy feedback(&controller, &VolumeKeyController::volumeFeedbackRequested);
    controller.lowerVolume();

    QCOMPARE(feedback.at(0).at(0).toInt(), 0);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QCOMPARE(m_transport.operations().constFirst().volume, 0.0);
    client.stop();
}

void VolumeKeyControllerTest::raiseVolumeClampsAtFull() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.98, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy feedback(&controller, &VolumeKeyController::volumeFeedbackRequested);
    controller.raiseVolume();

    QCOMPARE(feedback.at(0).at(0).toInt(), 100);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QCOMPARE(m_transport.operations().constFirst().volume, 1.0);
    client.stop();
}

void VolumeKeyControllerTest::toggleMuteFlipsAndReportsMutedFeedback() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.4, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy feedback(&controller, &VolumeKeyController::volumeFeedbackRequested);
    controller.toggleMute();

    QCOMPARE(feedback.count(), 1);
    QCOMPARE(feedback.at(0).at(0).toInt(), 40);
    QCOMPARE(feedback.at(0).at(1).toBool(), true);
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QCOMPARE(m_transport.operations().constFirst().kind, Audio::OperationKind::SetMute);
    QCOMPARE(m_transport.operations().constFirst().muted, true);
    client.stop();
}

void VolumeKeyControllerTest::missingSnapshotReportsUnavailableWithoutOperation() {
    Audio::AudioClient client(&m_transport);
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(!client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    QSignalSpy feedback(&controller, &VolumeKeyController::volumeFeedbackRequested);
    controller.raiseVolume();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-default-output"));
    QCOMPARE(feedback.count(), 0);
    QCOMPARE(m_transport.operations().size(), 0);
    client.stop();
}

void VolumeKeyControllerTest::unsupportedVolumeReportsUnavailableWithoutOperation() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.5, false, false, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    controller.raiseVolume();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("volume-unsupported"));
    QCOMPARE(m_transport.operations().size(), 0);
    client.stop();
}

void VolumeKeyControllerTest::rejectedOperationReportsUnavailable() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.5, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());
    m_transport.queueResult(resultFor(Audio::OperationStatus::Rejected,
                                     QStringLiteral("volume-out-of-range")));

    VolumeKeyController controller(client);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    controller.raiseVolume();
    QTRY_COMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("volume-out-of-range"));
    client.stop();
}

void VolumeKeyControllerTest::uncertainOperationStaysQuiet() {
    Audio::AudioClient client(&m_transport);
    m_transport.queueSnapshot(readySnapshot(0.5, false, true, true));
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());
    m_transport.queueResult(resultFor(Audio::OperationStatus::Uncertain,
                                     QStringLiteral("owner-replaced")));

    VolumeKeyController controller(client);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    controller.raiseVolume();
    QTRY_COMPARE(m_transport.operations().size(), 1);
    QTest::qWait(50);
    QCOMPARE(unavailable.count(), 0);
    client.stop();
}

void VolumeKeyControllerTest::noDefaultOutputHandleReportsUnavailable() {
    Audio::AudioClient client(&m_transport);
    Audio::Snapshot snapshot = readySnapshot(0.5, false, true, true);
    snapshot.defaultOutput = Audio::Handle{};
    snapshot.outputs.first().isDefault = false;
    m_transport.queueSnapshot(snapshot);
    client.start();
    m_transport.announceOwner(QStringLiteral(":1.23"));
    QTRY_VERIFY(client.hasSnapshot());

    VolumeKeyController controller(client);
    QSignalSpy unavailable(&controller, &VolumeKeyController::volumeUnavailable);
    controller.toggleMute();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-default-output"));
    client.stop();
}

QTEST_MAIN(VolumeKeyControllerTest)
#include "tst_volume_key_controller.moc"
