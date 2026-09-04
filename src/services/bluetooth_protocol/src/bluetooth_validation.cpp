// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::Bluetooth
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

bool validDeviceClass(const DeviceClass value)
{
    return value >= DeviceClass::Unknown && value <= DeviceClass::Tag;
}

bool validDeviceRole(const DeviceRole value)
{
    return value >= DeviceRole::Unknown && value <= DeviceRole::CentralPeripheral;
}

bool validBattery(const Device &device)
{
    // AGENT-GUARD: Unknown battery is exactly zero; a reported percentage
    // must stay inside BlueZ's [0, 100] range. Anything else is fabricated
    // data and fails closed.
    if (!device.batteryKnown) {
        return device.batteryPercent == 0;
    }
    return device.batteryPercent <= 100;
}

bool sixDigits(const QString &value)
{
    if (value.size() != 6) {
        return false;
    }
    return std::all_of(value.cbegin(), value.cend(), [](const QChar character) {
        return character >= QLatin1Char('0') && character <= QLatin1Char('9');
    });
}

bool validPin(const QString &value)
{
    if (value.isEmpty() || value.size() > 16
        || !isBoundedText(value, kMaxPairingTextUtf8Bytes)) {
        return false;
    }
    return std::all_of(value.cbegin(), value.cend(), [](const QChar character) {
        return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
            || (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
            || (character >= QLatin1Char('A') && character <= QLatin1Char('Z'));
    });
}

bool validPromptKind(const PairingPromptKind value)
{
    return value >= PairingPromptKind::None
        && value <= PairingPromptKind::AuthorizeService;
}

bool validOperationKind(const OperationKind value)
{
    return value >= OperationKind::SetAdapterPower && value <= OperationKind::CancelPrompt;
}

bool isPromptOperation(const OperationKind value)
{
    return value >= OperationKind::ReplyConfirmation
        && value <= OperationKind::CancelPrompt;
}

bool validOperationStatus(const OperationStatus value)
{
    return value >= OperationStatus::Succeeded && value <= OperationStatus::Busy;
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

// Known capability bits for the v2 schema. Any other bit in a decoded or
// backend-provided snapshot is rejected rather than ignored.
constexpr quint32 knownCapabilityBits()
{
    return static_cast<quint32>(Capability::SetAdapterPower)
        | static_cast<quint32>(Capability::DiscoveryLease)
        | static_cast<quint32>(Capability::ConnectPaired)
        | static_cast<quint32>(Capability::DisconnectPaired)
        | static_cast<quint32>(Capability::Pair)
        | static_cast<quint32>(Capability::RemoveDevice)
        | static_cast<quint32>(Capability::SetTrusted)
        | static_cast<quint32>(Capability::PairingPrompt);
}

constexpr quint32 readyCapabilityBits()
{
    return knownCapabilityBits();
}

bool validRssi(const Device &device)
{
    if (!device.rssiKnown) {
        // AGENT-GUARD: Unknown signal strength is exactly zero. A nonzero
        // value with rssiKnown false is fabricated data and must fail closed.
        return device.rssi == 0;
    }
    return device.rssi >= -128 && device.rssi <= 0;
}

ValidationResult validatePrompt(const PairingPrompt &prompt, const Snapshot &snapshot,
                                const QSet<quint64> &deviceSerials)
{
    if (!validPromptKind(prompt.kind)) {
        return rejected(QStringLiteral("invalid-pairing-prompt"));
    }
    if (!prompt.active()) {
        return prompt.promptId == 0 && !prompt.device.isValid() && prompt.detail.isEmpty()
                && prompt.serviceUuid.isEmpty() && prompt.entered == 0
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    }
    if (prompt.promptId == 0 || snapshot.availability != Availability::Ready
        || !validHandleForEpoch(prompt.device, snapshot.epoch)
        || !deviceSerials.contains(prompt.device.serial)
        || !isBoundedText(prompt.detail, kMaxPairingTextUtf8Bytes)
        || !isBoundedText(prompt.serviceUuid, kMaxPairingTextUtf8Bytes)) {
        return rejected(QStringLiteral("invalid-pairing-prompt"));
    }
    switch (prompt.kind) {
    case PairingPromptKind::ConfirmPasskey:
        return sixDigits(prompt.detail) && prompt.serviceUuid.isEmpty()
                && prompt.entered == 0
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    case PairingPromptKind::EnterPasskey:
    case PairingPromptKind::EnterPin:
        return prompt.detail.isEmpty() && prompt.serviceUuid.isEmpty()
                && prompt.entered == 0
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    case PairingPromptKind::DisplayPasskey:
        return sixDigits(prompt.detail) && prompt.serviceUuid.isEmpty()
                && prompt.entered <= 6
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    case PairingPromptKind::DisplayPin:
        return validPin(prompt.detail) && prompt.serviceUuid.isEmpty()
                && prompt.entered == 0
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    case PairingPromptKind::AuthorizeService:
        return prompt.detail.isEmpty() && !prompt.serviceUuid.isEmpty()
                && prompt.entered == 0
            ? ValidationResult{.accepted = true, .reasonCode = {}}
            : rejected(QStringLiteral("invalid-pairing-prompt"));
    case PairingPromptKind::None:
        break;
    }
    return rejected(QStringLiteral("invalid-pairing-prompt"));
}

} // namespace

bool isBoundedText(const QString &value, const qsizetype maxUtf8Bytes)
{
    return !value.contains(QChar::Null) && value.toUtf8().size() <= maxUtf8Bytes;
}

bool isCanonicalAddress(const QString &value)
{
    if (value.size() != kAddressUtf8Bytes) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        const bool separator = (index % 3) == 2;
        if (separator) {
            if (character != QLatin1Char(':')) {
                return false;
            }
        } else {
            const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
            const bool upperHex = character >= QLatin1Char('A') && character <= QLatin1Char('F');
            if (!digit && !upperHex) {
                return false;
            }
        }
    }
    return true;
}

