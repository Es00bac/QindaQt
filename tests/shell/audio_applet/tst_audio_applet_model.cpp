// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_model.h"

#include <QtTest>

#include <algorithm>
#include <limits>

using namespace QindaQt::Shell::AudioApplet;

namespace {

using QindaQt::Audio::Availability;
using QindaQt::Audio::Capability;
using QindaQt::Audio::Capabilities;
using QindaQt::Audio::Device;
using QindaQt::Audio::DeviceKind;
using QindaQt::Audio::Handle;
using QindaQt::Audio::Snapshot;
using QindaQt::Audio::Stream;
using QindaQt::Audio::StreamDirection;

constexpr quint64 kEpoch = 7;
constexpr quint64 kRevision = 3;

Handle handleFor(quint64 serial)
{
    return Handle{.epoch = kEpoch, .serial = serial};
}

Device makeDevice(quint64 serial, DeviceKind kind, const QString &name,
                  const QString &description)
{
    Device device;
    device.handle = handleFor(serial);
    device.kind = kind;
    device.name = name;
    device.description = description;
    device.volume = 0.0;
    device.volumeKnown = false;
    device.muted = false;
    device.muteKnown = true;
    device.isDefault = false;
    device.canSetVolume = false;
    device.canSetMute = true;
    return device;
}

Stream makeStream(quint64 serial, const QString &application,
                  const QString &media)
{
    Stream stream;
    stream.handle = handleFor(serial);
    stream.direction = StreamDirection::Playback;
    stream.applicationName = application;
    stream.mediaName = media;
    stream.target = Handle{};
    stream.targetKnown = false;
    stream.volume = 0.0;
    stream.volumeKnown = false;
    stream.muted = false;
    stream.muteKnown = true;
    stream.canSetVolume = false;
    stream.canSetMute = true;
    stream.canMove = false;
    return stream;
}

Snapshot snapshotWithCounts(int outputs, int inputs, int streams)
{
    Snapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.epoch = kEpoch;
    snapshot.revision = kRevision;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capabilities(Capability::SetVolume)
        | Capability::SetMute;
    quint64 serial = 0;
    for (int i = 0; i < outputs; ++i) {
        Device device = makeDevice(++serial, DeviceKind::Output,
                                   QStringLiteral("out%1").arg(serial),
                                   QStringLiteral("Output %1").arg(serial));
        if (serial == 1) {
            device.isDefault = true;
            snapshot.defaultOutput = device.handle;
        }
        snapshot.outputs.append(device);
    }
    const quint64 firstInputSerial = serial;
    for (int i = 0; i < inputs; ++i) {
        Device device = makeDevice(++serial, DeviceKind::Input,
                                   QStringLiteral("in%1").arg(serial),
                                   QStringLiteral("Input %1").arg(serial));
        if (inputs > 0 && serial == firstInputSerial + 1) {
            device.isDefault = true;
            snapshot.defaultInput = device.handle;
        }
        snapshot.inputs.append(device);
    }
    for (int i = 0; i < streams; ++i)
        snapshot.streams.append(makeStream(
            ++serial, QStringLiteral("App %1").arg(serial), {}));
    return snapshot;
}

} // namespace

class AudioAppletModelTests final : public QObject {
    Q_OBJECT

private slots:
    void clampAcceptsFiniteAndRejectsNonFinite();
    void missingSnapshotKeepsCallerPhaseWithoutRows();
    void wireInvalidSnapshotFailsClosed();
    void projectsRowsInOrderWithDefaultLabels();
    void labelFallsBackFromDescriptionToNameToUnknown();
    void unknownLevelProjectsAsUnknownInsteadOfFabricated();
    void boundsRowsAndReportsOverflow();
    void defaultLabelsStayCorrectBeyondWindow();
    void pendingSerialsMarkOnlyMatchingRows();
    void degradedPhaseKeepsRowsAndReason();
};

