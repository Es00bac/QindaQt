// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_controller.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace QindaQt::Shell::AudioApplet;

namespace {

using QindaQt::Audio::AudioClient;
using QindaQt::Audio::AudioTransport;
using QindaQt::Audio::Availability;
using QindaQt::Audio::Capability;
using QindaQt::Audio::Capabilities;
using QindaQt::Audio::Device;
using QindaQt::Audio::DeviceKind;
using QindaQt::Audio::Handle;
using QindaQt::Audio::OperationKind;
using QindaQt::Audio::OperationRequest;
using QindaQt::Audio::OperationResult;
using QindaQt::Audio::OperationStatus;
using QindaQt::Audio::Snapshot;
using QindaQt::Audio::Stream;
using QindaQt::Audio::StreamDirection;

constexpr quint64 kEpoch = 11;
constexpr quint64 kRevision = 4;
const QString kOwner = QStringLiteral(":1.42");

Handle handleFor(quint64 serial)
{
    return Handle{.epoch = kEpoch, .serial = serial};
}

Device makeDevice(quint64 serial, DeviceKind kind, const QString &description,
                  bool defaultDevice)
{
    Device device;
    device.handle = handleFor(serial);
    device.kind = kind;
    device.name = QStringLiteral("dev%1").arg(serial);
    device.description = description;
    device.volume = 0.5;
    device.volumeKnown = true;
    device.muted = false;
    device.muteKnown = true;
    device.isDefault = defaultDevice;
    device.canSetVolume = true;
    device.canSetMute = true;
    return device;
}

// One default output (serial 1), one limited input (serial 2: unknown volume,
// no volume control), one full default input (serial 3), and one playback
// stream (serial 4) targeting the default output. Serials strictly ascend
// across all lists, as Audio1 validation requires.
Snapshot makeReadySnapshot()
{
    Snapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.epoch = kEpoch;
    snapshot.revision = kRevision;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capabilities(Capability::SetVolume)
        | Capability::SetMute;

    Device output = makeDevice(1, DeviceKind::Output,
                               QStringLiteral("Built-in Speakers"), true);
    snapshot.defaultOutput = output.handle;
    snapshot.outputs.append(output);

    Device limitedInput = makeDevice(2, DeviceKind::Input,
                                     QStringLiteral("Limited Microphone"),
                                     false);
    limitedInput.volume = 0.0;
    limitedInput.volumeKnown = false;
    limitedInput.canSetVolume = false;
    snapshot.inputs.append(limitedInput);

    Device input = makeDevice(3, DeviceKind::Input,
                              QStringLiteral("Webcam Microphone"), true);
    snapshot.defaultInput = input.handle;
    snapshot.inputs.append(input);

    Stream stream;
    stream.handle = handleFor(4);
    stream.direction = StreamDirection::Playback;
    stream.applicationName = QStringLiteral("Music Player");
    stream.mediaName = QStringLiteral("Song");
    stream.target = handleFor(1);
    stream.targetKnown = true;
    stream.volume = 0.4;
    stream.volumeKnown = true;
    stream.muted = false;
    stream.muteKnown = true;
    stream.canSetVolume = true;
    stream.canSetMute = true;
    stream.canMove = false;
    snapshot.streams.append(stream);

    return snapshot;
}

// Re-stamp every handle in the snapshot to the given epoch so a new lineage
// stays internally consistent under Audio1 validation.
Snapshot withEpoch(Snapshot snapshot, quint64 epoch)
{
    snapshot.epoch = epoch;
    snapshot.revision = 1;
    for (Device &device : snapshot.outputs)
        device.handle.epoch = epoch;
    for (Device &device : snapshot.inputs)
        device.handle.epoch = epoch;
    for (Stream &stream : snapshot.streams) {
        stream.handle.epoch = epoch;
        if (stream.targetKnown)
            stream.target.epoch = epoch;
    }
    snapshot.defaultOutput.epoch = epoch;
    snapshot.defaultInput.epoch = epoch;
    return snapshot;
}

OperationResult makeResult(OperationKind kind, OperationStatus status,
                           const QString &reasonCode)
{
    return {.kind = kind,
            .status = status,
            .initiatingEpoch = kEpoch,
            .initiatingRevision = kRevision,
            .observedEpoch = kEpoch,
            .observedRevision = kRevision,
            .reasonCode = reasonCode,
            .diagnostic = {},
            .wireValid = true};
}

class FakeTransport final : public AudioTransport {
public:
    explicit FakeTransport(QObject *parent = nullptr) : AudioTransport(parent) {}

