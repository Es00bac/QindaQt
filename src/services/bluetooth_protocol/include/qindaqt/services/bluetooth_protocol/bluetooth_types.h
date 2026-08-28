// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

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

// AGENT-CONTRACT: These are service-level capability bits only. Bluetooth1 v1
// deliberately has no per-adapter or per-device capability flags; adapters
// expose powered/discovering truth and devices expose paired/connected truth.
// Adding an entity-level flag requires a wire-schema revision.
enum class Capability : quint32 {
    None = 0,
    SetAdapterPower = 1U << 0U,
    DiscoveryLease = 1U << 1U,
    ConnectPaired = 1U << 2U,
    DisconnectPaired = 1U << 3U,
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class OperationKind : quint32 {
    SetAdapterPower = 0,
    AcquireDiscovery = 1,
    ReleaseDiscovery = 2,
    Connect = 3,
    Disconnect = 4,
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

    friend bool operator==(const Device &, const Device &) = default;
};

struct Snapshot {
    quint32 schemaVersion = 1;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    Capabilities capabilities;
    QString reasonCode;
    QString diagnostic;
    QList<Adapter> adapters;
    QList<Device> devices;

    // AGENT-GUARD: D-Bus decoding sets this false when an array exceeded its
    // bound while still consuming the complete argument. Clients must validate
    // it before publishing any decoded values.
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

// AGENT-NOTE: OperationRequest is an in-process value. Bluetooth1 v1 method
// calls carry their typed arguments directly on the wire; this struct is never
// D-Bus-marshalled and exists so the model, client preflight, and backends
// share one request vocabulary.
struct OperationRequest {
    OperationKind kind = OperationKind::Connect;
    Handle target;
    bool powered = false;

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
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Handle)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Adapter)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Device)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::DeviceRole)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationResult)
