// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <array>

namespace QindaQt::Bluetooth
{

enum class Availability : quint32 {
    Starting = 0,
    Ready = 1,
    Unavailable = 2,
    Degraded = 3,
};

enum class DeviceClass : quint32 {
    Unknown = 0,
    Computer = 1,
    Phone = 2,
    AudioVideo = 3,
    Headset = 4,
    Headphones = 5,
    Keyboard = 6,
    Mouse = 7,
    Tablet = 8,
    Printer = 9,
    GameInput = 10,
    Wearable = 11,
    Tag = 12,
};

// GAP connection role. Unknown means the platform did not report one;
// Bluetooth1 never fabricates a role.
enum class DeviceRole : quint32 {
    Unknown = 0,
    Central = 1,
    Peripheral = 2,
    CentralPeripheral = 3,
};

// AGENT-CONTRACT: These are service-level capability bits only. Entity truth
// remains in the fixed adapter/device values; callers must check both the
// service bit and the current entity state before dispatch.
enum class Capability : quint32 {
    None = 0,
    SetAdapterPower = 1U << 0U,
    DiscoveryLease = 1U << 1U,
    ConnectPaired = 1U << 2U,
    DisconnectPaired = 1U << 3U,
    Pair = 1U << 4U,
    RemoveDevice = 1U << 5U,
    SetTrusted = 1U << 6U,
    PairingPrompt = 1U << 7U,
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class OperationKind : quint32 {
    SetAdapterPower = 0,
    AcquireDiscovery = 1,
    ReleaseDiscovery = 2,
    Connect = 3,
    Disconnect = 4,
    Pair = 5,
    CancelPairing = 6,
    RemoveDevice = 7,
    SetTrusted = 8,
    ReplyConfirmation = 9,
    ReplyPasskey = 10,
    ReplyPin = 11,
    CancelPrompt = 12,
};

enum class PairingPromptKind : quint32 {
    None = 0,
    ConfirmPasskey = 1,
    EnterPasskey = 2,
    EnterPin = 3,
    DisplayPasskey = 4,
    DisplayPin = 5,
    AuthorizeService = 6,
};

enum class OperationStatus : quint32 {
    Succeeded = 0,
    Rejected = 1,
    Unsupported = 2,
    Failed = 3,
    Uncertain = 4,
    Busy = 5,
};

struct Handle {
    quint64 epoch = 0;
    quint64 serial = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return epoch != 0 && serial != 0;
    }

    friend bool operator==(const Handle &, const Handle &) = default;
};

struct Adapter {
    Handle handle;
    QString address;
    QString name;
    bool powered = false;
    bool discovering = false;

    friend bool operator==(const Adapter &, const Adapter &) = default;
};

struct Device {
    Handle handle;
    Handle adapterHandle;
    QString address;
    QString name;
    DeviceClass deviceClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unknown;
    bool paired = false;
    bool connected = false;
    bool rssiKnown = false;
    qint16 rssi = 0;
    // Optional battery percentage in the BlueZ-reported range [0, 100].
    // batteryKnown == false must carry batteryPercent == 0.
    bool batteryKnown = false;
    quint8 batteryPercent = 0;
    bool trusted = false;

    friend bool operator==(const Device &, const Device &) = default;
};

// Bluetooth1's frozen v1 wire shape. Keep this projection distinct from the
// current Device value so additions cannot silently alter old decoders.
struct Bluetooth1Device {
    Handle handle;
    Handle adapterHandle;
    QString address;
    QString name;
    DeviceClass deviceClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unknown;
    bool paired = false;
    bool connected = false;
    bool rssiKnown = false;
    qint16 rssi = 0;
    bool batteryKnown = false;
    quint8 batteryPercent = 0;

    friend bool operator==(const Bluetooth1Device &, const Bluetooth1Device &) = default;
};

// One bounded prompt is part of the same epoch/revision snapshot as device
// truth. `detail` carries a zero-padded passkey/PIN when displayed;
// `serviceUuid` is populated only for AuthorizeService. An inactive prompt is
// exactly the default value and carries no stale device handle or text.
struct PairingPrompt {
    quint64 promptId = 0;
    PairingPromptKind kind = PairingPromptKind::None;
    Handle device;
    QString detail;
    QString serviceUuid;
    quint16 entered = 0;

    [[nodiscard]] bool active() const noexcept
    {
        return kind != PairingPromptKind::None;
    }

    friend bool operator==(const PairingPrompt &, const PairingPrompt &) = default;
};

// AGENT-CONTRACT: org.qindaqt.Bluetooth1.GetSnapshot stays byte-compatible
// with the pre-pairing v1 ABI. Current clients use Snapshot on the additive
// org.qindaqt.Bluetooth2 interface.
struct Bluetooth1Snapshot {
    quint32 schemaVersion = 1;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    Capabilities capabilities;
    QString reasonCode;
    QString diagnostic;
    QList<Adapter> adapters;
    QList<Bluetooth1Device> devices;
    bool wireValid = true;

    friend bool operator==(const Bluetooth1Snapshot &, const Bluetooth1Snapshot &) = default;
};

struct Snapshot {
    quint32 schemaVersion = 2;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    Capabilities capabilities;
    QString reasonCode;
    QString diagnostic;
    QList<Adapter> adapters;
    QList<Device> devices;
    PairingPrompt pairingPrompt;

    // AGENT-GUARD: D-Bus decoding sets this false when an array exceeded its
    // bound while still consuming the complete argument. Clients must validate
    // it before publishing any decoded values.
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

using PairingInput = std::array<char, 17>;

[[nodiscard]] inline QString pairingInputString(const PairingInput &input,
                                                const quint8 size)
{
    return QString::fromLatin1(input.data(), static_cast<qsizetype>(size));
}

inline bool setPairingInput(PairingInput &input, quint8 &size,
                            const QString &value)
{
    input.fill('\0');
    size = 0;
    if (value.size() > 16) {
        return false;
    }
    for (const QChar character : value) {
        const char byte = character.toLatin1();
        if (byte == '\0') {
            input.fill('\0');
            size = 0;
            return false;
        }
        input.at(static_cast<std::size_t>(size++)) = byte;
    }
    return true;
}

// AGENT-NOTE: OperationRequest is an in-process value. Bluetooth1 v1 method
// calls carry their typed arguments directly on the wire; this struct is never
// D-Bus-marshalled and exists so the model, client preflight, and backends
// share one request vocabulary.
struct OperationRequest {
    OperationKind kind = OperationKind::Connect;
    Handle target;
    bool powered = false;
    bool accepted = false;
    bool trusted = false;
    quint64 promptId = 0;
    PairingInput input{};
    quint8 inputSize = 0;

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::Connect;
    OperationStatus status = OperationStatus::Failed;
    quint64 initiatingEpoch = 0;
    quint64 initiatingRevision = 0;
    quint64 observedEpoch = 0;
    quint64 observedRevision = 0;
    QString reasonCode;
    QString diagnostic;
    bool wireValid = true;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

} // namespace QindaQt::Bluetooth

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Bluetooth::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::PairingPromptKind)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Handle)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Adapter)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Device)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Bluetooth1Device)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::DeviceRole)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::PairingPrompt)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Bluetooth1Snapshot)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationResult)