    struct Fetch {
        QString owner;
        quint64 requestId = 0;
    };
    struct Submission {
        QString owner;
        quint64 requestId = 0;
        OperationRequest request;
    };

    void start() override { m_started = true; }
    void stop() override { m_stopped = true; }
    void fetchSnapshot(const QString &owner, quint64 requestId) override
    {
        fetches.append(Fetch{owner, requestId});
    }
    void submitOperation(const QString &owner, quint64 requestId,
                         const OperationRequest &request) override
    {
        submissions.append(Submission{owner, requestId, request});
    }

    void changeOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void invalidate(quint64 epoch, quint64 revision)
    {
        Q_EMIT invalidated(kOwner, epoch, revision);
    }
    void deliverSnapshot(quint64 requestId, bool success,
                         const Snapshot &snapshot, const QString &reason = {})
    {
        Q_EMIT snapshotReply(kOwner, requestId, success, snapshot, reason);
    }
    void deliverOperation(quint64 requestId, bool success,
                          const OperationResult &result,
                          const QString &reason = {})
    {
        Q_EMIT operationReply(kOwner, requestId, success, result, reason);
    }

    bool m_started = false;
    bool m_stopped = false;
    QList<Fetch> fetches;
    QList<Submission> submissions;
};

} // namespace

class AudioAppletControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void stoppedClientProjectsLoadingWithoutRows();
    void readySnapshotProjectsRowsDefaultsAndLabels();
    void invalidWireSnapshotFailsClosedThroughClient();
    void volumeRequestClampsBeforeDispatch();
    void nonFiniteVolumeIsRefusedWhileOutOfRangeClamps();
    void uncapableRowIsRefusedWithoutDispatch();
    void secondRequestWhilePendingIsRefused();
    void rejectedResultPublishesUnsupportedFeedback();
    void uncertainResultPublishesConfirmationFeedback();
    void successClearsPendingWithoutFeedback();
    void pendingPrunesWhenSerialVanishesAndLateReplyIsIgnored();
    void degradedSnapshotKeepsRowsWithReason();
    void unavailableSnapshotFailsClosedWithReason();

private:
    void publishReadySnapshot();
    void deliverSnapshotAfterRefetch(const Snapshot &snapshot);
    [[nodiscard]] int countPendingDeviceRows() const;
    [[nodiscard]] int countPendingStreamRows() const;

    FakeTransport *m_transport = nullptr;
    AudioClient *m_client = nullptr;
    AudioAppletController *m_controller = nullptr;
};

void AudioAppletControllerTests::init()
{
    m_transport = new FakeTransport(this);
    m_client = new AudioClient(m_transport, this);
    m_controller = new AudioAppletController(m_client, this);
}

void AudioAppletControllerTests::cleanup()
{
    delete m_controller;
    m_controller = nullptr;
    delete m_client;
    m_client = nullptr;
    delete m_transport;
    m_transport = nullptr;
}

void AudioAppletControllerTests::publishReadySnapshot()
{
    m_client->start();
    m_transport->changeOwner(kOwner);
    QVERIFY(!m_transport->fetches.isEmpty());
    m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                 true, makeReadySnapshot());
    QCOMPARE(m_controller->phaseText(), QStringLiteral("ready"));
}

void AudioAppletControllerTests::deliverSnapshotAfterRefetch(
    const Snapshot &snapshot)
{
    // A revision or epoch change reaches the client as an invalidation, which
    // starts a new fetch that must be answered for the snapshot to publish.
    QVERIFY(!m_transport->fetches.isEmpty());
    m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                 true, snapshot);
}

int AudioAppletControllerTests::countPendingDeviceRows() const
{
    int pending = 0;
    const QVariantList rows = m_controller->deviceRows();
    for (const QVariant &value : rows) {
        if (value.value<DeviceRow>().pending())
            ++pending;
    }
    return pending;
}

int AudioAppletControllerTests::countPendingStreamRows() const
{
    int pending = 0;
    const QVariantList rows = m_controller->streamRows();
    for (const QVariant &value : rows) {
        if (value.value<StreamRow>().pending())
            ++pending;
    }
    return pending;
}

void AudioAppletControllerTests::stoppedClientProjectsLoadingWithoutRows()
{
    QCOMPARE(m_controller->phaseText(), QStringLiteral("loading"));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
    QVERIFY(!m_controller->feedbackPresent());
    QVERIFY(!m_controller->hasDefaultOutput());
    QVERIFY(!m_controller->hasDefaultInput());
}