bool isStructuredReasonCode(const QString &value)
{
    if (value.isEmpty() || !isBoundedText(value, kMaxReasonCodeUtf8Bytes)) {
        return false;
    }
    for (const QChar character : value) {
        const bool lowerAscii = character >= QLatin1Char('a')
            && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!lowerAscii && !digit && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
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
    if ((static_cast<quint32>(snapshot.capabilities.toInt()) & ~knownCapabilityBits()) != 0) {
        return rejected(QStringLiteral("invalid-capabilities"));
    }
    if (!isStructuredReasonCode(snapshot.reasonCode)
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
    if (snapshot.availability != Availability::Ready && snapshot.capabilities != Capabilities{}) {
        return rejected(QStringLiteral("inconsistent-capabilities"));
    }
    if (snapshot.availability != Availability::Ready
        && (!snapshot.adapters.isEmpty() || !snapshot.devices.isEmpty())) {
        return rejected(QStringLiteral("inconsistent-inventory"));
    }

    QSet<quint64> serials;
    quint64 previousAdapterSerial = 0;
    for (const Adapter &adapter : snapshot.adapters) {
        if (!validHandleForEpoch(adapter.handle, snapshot.epoch)
            || adapter.handle.serial <= previousAdapterSerial
            || serials.contains(adapter.handle.serial)) {
            return rejected(QStringLiteral("invalid-adapter-order"));
        }
        if (!isCanonicalAddress(adapter.address)
            || !isBoundedText(adapter.name, kMaxAdapterNameUtf8Bytes)) {
            return rejected(QStringLiteral("invalid-adapter"));
        }
        if (!adapter.powered && adapter.discovering) {
            return rejected(QStringLiteral("invalid-adapter-state"));
        }
        previousAdapterSerial = adapter.handle.serial;
        serials.insert(previousAdapterSerial);
    }

    QSet<quint64> adapterSerials = serials;
    QSet<QString> deviceAddresses;
    QSet<quint64> deviceSerials;
    quint64 previousDeviceSerial = 0;
    for (const Device &device : snapshot.devices) {
        if (!validHandleForEpoch(device.handle, snapshot.epoch)
            || device.handle.serial <= previousDeviceSerial
            || serials.contains(device.handle.serial)
            || !validHandleForEpoch(device.adapterHandle, snapshot.epoch)
            || !adapterSerials.contains(device.adapterHandle.serial)) {
            return rejected(QStringLiteral("invalid-device-order"));
        }
        if (!validDeviceClass(device.deviceClass)
            || !validDeviceRole(device.role)
            || !isCanonicalAddress(device.address)
            || deviceAddresses.contains(device.address)
            || !isBoundedText(device.name, kMaxDeviceNameUtf8Bytes)
            || !validRssi(device)
            || !validBattery(device)) {
            return rejected(QStringLiteral("invalid-device"));
        }
        // AGENT-GUARD: Bluetooth truth invariants. A connected device must be
        // paired, and a connection may exist only while its adapter is
        // powered; BlueZ drops connections when an adapter powers off. A
        // merely known device may remain listed on an unpowered adapter.
        if (device.connected && !device.paired) {
            return rejected(QStringLiteral("invalid-device-state"));
        }
        const auto adapterIt = std::find_if(
            snapshot.adapters.cbegin(), snapshot.adapters.cend(),
            [&](const Adapter &adapter) {
                return adapter.handle == device.adapterHandle;
            });
        if (adapterIt == snapshot.adapters.cend()
            || (device.connected && !adapterIt->powered)) {
            return rejected(QStringLiteral("invalid-device-state"));
        }
        previousDeviceSerial = device.handle.serial;
        serials.insert(previousDeviceSerial);
        deviceSerials.insert(previousDeviceSerial);
        deviceAddresses.insert(device.address);
    }

    const ValidationResult promptValidation =
        validatePrompt(snapshot.pairingPrompt, snapshot, deviceSerials);
    if (!promptValidation.accepted) {
        return promptValidation;
    }

    if (snapshot.availability == Availability::Ready
        && snapshot.capabilities != Capabilities::fromInt(readyCapabilityBits())) {
        return rejected(QStringLiteral("inconsistent-capabilities"));
    }

    return {.accepted = true, .reasonCode = {}};
}

ValidationResult validateOperationResult(const OperationResult &result)
{
    if (!result.wireValid) {
        return rejected(QStringLiteral("malformed-result"));
    }
    if (!validOperationKind(result.kind) || !validOperationStatus(result.status)) {
        return rejected(QStringLiteral("malformed-result"));
    }
    if (result.initiatingEpoch == 0 || result.initiatingRevision == 0
        || result.observedEpoch == 0 || result.observedRevision == 0
        || !isStructuredReasonCode(result.reasonCode)
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

ValidationResult validateOperationRequest(const OperationRequest &request)
{
    if (!validOperationKind(request.kind)) {
        return rejected(QStringLiteral("malformed-request"));
    }
    if (!request.target.isValid()) {
        return rejected(QStringLiteral("stale-handle"));
    }
    if ((isPromptOperation(request.kind) && request.promptId == 0)
        || (!isPromptOperation(request.kind) && request.promptId != 0)) {
        return rejected(QStringLiteral("malformed-request"));
    }
    if (request.inputSize > 16) {
        return rejected(QStringLiteral("malformed-request"));
    }
    const QString input = pairingInputString(request.input, request.inputSize);
    if (request.kind == OperationKind::ReplyPasskey) {
        bool ok = false;
        const quint32 passkey = input.toUInt(&ok);
        if (!ok || input.isEmpty() || input.size() > 6
            || passkey > 999999 || !sixDigits(input.rightJustified(6, QLatin1Char('0')))) {
            return rejected(QStringLiteral("malformed-request"));
        }
    } else if (request.kind == OperationKind::ReplyPin && !validPin(input)) {
        return rejected(QStringLiteral("malformed-request"));
    } else if (request.kind != OperationKind::ReplyPin
               && request.kind != OperationKind::ReplyPasskey
               && !input.isEmpty()) {
        return rejected(QStringLiteral("malformed-request"));
    }
    return {.accepted = true, .reasonCode = {}};
}

} // namespace QindaQt::Bluetooth
