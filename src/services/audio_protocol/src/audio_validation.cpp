// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <QSet>

#include <cmath>

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QSet>

#include <cmath>

namespace QindaQt::Audio
{
namespace
{

ValidationResult rejected(const QString &code)
{
    return {.accepted = false, .reasonCode = code};
}

bool validAvailability(const Availability value)
{
    return value == Availability::Starting || value == Availability::Ready
        || value == Availability::Unavailable || value == Availability::Degraded;
}

bool validKind(const DeviceKind value)
{
    return value == DeviceKind::Output || value == DeviceKind::Input;
}

bool validDirection(const StreamDirection value)
{
    return value == StreamDirection::Playback || value == StreamDirection::Capture;
}

bool validLevel(const double value)
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool safeDiagnostic(const QString &value)
{
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            && character != QLatin1Char('\n') && character != QLatin1Char('\t')) {
            return false;
        }
    }
    return true;
}

bool validHandleForEpoch(const Handle &handle, const quint64 epoch)
{
    return handle.epoch == epoch && handle.serial != 0;
}

bool validOptionalHandleForEpoch(const Handle &handle, const quint64 epoch)
{
    return (handle.epoch == 0 && handle.serial == 0) || validHandleForEpoch(handle, epoch);
}

bool validChannelLayout(const QVector<double> &channelVolumes, const QStringList &channelMap)
{
    if (channelVolumes.size() > kMaxChannelsPerDevice
        || channelMap.size() > kMaxChannelsPerDevice) {
        return false;
    }
    for (const double level : channelVolumes) {
        if (!validLevel(level)) {
            return false;
        }
    }
    for (const QString &label : channelMap) {
        if (label.isEmpty() || label.contains(QChar::Null)
            || !isBoundedText(label, kMaxChannelNameUtf8Bytes)) {
            return false;
        }
    }
    // A known layout is the projection target for per-channel volumes: when
    // both are present they must describe the same channel count.
    if (!channelMap.isEmpty() && !channelVolumes.isEmpty()
        && channelVolumes.size() != channelMap.size()) {
        return false;
    }
    return true;
}

} // namespace

bool operationTargetsHandle(const OperationKind kind) noexcept
{
    switch (kind) {
    case OperationKind::CreateVirtualDevice:
    // Console operations (ADR-0173) name a strip or bus id, never a handle.
    case OperationKind::SetStripGain:
    case OperationKind::SetStripMute:
    case OperationKind::SetStripSolo:
    case OperationKind::SetStripMono:
    case OperationKind::SetStripPan:
    case OperationKind::SetStripTrim:
    case OperationKind::SetStripSend:
    case OperationKind::SetBusGain:
    case OperationKind::SetBusMute:
    case OperationKind::SetBusMono:
        return false;
    // SetBusTarget and SetStripSource name a console element AND the device it
    // should follow, so a valid handle is checked like any other; an INVALID
    // handle on these two means "back to automatic" and is admitted by the
    // callers as such (ADR-0178).
    case OperationKind::SetBusTarget:
    case OperationKind::SetStripSource:
    case OperationKind::SetDefault:
    case OperationKind::SetVolume:
    case OperationKind::SetMute:
    case OperationKind::MoveStream:
    case OperationKind::SetChannelVolumes:
    case OperationKind::RemoveVirtualDevice:
        return true;
    }
    return true;
}

bool isBoundedText(const QString &value, const qsizetype maxUtf8Bytes)
{
    return !value.contains(QChar::Null) && value.toUtf8().size() <= maxUtf8Bytes;
}

QString boundedSafeDiagnostic(QString value)
{
    value.remove(QChar::Null);
    for (auto &character : value) {
        if (character.category() == QChar::Other_Control && character != QLatin1Char('\n')
            && character != QLatin1Char('\t')) {
            character = QLatin1Char(' ');
        }
    }
    QByteArray bytes = value.toUtf8();
    if (bytes.size() <= kMaxDiagnosticUtf8Bytes) {
        return value;
    }
    bytes.truncate(kMaxDiagnosticUtf8Bytes);
    while (!QString::fromUtf8(bytes).toUtf8().startsWith(bytes) && !bytes.isEmpty()) {
        bytes.chop(1);
    }
    return QString::fromUtf8(bytes);
}