void AudioAppletControllerTests::readySnapshotProjectsRowsDefaultsAndLabels()
{
    publishReadySnapshot();

    QCOMPARE(m_controller->deviceRows().size(), 3);
    QCOMPARE(m_controller->streamRows().size(), 1);
    QCOMPARE(m_controller->overflowDeviceCount(), 0);
    QCOMPARE(m_controller->overflowStreamCount(), 0);
    QVERIFY(m_controller->hasDefaultOutput());
    QCOMPARE(m_controller->defaultOutputLabel(),
             QStringLiteral("Built-in Speakers"));
    QVERIFY(m_controller->hasDefaultInput());
    QCOMPARE(m_controller->defaultInputLabel(),
             QStringLiteral("Webcam Microphone"));

    const auto defaultRow =
        m_controller->deviceRows().at(0).value<DeviceRow>();
    QCOMPARE(defaultRow.serial(), 1ULL);
    QVERIFY(defaultRow.isDefault());
    QVERIFY(defaultRow.canSetVolume());
    QVERIFY(defaultRow.volumeKnown());
    QVERIFY(!defaultRow.pending());
    QCOMPARE(defaultRow.volume(), 0.5);

    const auto limitedRow =
        m_controller->deviceRows().at(1).value<DeviceRow>();
    QVERIFY(!limitedRow.volumeKnown());
    QVERIFY(!limitedRow.canSetVolume());
    QVERIFY(limitedRow.canSetMute());

    const auto streamRow =
        m_controller->streamRows().at(0).value<StreamRow>();
    QCOMPARE(streamRow.label(), QStringLiteral("Music Player"));
    QVERIFY(streamRow.isPlayback());
    QVERIFY(streamRow.canSetVolume());
}

void AudioAppletControllerTests::invalidWireSnapshotFailsClosedThroughClient()
{
    m_client->start();
    m_transport->changeOwner(kOwner);
    QVERIFY(!m_transport->fetches.isEmpty());
    Snapshot snapshot = makeReadySnapshot();
    snapshot.wireValid = false;
    m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                 true, snapshot);
    QCOMPARE(m_controller->phaseText(), QStringLiteral("unavailable"));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
    QVERIFY(!m_controller->phaseReasonText().isEmpty());
}

void AudioAppletControllerTests::volumeRequestClampsBeforeDispatch()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestVolume(1, false, 1.7));
    QCOMPARE(m_transport->submissions.size(), 1);
    const auto submission = m_transport->submissions.constFirst();
    QCOMPARE(submission.request.kind, OperationKind::SetVolume);
    QCOMPARE(submission.request.primary.serial, 1ULL);
    QCOMPARE(submission.request.primary.epoch, kEpoch);
    QCOMPARE(submission.request.volume, 1.0);
    QCOMPARE(countPendingDeviceRows(), 1);

    m_transport->deliverOperation(
        submission.requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::nonFiniteVolumeIsRefusedWhileOutOfRangeClamps()
{
    publishReadySnapshot();

    // A non-finite level is domain-invalid: refused locally, never dispatched,
    // and always explained by feedback.
    QSignalSpy feedbackSpy(m_controller,
                           &AudioAppletController::feedbackChanged);
    QVERIFY(!m_controller->requestVolume(
        1, false, std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(m_transport->submissions.size(), 0);
    QCOMPARE(feedbackSpy.size(), 1);
    QVERIFY(m_controller->feedbackPresent());
    QCOMPARE(countPendingDeviceRows(), 0);

    // A finite out-of-range level is clamped into range and dispatched.
    m_controller->clearFeedback();
    QVERIFY(m_controller->requestVolume(1, false, 42.0));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(m_transport->submissions.constFirst().request.volume, 1.0);
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::uncapableRowIsRefusedWithoutDispatch()
{
    publishReadySnapshot();

    QVERIFY(!m_controller->requestVolume(2, false, 0.3));
    QCOMPARE(m_transport->submissions.size(), 0);
    QCOMPARE(m_controller->feedback(),
             AudioAppletController::tr(
                 "This device does not support volume changes."));
    QCOMPARE(countPendingDeviceRows(), 0);

    // The same row still allows a mute request through.
    QVERIFY(m_controller->requestMute(2, false, true));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(m_transport->submissions.constFirst().request.kind,
             OperationKind::SetMute);
    QCOMPARE(m_transport->submissions.constFirst().request.muted, true);
    QCOMPARE(countPendingDeviceRows(), 1);

    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::secondRequestWhilePendingIsRefused()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(1, false, true));
    QCOMPARE(m_transport->submissions.size(), 1);
    QVERIFY(!m_controller->requestVolume(1, false, 0.2));
    QCOMPARE(m_controller->feedback(),
             AudioAppletController::tr(
                 "A change for this item is already in progress."));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(countPendingDeviceRows(), 1);

    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
    // The refusal feedback stays visible until the user dismisses it.
    QVERIFY(m_controller->feedbackPresent());
}

void AudioAppletControllerTests::rejectedResultPublishesUnsupportedFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(3, false, true));
    m_transport->deliverOperation(
        m_transport->submissions.constLast().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Unsupported,
                   QStringLiteral("unsupported")));
    QTRY_COMPARE(m_controller->feedback(),
                 AudioAppletController::tr(
                     "This device does not support that change."));
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::uncertainResultPublishesConfirmationFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(1, false, true));
    // A transport loss is classified uncertain by the client and must reach
    // the user as an unconfirmed change, never as silent success.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, false,
        makeResult(OperationKind::SetMute, OperationStatus::Failed, {}),
        QStringLiteral("operation-timeout"));
    QTRY_COMPARE(
        m_controller->feedback(),
        AudioAppletController::tr("The %1 change could not be confirmed. "
                                  "Shown state will refresh from the audio "
                                  "service.")
            .arg(AudioAppletController::tr("mute")));
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::successClearsPendingWithoutFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestVolume(4, true, 0.9));
    const auto submission = m_transport->submissions.constFirst();
    QCOMPARE(submission.request.kind, OperationKind::SetVolume);
    QCOMPARE(submission.request.primary.serial, 4ULL);
    QCOMPARE(submission.request.volume, 0.9);
    QCOMPARE(countPendingStreamRows(), 1);
    QCOMPARE(countPendingDeviceRows(), 0);
    QVERIFY(!m_controller->feedbackPresent());

    m_transport->deliverOperation(
        submission.requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingStreamRows(), 0);
    QVERIFY(!m_controller->feedbackPresent());
}