void AudioAppletModelTests::clampAcceptsFiniteAndRejectsNonFinite()
{
    QCOMPARE(AudioAppletModel::clampVolumeLevel(-0.75).value_or(-1.0), 0.0);
    QCOMPARE(AudioAppletModel::clampVolumeLevel(0.42).value_or(-1.0), 0.42);
    QCOMPARE(AudioAppletModel::clampVolumeLevel(1.7).value_or(-1.0), 1.0);
    QCOMPARE(AudioAppletModel::clampVolumeLevel(0.0).value_or(-1.0), 0.0);
    QCOMPARE(AudioAppletModel::clampVolumeLevel(1.0).value_or(-1.0), 1.0);
    QVERIFY(!AudioAppletModel::clampVolumeLevel(std::numeric_limits<double>::quiet_NaN()).has_value());
    QVERIFY(!AudioAppletModel::clampVolumeLevel(std::numeric_limits<double>::infinity()).has_value());
    QVERIFY(!AudioAppletModel::clampVolumeLevel(-std::numeric_limits<double>::infinity()).has_value());
}

void AudioAppletModelTests::missingSnapshotKeepsCallerPhaseWithoutRows()
{
    const AudioAppletModel loading = AudioAppletModel::project(
        Phase::Loading, QStringLiteral("discovering-owner"), nullptr, {});
    QCOMPARE(loading.phase(), Phase::Loading);
    QCOMPARE(loading.phaseReasonCode(), QStringLiteral("discovering-owner"));
    QVERIFY(loading.deviceRows().isEmpty());
    QVERIFY(loading.streamRows().isEmpty());
    QVERIFY(loading.defaultOutputLabel().isEmpty());
    QVERIFY(loading.defaultInputLabel().isEmpty());
    QCOMPARE(loading.overflowDeviceCount(), 0);
    QCOMPARE(loading.overflowStreamCount(), 0);
}

void AudioAppletModelTests::wireInvalidSnapshotFailsClosed()
{
    // A snapshot can be fully populated yet invalid on the wire. The
    // projection must not show any of it, whatever phase the caller chose.
    Snapshot snapshot = snapshotWithCounts(2, 1, 1);
    snapshot.wireValid = false;
    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    QCOMPARE(model.phase(), Phase::Unavailable);
    QCOMPARE(model.phaseReasonCode(), QStringLiteral("malformed-snapshot"));
    QVERIFY(model.deviceRows().isEmpty());
    QVERIFY(model.streamRows().isEmpty());
    QVERIFY(model.defaultOutputLabel().isEmpty());
    QVERIFY(model.defaultInputLabel().isEmpty());
}

void AudioAppletModelTests::projectsRowsInOrderWithDefaultLabels()
{
    Snapshot snapshot = snapshotWithCounts(3, 2, 2);
    // Give the default input a distinct label and make one stream capture.
    snapshot.inputs[0].description = QStringLiteral("Internal Microphone");
    snapshot.streams[1].direction = StreamDirection::Capture;

    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    QCOMPARE(model.phase(), Phase::Ready);
    QCOMPARE(model.deviceRows().size(), 5);
    QCOMPARE(model.streamRows().size(), 2);
    QCOMPARE(model.overflowDeviceCount(), 0);
    QCOMPARE(model.overflowStreamCount(), 0);

    // Outputs precede inputs; each list stays in ascending-serial order.
    QCOMPARE(model.deviceRows()[0].serial(), 1ULL);
    QVERIFY(model.deviceRows()[0].isOutput());
    QVERIFY(model.deviceRows()[0].isDefault());
    QCOMPARE(model.deviceRows()[3].serial(), 4ULL);
    QVERIFY(!model.deviceRows()[3].isOutput());
    QCOMPARE(model.deviceRows()[4].serial(), 5ULL);
    QVERIFY(!model.deviceRows()[4].isOutput());

    QCOMPARE(model.defaultOutputLabel(), QStringLiteral("Output 1"));
    QCOMPARE(model.defaultInputLabel(), QStringLiteral("Internal Microphone"));

    QVERIFY(model.streamRows()[0].isPlayback());
    QVERIFY(!model.streamRows()[1].isPlayback());
    QCOMPARE(model.streamRows()[0].label(), QStringLiteral("App 6"));
}

