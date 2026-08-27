// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_validation.h>

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

} // namespace

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
        | static_cast<quint32>(Capability::MoveStream);
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
            if (!isBoundedText(device.name, kMaxDisplayNameUtf8Bytes)
                || !isBoundedText(device.description, kMaxDisplayNameUtf8Bytes)
                || !validLevel(device.volume)
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
        if (!validDirection(stream.direction)
            || !validHandleForEpoch(stream.handle, snapshot.epoch)
            || stream.handle.serial <= previousStream || serials.contains(stream.handle.serial)
            || !isBoundedText(stream.applicationName, kMaxApplicationNameUtf8Bytes)
            || !isBoundedText(stream.mediaName, kMaxDisplayNameUtf8Bytes)
            || !validLevel(stream.volume)
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
            const bool compatible = stream.direction == StreamDirection::Playback
                ? outputSerials.contains(stream.target.serial)
                : inputSerials.contains(stream.target.serial);
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
    if (kind > static_cast<quint32>(OperationKind::MoveStream)
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