namespace
{

[[nodiscard]] bool validGainDb(double value)
{
    return std::isfinite(value) && value >= kMinGainDb && value <= kMaxGainDb;
}

[[nodiscard]] bool validMeterDb(double value)
{
    // A meter reads dBFS and must never be fed the fader's range; an out-of-band
    // value here means a producer confused the two scales.
    return std::isfinite(value) && value >= kSilentMeterDb && value <= kMaxMeterDb;
}

[[nodiscard]] bool validLevel(const Level &level)
{
    return validMeterDb(level.peakDb) && validMeterDb(level.rmsDb)
        && level.rmsDb <= level.peakDb;
}

[[nodiscard]] bool validConsoleIdentity(const QString &id, const QString &label)
{
    return !id.isEmpty() && isBoundedText(id, kMaxConsoleIdUtf8Bytes)
        && isBoundedText(label, kMaxConsoleLabelUtf8Bytes);
}

} // namespace

ValidationResult validateConsole(const Console &console)
{
    if (!console.wireValid) {
        return rejected(QStringLiteral("oversized-payload"));
    }
    if (console.strips.size() > kMaxStrips || console.buses.size() > kMaxBuses) {
        return rejected(QStringLiteral("oversized-payload"));
    }

    QSet<QString> busIds;
    QSet<quint32> busIndices;
    for (const Bus &bus : console.buses) {
        if (!bus.wireValid) {
            return rejected(QStringLiteral("oversized-payload"));
        }
        if (!validConsoleIdentity(bus.id, bus.label)
            || !isBoundedText(bus.pinnedTarget, kMaxNodeNameUtf8Bytes)) {
            return rejected(QStringLiteral("invalid-bus-identity"));
        }
        // AGENT-GUARD: a duplicate id or index would let two console cells
        // address the same bus, so a routing change would land on whichever
        // the consumer happened to iterate first.
        if (busIds.contains(bus.id) || busIndices.contains(bus.index)) {
            return rejected(QStringLiteral("duplicate-bus"));
        }
        if (bus.index >= static_cast<quint32>(kMaxBuses)) {
            return rejected(QStringLiteral("invalid-bus-index"));
        }
        if (bus.kind != BusKind::Physical && bus.kind != BusKind::Virtual) {
            return rejected(QStringLiteral("invalid-bus-kind"));
        }
        if (!validGainDb(bus.gainDb) || !validLevel(bus.level)) {
            return rejected(QStringLiteral("invalid-bus-level"));
        }
        busIds.insert(bus.id);
        busIndices.insert(bus.index);
    }

    QSet<QString> stripIds;
    for (const Strip &strip : console.strips) {
        if (!strip.wireValid) {
            return rejected(QStringLiteral("oversized-payload"));
        }
        if (!validConsoleIdentity(strip.id, strip.label)
            || !isBoundedText(strip.pinnedSource, kMaxNodeNameUtf8Bytes)) {
            return rejected(QStringLiteral("invalid-strip-identity"));
        }
        if (stripIds.contains(strip.id)) {
            return rejected(QStringLiteral("duplicate-strip"));
        }
        if (strip.kind != StripKind::HardwareInput
            && strip.kind != StripKind::VirtualInput) {
            return rejected(QStringLiteral("invalid-strip-kind"));
        }
        if (!validGainDb(strip.gainDb) || !validLevel(strip.level)) {
            return rejected(QStringLiteral("invalid-strip-level"));
        }
        if (!std::isfinite(strip.pan) || strip.pan < kMinPan || strip.pan > kMaxPan) {
            return rejected(QStringLiteral("invalid-strip-pan"));
        }
        if (strip.channelTrimDb.size() > kMaxChannelsPerDevice) {
            return rejected(QStringLiteral("oversized-payload"));
        }
        for (const double trim : strip.channelTrimDb) {
            if (!validGainDb(trim)) {
                return rejected(QStringLiteral("invalid-strip-trim"));
            }
        }
        if (strip.sends.size() > kMaxSendsPerStrip) {
            return rejected(QStringLiteral("oversized-payload"));
        }
        QSet<quint32> sendTargets;
        for (const MatrixSend &send : strip.sends) {
            // AGENT-GUARD: every send must name a bus this console actually
            // publishes. A send to an unknown bus is a routing instruction
            // nothing can carry out, and silently dropping it would leave the
            // user's matrix showing a connection that does not exist.
            if (!busIndices.contains(send.busIndex)) {
                return rejected(QStringLiteral("send-without-bus"));
            }
            if (sendTargets.contains(send.busIndex)) {
                return rejected(QStringLiteral("duplicate-send"));
            }
            if (!validGainDb(send.gainDb)) {
                return rejected(QStringLiteral("invalid-send-gain"));
            }
            sendTargets.insert(send.busIndex);
        }
        stripIds.insert(strip.id);
    }

    // soloActive is published truth, not a hint: a console that disagrees with
    // its own strips would dim the wrong faders.
    bool anySolo = false;
    for (const Strip &strip : console.strips) {
        anySolo = anySolo || strip.soloed;
    }
    if (console.soloActive != anySolo) {
        return rejected(QStringLiteral("inconsistent-solo"));
    }
    return {.accepted = true, .reasonCode = {}};
}