void AudioAppletModelTests::labelFallsBackFromDescriptionToNameToUnknown()
{
    Snapshot snapshot = snapshotWithCounts(3, 0, 2);
    snapshot.outputs[0].description = QStringLiteral("Prefer me");
    snapshot.outputs[1].description.clear();
    snapshot.outputs[1].name = QStringLiteral("Named fallback");
    snapshot.outputs[2].description.clear();
    snapshot.outputs[2].name.clear();
    snapshot.streams[0].applicationName = QStringLiteral("Player");
    snapshot.streams[1].applicationName.clear();
    snapshot.streams[1].mediaName = QStringLiteral("Song");

    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    QCOMPARE(model.deviceRows()[0].label(), QStringLiteral("Prefer me"));
    QCOMPARE(model.deviceRows()[1].label(), QStringLiteral("Named fallback"));
    QCOMPARE(model.deviceRows()[2].label(), QStringLiteral("Unknown device"));
    QCOMPARE(model.streamRows()[0].label(), QStringLiteral("Player"));
    QCOMPARE(model.streamRows()[1].label(), QStringLiteral("Song"));
}

void AudioAppletModelTests::unknownLevelProjectsAsUnknownInsteadOfFabricated()
{
    Snapshot snapshot = snapshotWithCounts(1, 0, 0);
    snapshot.outputs[0].volume = 0.0;
    snapshot.outputs[0].volumeKnown = false;
    snapshot.outputs[0].muted = true;
    snapshot.outputs[0].muteKnown = false;
    snapshot.outputs[0].canSetVolume = false;
    snapshot.outputs[0].canSetMute = false;

    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    const DeviceRow &row = model.deviceRows().first();
    QVERIFY(!row.volumeKnown());
    QCOMPARE(row.volume(), 0.0);
    QVERIFY(row.muted());
    QVERIFY(!row.muteKnown());
    QVERIFY(!row.canSetVolume());
    QVERIFY(!row.canSetMute());
}

void AudioAppletModelTests::boundsRowsAndReportsOverflow()
{
    Snapshot snapshot = snapshotWithCounts(12, 6, 10);
    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    QCOMPARE(model.deviceRows().size(), kMaxDeviceRows);
    QCOMPARE(model.streamRows().size(), kMaxStreamRows);
    QCOMPARE(model.overflowDeviceCount(), 18 - kMaxDeviceRows);
    QCOMPARE(model.overflowStreamCount(), 10 - kMaxStreamRows);
    // The bounded window keeps protocol order: the first eight outputs.
    QCOMPARE(model.deviceRows().first().serial(), 1ULL);
    QCOMPARE(model.deviceRows().last().serial(), 8ULL);
    QVERIFY(std::all_of(model.deviceRows().cbegin(), model.deviceRows().cend(),
                        [](const DeviceRow &row) { return row.isOutput(); }));
}

void AudioAppletModelTests::defaultLabelsStayCorrectBeyondWindow()
{
    Snapshot snapshot = snapshotWithCounts(12, 6, 0);
    // The default input is the first input, serial 13, far beyond the
    // retained window. The label must still resolve from the snapshot.
    QCOMPARE(snapshot.defaultInput.serial, 13ULL);
    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, {});
    QCOMPARE(model.defaultInputLabel(), QStringLiteral("Input 13"));
    QVERIFY(!model.defaultOutputLabel().isEmpty());
}

void AudioAppletModelTests::pendingSerialsMarkOnlyMatchingRows()
{
    Snapshot snapshot = snapshotWithCounts(2, 1, 1);
    QSet<quint64> pending{2ULL, 4ULL};
    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Ready, {}, &snapshot, pending);
    QVERIFY(!model.deviceRows()[0].pending());
    QVERIFY(model.deviceRows()[1].pending());
    QVERIFY(!model.deviceRows()[2].pending());
    QVERIFY(model.streamRows()[0].pending());
}

void AudioAppletModelTests::degradedPhaseKeepsRowsAndReason()
{
    Snapshot snapshot = snapshotWithCounts(2, 1, 1);
    snapshot.availability = Availability::Degraded;
    const AudioAppletModel model = AudioAppletModel::project(
        Phase::Degraded, QStringLiteral("backend-malformed"), &snapshot, {});
    QCOMPARE(model.phase(), Phase::Degraded);
    QCOMPARE(model.phaseReasonCode(), QStringLiteral("backend-malformed"));
    QCOMPARE(model.deviceRows().size(), 3);
    QCOMPARE(model.streamRows().size(), 1);
}

QTEST_GUILESS_MAIN(AudioAppletModelTests)

#include "tst_audio_applet_model.moc"
