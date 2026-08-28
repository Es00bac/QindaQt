// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Bluetooth
{

enum class AdapterState : quint32 {
    Off = 0,
    On = 1,
};

enum class AdapterCapability : quint32 {
    None = 0,
    Discover = 1U << 0U,
    Pair = 1U << 1U,
    Connect = 1U << 2U,
};
Q_DECLARE_FLAGS(AdapterCapabilities, AdapterCapability)

enum class DeviceState : quint32 {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
};

enum class DeviceCapability : quint32 {
    None = 0,
    Pair = 1U << 0U,
    Connect = 1U << 1U,
    Disconnect = 1U << 2U,
    Trust = 1U << 3U,
};
Q_DECLARE_FLAGS(DeviceCapabilities, DeviceCapability)

enum class OperationKind : quint32 {
    Pair = 0,
    Connect = 1,
    Disconnect = 2,
    Trust = 3,
    Untrust = 4,
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
    AdapterState state = AdapterState::Off;
    bool discoveringActive = false;
    AdapterCapabilities capabilities;

    friend bool operator==(const Adapter &, const Adapter &) = default;
};

struct Device {
    Handle handle;
    Handle adapterHandle;
    QString address;
    QString name;
    DeviceState state = DeviceState::Disconnected;
    qint16 rssi = 0;
    bool rssiKnown = false;
    bool paired = false;
    bool trusted = false;
    DeviceCapabilities capabilities;

    friend bool operator==(const Device &, const Device &) = default;
};

struct Snapshot {
    quint32 schemaVersion = 1;
    quint64 epoch = 0;
    quint64 revision = 0;
    QString reasonCode;
    QString diagnostic;
    QList<Adapter> adapters;
    QList<Device> devices;

    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationRequest {
    OperationKind kind = OperationKind::Pair;
    Handle primary;
    Handle secondary;

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::Pair;
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

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Bluetooth::AdapterCapabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Bluetooth::DeviceCapabilities)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::AdapterState)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::AdapterCapabilities)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::DeviceState)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::DeviceCapabilities)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Handle)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Adapter)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Device)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationResult)
