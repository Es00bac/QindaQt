// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtCore/QSet>

namespace QindaQt::Bluetooth
{
namespace
{

ValidationResult rejected(const QString &code)
{
    return {.accepted = false, .reasonCode = code};
}

bool validAdapterState(const AdapterState value)
{
    return value == AdapterState::Off || value == AdapterState::On;
}

bool validDeviceState(const DeviceState value)
{
    return value == DeviceState::Disconnected || value == DeviceState::Connecting
        || value == DeviceState::Connected;
}

bool validRssi(const qint16 value, const bool known)
{
    if (!known) {
        return true;
    }
    return value >= -127 && value <= 127;
}

bool safeDiagnostic(const QString &value)
{
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control && character != QLatin1Char('\n')
            && character != QLatin1Char('\t')) {
            return false;
        }
    }
    return true;
}

bool validHandleForEpoch(const Handle &handle, const quint64 epoch)
{
    return handle.epoch == epoch && handle.serial != 0;
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
    if (!isBoundedText(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes)
        || !isBoundedText(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes)
        || !safeDiagnostic(snapshot.diagnostic)) {
        return rejected(QStringLiteral("oversized-text"));
    }
    if (snapshot.adapters.size() > kMaxAdapters || snapshot.devices.size() > kMaxDevices) {
        return rejected(QStringLiteral("oversized-payload"));
    }
    if (snapshot.epoch == 0 || snapshot.revision == 0) {
        return rejected(QStringLiteral("invalid-lineage"));
    }

    QSet<quint64> adapterSerials;
    for (const Adapter &adapter : snapshot.adapters) {
        if (!validAdapterState(adapter.state) || !validHandleForEpoch(adapter.handle, snapshot.epoch)
            || adapterSerials.contains(adapter.handle.serial)
            || !isBoundedText(adapter.address, kMaxAddressUtf8Bytes)
            || !isBoundedText(adapter.name, kMaxNameUtf8Bytes)) {
            return rejected(QStringLiteral("invalid-adapter"));
        }
        adapterSerials.insert(adapter.handle.serial);
    }

    QSet<quint64> deviceSerials;
    for (const Device &device : snapshot.devices) {
        if (!validDeviceState(device.state) || !validHandleForEpoch(device.handle, snapshot.epoch)
            || deviceSerials.contains(device.handle.serial)
            || !validHandleForEpoch(device.adapterHandle, snapshot.epoch)
            || !adapterSerials.contains(device.adapterHandle.serial)
            || !isBoundedText(device.address, kMaxAddressUtf8Bytes)
            || !isBoundedText(device.name, kMaxNameUtf8Bytes)
            || !validRssi(device.rssi, device.rssiKnown)) {
            return rejected(QStringLiteral("invalid-device"));
        }
        deviceSerials.insert(device.handle.serial);
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
    if (kind > static_cast<quint32>(OperationKind::Untrust)
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

} // namespace QindaQt::Bluetooth