ValidationResult validateSnapshot(const Snapshot &snapshot)
{
    if (!snapshot.wireValid) {
        return rejected(QStringLiteral("oversized-payload"));
    }
    if (snapshot.schemaVersion != kSchemaVersion) {
        return rejected(QStringLiteral("unsupported-version"));
    }
    if (!validAvailability(snapshot.availability)) {
        return rejected(QStringLiteral("invalid-availability"));
    }
    constexpr quint32 knownCapabilities = static_cast<quint32>(Capability::SetDefault)
        | static_cast<quint32>(Capability::SetVolume)
        | static_cast<quint32>(Capability::SetMute)
        | static_cast<quint32>(Capability::MoveStream)
        | static_cast<quint32>(Capability::SetChannelVolumes)
        | static_cast<quint32>(Capability::ManageVirtualDevices)
        | static_cast<quint32>(Capability::Console)
        | static_cast<quint32>(Capability::SetConsoleGain)
        | static_cast<quint32>(Capability::SetConsoleRouting)
        | static_cast<quint32>(Capability::ConsoleMeters);
    if ((static_cast<quint32>(snapshot.capabilities.toInt()) & ~knownCapabilities) != 0) {
        return rejected(QStringLiteral("invalid-capabilities"));
    }
    if (!isBoundedText(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes)
        || !isBoundedText(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes)
        || !safeDiagnostic(snapshot.diagnostic)) {
        return rejected(QStringLiteral("oversized-text"));
    }
    if (snapshot.outputs.size() > kMaxOutputs || snapshot.inputs.size() > kMaxInputs
        || snapshot.streams.size() > kMaxStreams) {
        return rejected(QStringLiteral("oversized-payload"));
    }
    if (const auto console = validateConsole(snapshot.console); !console.accepted) {
        return console;
    }
    if (snapshot.epoch == 0 || snapshot.revision == 0) {
        return rejected(QStringLiteral("invalid-lineage"));
    }
    if (!validOptionalHandleForEpoch(snapshot.defaultOutput, snapshot.epoch)
        || !validOptionalHandleForEpoch(snapshot.defaultInput, snapshot.epoch)) {
        return rejected(QStringLiteral("invalid-default-handle"));
    }

    QSet<quint64> serials;
    QSet<quint64> outputSerials;
    QSet<quint64> inputSerials;
    auto validateDeviceList = [&](const QList<Device> &devices, const DeviceKind expected,
                                  QSet<quint64> &kindSerials) -> ValidationResult {
        quint64 previous = 0;
        for (const Device &device : devices) {
            if (!validKind(device.kind) || device.kind != expected
                || !validHandleForEpoch(device.handle, snapshot.epoch)
                || device.handle.serial <= previous || serials.contains(device.handle.serial)) {
                return rejected(QStringLiteral("invalid-device-order"));
            }
            if (!device.wireValid
                || !isBoundedText(device.name, kMaxDisplayNameUtf8Bytes)
                || !isBoundedText(device.description, kMaxDisplayNameUtf8Bytes)
                || !isBoundedText(device.nodeName, kMaxNodeNameUtf8Bytes)
                || !validLevel(device.volume)
                || !validChannelLayout(device.channelVolumes, device.channelMap)
                || (device.canSetVolume
                    && (!device.volumeKnown
                        || !snapshot.capabilities.testFlag(Capability::SetVolume)))
                || (device.canSetMute
                    && (!device.muteKnown
                        || !snapshot.capabilities.testFlag(Capability::SetMute)))) {
                return rejected(QStringLiteral("invalid-device"));
            }
            previous = device.handle.serial;
            serials.insert(previous);
            kindSerials.insert(previous);
        }
        return {.accepted = true, .reasonCode = {}};
    };

    if (const auto result = validateDeviceList(snapshot.outputs, DeviceKind::Output,
                                                outputSerials);
        !result.accepted) {
        return result;
    }
    if (const auto result = validateDeviceList(snapshot.inputs, DeviceKind::Input, inputSerials);
        !result.accepted) {
        return result;
    }
    if (snapshot.defaultOutput.isValid()
        && !outputSerials.contains(snapshot.defaultOutput.serial)) {
        return rejected(QStringLiteral("missing-default-output"));
    }
    if (snapshot.defaultInput.isValid() && !inputSerials.contains(snapshot.defaultInput.serial)) {
        return rejected(QStringLiteral("missing-default-input"));
    }
    for (const Device &device : snapshot.outputs) {
        if (device.isDefault != (device.handle == snapshot.defaultOutput)) {
            return rejected(QStringLiteral("inconsistent-default-output"));
        }
    }
    for (const Device &device : snapshot.inputs) {
        if (device.isDefault != (device.handle == snapshot.defaultInput)) {
            return rejected(QStringLiteral("inconsistent-default-input"));
        }
    }

    quint64 previousStream = 0;
    for (const Stream &stream : snapshot.streams) {
        if (!stream.wireValid
            || !validDirection(stream.direction)
            || !validHandleForEpoch(stream.handle, snapshot.epoch)
            || stream.handle.serial <= previousStream || serials.contains(stream.handle.serial)
            || !isBoundedText(stream.applicationName, kMaxApplicationNameUtf8Bytes)
            || !isBoundedText(stream.mediaName, kMaxDisplayNameUtf8Bytes)
            || !validLevel(stream.volume)
            || !validChannelLayout(stream.channelVolumes, stream.channelMap)
            || (stream.canSetVolume
                && (!stream.volumeKnown
                    || !snapshot.capabilities.testFlag(Capability::SetVolume)))
            || (stream.canSetMute
                && (!stream.muteKnown
                    || !snapshot.capabilities.testFlag(Capability::SetMute)))
            || (stream.canMove
                && !snapshot.capabilities.testFlag(Capability::MoveStream))) {
            return rejected(QStringLiteral("invalid-stream"));
        }
        if (stream.targetKnown) {
            // A capture stream may read an output device's monitor (ADR-0175);
            // a playback stream into an input is still impossible.
            const bool compatible = stream.direction == StreamDirection::Playback
                ? outputSerials.contains(stream.target.serial)
                : (inputSerials.contains(stream.target.serial)
                   || outputSerials.contains(stream.target.serial));
            if (!validHandleForEpoch(stream.target, snapshot.epoch) || !compatible) {
                return rejected(QStringLiteral("invalid-stream-target"));
            }
        } else if (stream.target != Handle{}) {
            return rejected(QStringLiteral("unexpected-stream-target"));
        }
        previousStream = stream.handle.serial;
        serials.insert(previousStream);
    }

    return {.accepted = true, .reasonCode = {}};
}

ValidationResult validateOperationResult(const OperationResult &result)
{
    if (!result.wireValid) {
        return rejected(QStringLiteral("malformed-result"));
    }
    const auto kind = static_cast<quint32>(result.kind);
    const auto status = static_cast<quint32>(result.status);
    if (kind > static_cast<quint32>(OperationKind::RemoveVirtualDevice)
        || status > static_cast<quint32>(OperationStatus::Busy)) {
        return rejected(QStringLiteral("malformed-result"));
    }
    if (result.initiatingEpoch == 0 || result.initiatingRevision == 0
        || result.observedEpoch == 0 || result.observedRevision == 0
        || !isBoundedText(result.reasonCode, kMaxReasonCodeUtf8Bytes)
        || !isBoundedText(result.diagnostic, kMaxDiagnosticUtf8Bytes)
        || !safeDiagnostic(result.diagnostic)) {
        return rejected(QStringLiteral("malformed-result"));
    }
    if (result.status == OperationStatus::Succeeded
        && (result.observedEpoch != result.initiatingEpoch
            || result.observedRevision < result.initiatingRevision)) {
        return rejected(QStringLiteral("invalid-success-lineage"));
    }
    return {.accepted = true, .reasonCode = {}};
}

} // namespace QindaQt::Audio
