// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtDBus/QDBusMetaType>
#include <QtTest>

#include <limits>

using namespace QindaQt::Audio;

namespace
{

Snapshot validSnapshot()
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = 19;
    snapshot.revision = 4;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capability::SetDefault | Capability::SetVolume
        | Capability::SetMute | Capability::MoveStream;
    snapshot.defaultOutput = {.epoch = 19, .serial = 100};
    snapshot.defaultInput = {.epoch = 19, .serial = 200};
    snapshot.outputs = {{.handle = {.epoch = 19, .serial = 100},
                         .kind = DeviceKind::Output,
                         .name = QStringLiteral("Speakers"),
                         .description = QStringLiteral("Null sink"),
                         .volume = 0.5,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .isDefault = true,
                         .canSetVolume = true,
                         .canSetMute = true}};
    snapshot.inputs = {{.handle = {.epoch = 19, .serial = 200},
                        .kind = DeviceKind::Input,
                        .name = QStringLiteral("Microphone"),
                        .description = QStringLiteral("Null source"),
                        .volume = 0.75,
                        .volumeKnown = true,
                        .muted = true,
                        .muteKnown = true,
                        .isDefault = true,
                        .canSetVolume = true,
                        .canSetMute = true}};
    snapshot.streams = {{.handle = {.epoch = 19, .serial = 300},
                         .direction = StreamDirection::Playback,
                         .applicationName = QStringLiteral("Player"),
                         .mediaName = QStringLiteral("Music"),
                         .target = {.epoch = 19, .serial = 100},
                         .targetKnown = true,
                         .volume = 0.25,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .canMove = true}};
    return snapshot;
}

} // namespace

class AudioProtocolTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fixedSignatures();
    void acceptsCanonicalSnapshot();
    void rejectsMalformedLineageAndKinds();
    void rejectsUnsortedDuplicateAndStaleHandles();
    void rejectsInvalidLevelsAndText();
    void rejectsOversizedCollections();
    void operationResultLineage();
    void rejectsInconsistentCapabilitiesDefaultsAndDiagnostics();
};

void AudioProtocolTests::fixedSignatures()
{
    registerDBusTypes();
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Handle>()), "(tt)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Device>()),
             "((tt)ussdbbbbbb)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Stream>()),
             "((tt)uss(tt)bdbbbbbb)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Snapshot>()),
             "(uttuuss(tt)(tt)a((tt)ussdbbbbbb)a((tt)ussdbbbbbb)"
             "a((tt)uss(tt)bdbbbbbb))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<OperationResult>()),
             "(uuttttss)");
}

void AudioProtocolTests::acceptsCanonicalSnapshot()
{
    const auto validation = validateSnapshot(validSnapshot());
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));
}

void AudioProtocolTests::rejectsMalformedLineageAndKinds()
{
    Snapshot snapshot = validSnapshot();
    snapshot.schemaVersion++;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unsupported-version"));
    snapshot = validSnapshot();
    snapshot.epoch = 0;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-lineage"));
    snapshot = validSnapshot();
    snapshot.outputs[0].kind = static_cast<DeviceKind>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device-order"));
    snapshot = validSnapshot();
    snapshot.availability = static_cast<Availability>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-availability"));
}

void AudioProtocolTests::rejectsUnsortedDuplicateAndStaleHandles()
{
    Snapshot snapshot = validSnapshot();
    Device second = snapshot.outputs[0];
    second.handle.serial = 99;
    second.isDefault = false;
    snapshot.outputs.push_back(second);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.inputs[0].handle.serial = snapshot.outputs[0].handle.serial;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.streams[0].handle.epoch++;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));

    snapshot = validSnapshot();
    snapshot.streams[0].target.serial = 200;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-stream-target"));
}

void AudioProtocolTests::rejectsInvalidLevelsAndText()
{
    Snapshot snapshot = validSnapshot();
    snapshot.outputs[0].volume = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));
    snapshot = validSnapshot();
    snapshot.streams[0].volume = 1.01;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));
    snapshot = validSnapshot();
    snapshot.outputs[0].name = QString(kMaxDisplayNameUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));
    snapshot = validSnapshot();
    snapshot.reasonCode = QString(kMaxReasonCodeUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-text"));
}

void AudioProtocolTests::rejectsOversizedCollections()
{
    Snapshot snapshot = validSnapshot();
    snapshot.outputs.clear();
    snapshot.defaultOutput = {};
    for (qsizetype index = 0; index <= kMaxOutputs; ++index) {
        Device device = validSnapshot().outputs[0];
        device.handle.serial = static_cast<quint64>(index + 1);
        device.isDefault = false;
        snapshot.outputs.push_back(device);
    }
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-payload"));
    snapshot = validSnapshot();
    snapshot.wireValid = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-payload"));
}

void AudioProtocolTests::operationResultLineage()
{
    OperationResult result{.kind = OperationKind::SetVolume,
                           .status = OperationStatus::Succeeded,
                           .initiatingEpoch = 3,
                           .initiatingRevision = 5,
                           .observedEpoch = 3,
                           .observedRevision = 6,
                           .reasonCode = QStringLiteral("ok"),
                           .diagnostic = {},
                           .wireValid = true};
    QVERIFY(validateOperationResult(result).accepted);
    result.observedEpoch++;
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-success-lineage"));
    result.status = static_cast<OperationStatus>(99);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));
}

void AudioProtocolTests::rejectsInconsistentCapabilitiesDefaultsAndDiagnostics()
{
    Snapshot snapshot = validSnapshot();
    snapshot.capabilities = Capabilities::fromInt(1U << 31U);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-capabilities"));
    snapshot = validSnapshot();
    snapshot.outputs[0].isDefault = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("inconsistent-default-output"));
    snapshot = validSnapshot();
    snapshot.streams[0].targetKnown = false;
    snapshot.streams[0].target = {.epoch = 0, .serial = 100};
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("unexpected-stream-target"));
    snapshot = validSnapshot();
    snapshot.diagnostic = QString(QChar(0x0001));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-text"));
}

QTEST_GUILESS_MAIN(AudioProtocolTests)
#include "tst_audio_protocol.moc"