void AudioAppletControllerTests::
    pendingPrunesWhenSerialVanishesAndLateReplyIsIgnored()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(2, false, true));
    QCOMPARE(countPendingDeviceRows(), 1);

    // A same-owner revision drops serial 2 from the graph. Reprojection must
    // clear the stale pending flag without waiting for the operation.
    m_transport->invalidate(kEpoch, kRevision + 1);
    Snapshot replacement = makeReadySnapshot();
    replacement.revision = kRevision + 1;
    replacement.inputs.removeAt(0);
    deliverSnapshotAfterRefetch(replacement);
    QCOMPARE(m_controller->deviceRows().size(), 2);
    QCOMPARE(countPendingDeviceRows(), 0);

    // The still-dispatched mute completes after its serial vanished. The late
    // completion is ignored: no feedback and no state change.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTest::qWait(50);
    QVERIFY(!m_controller->feedbackPresent());
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::degradedSnapshotKeepsRowsWithReason()
{
    publishReadySnapshot();

    Snapshot degraded = withEpoch(makeReadySnapshot(), kEpoch + 1);
    degraded.availability = Availability::Degraded;
    degraded.reasonCode = QStringLiteral("backend-malformed");
    m_transport->invalidate(kEpoch + 1, 1);
    deliverSnapshotAfterRefetch(degraded);

    QCOMPARE(m_controller->phaseText(), QStringLiteral("degraded"));
    QVERIFY(!m_controller->phaseReasonText().isEmpty());
    QCOMPARE(m_controller->deviceRows().size(), 3);
    QCOMPARE(m_controller->streamRows().size(), 1);
    QVERIFY(!m_controller->feedbackPresent());
}

void AudioAppletControllerTests::unavailableSnapshotFailsClosedWithReason()
{
    publishReadySnapshot();

    Snapshot unavailable = withEpoch(makeReadySnapshot(), kEpoch + 1);
    unavailable.availability = Availability::Unavailable;
    unavailable.reasonCode = QStringLiteral("unavailable");
    m_transport->invalidate(kEpoch + 1, 1);
    deliverSnapshotAfterRefetch(unavailable);

    QCOMPARE(m_controller->phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(
        m_controller->phaseReasonText(),
        AudioAppletController::tr("The audio service is not available right now."));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
}

QTEST_GUILESS_MAIN(AudioAppletControllerTests)

#include "tst_audio_applet_controller.moc"
