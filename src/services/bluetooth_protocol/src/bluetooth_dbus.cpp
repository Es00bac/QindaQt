// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Bluetooth
{
namespace
{

template<typename T>
void writeArray(QDBusArgument &argument, const QList<T> &values)
{
    argument.beginArray(QMetaType::fromType<T>());
    for (const auto &value : values) {
        argument << value;
    }
    argument.endArray();
}

template<typename T>
void readBoundedArray(const QDBusArgument &argument, QList<T> &values,
                      const qsizetype limit, bool &wireValid)
{
    values.clear();
    argument.beginArray();
    while (!argument.atEnd()) {
        T value;
        argument >> value;
        if (values.size() < limit) {
            values.push_back(std::move(value));
        } else {
            wireValid = false;
        }
    }
    argument.endArray();
}

} // namespace

void registerDBusTypes()
{
    qRegisterMetaType<Handle>();
    qRegisterMetaType<Adapter>();
    qRegisterMetaType<Device>();
    qRegisterMetaType<Bluetooth1Device>();
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<Bluetooth1Snapshot>();
    qRegisterMetaType<PairingPrompt>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<Handle>();
    qDBusRegisterMetaType<Adapter>();
    qDBusRegisterMetaType<Device>();
    qDBusRegisterMetaType<Bluetooth1Device>();
    qDBusRegisterMetaType<Snapshot>();
    qDBusRegisterMetaType<Bluetooth1Snapshot>();
    qDBusRegisterMetaType<PairingPrompt>();
    qDBusRegisterMetaType<OperationResult>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value)
{
    argument.beginStructure();
    argument << value.epoch << value.serial;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value)
{
    argument.beginStructure();
    argument >> value.epoch >> value.serial;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Adapter &value)
{
    argument.beginStructure();
    argument << value.handle << value.address << value.name << value.powered
             << value.discovering;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Adapter &value)
{
    argument.beginStructure();
    argument >> value.handle >> value.address >> value.name >> value.powered
        >> value.discovering;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Device &value)
{
    argument.beginStructure();
    argument << value.handle << value.adapterHandle << value.address << value.name
             << static_cast<quint32>(value.deviceClass)
             << static_cast<quint32>(value.role) << value.paired << value.connected
             << value.rssiKnown << value.rssi << value.batteryKnown
             << value.batteryPercent << value.trusted;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value)
{
    quint32 deviceClass = 0;
    quint32 role = 0;
    argument.beginStructure();
    argument >> value.handle >> value.adapterHandle >> value.address >> value.name
        >> deviceClass >> role >> value.paired >> value.connected >> value.rssiKnown
        >> value.rssi >> value.batteryKnown >> value.batteryPercent;
    argument >> value.trusted;
    argument.endStructure();
    value.deviceClass = static_cast<DeviceClass>(deviceClass);
    value.role = static_cast<DeviceRole>(role);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Bluetooth1Device &value)
{
    argument.beginStructure();
    argument << value.handle << value.adapterHandle << value.address << value.name
             << static_cast<quint32>(value.deviceClass)
             << static_cast<quint32>(value.role) << value.paired << value.connected
             << value.rssiKnown << value.rssi << value.batteryKnown
             << value.batteryPercent;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Bluetooth1Device &value)
{
    quint32 deviceClass = 0;
    quint32 role = 0;
    argument.beginStructure();
    argument >> value.handle >> value.adapterHandle >> value.address >> value.name
        >> deviceClass >> role >> value.paired >> value.connected >> value.rssiKnown
        >> value.rssi >> value.batteryKnown >> value.batteryPercent;
    argument.endStructure();
    value.deviceClass = static_cast<DeviceClass>(deviceClass);
    value.role = static_cast<DeviceRole>(role);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PairingPrompt &value)
{
    argument.beginStructure();
    argument << value.promptId << static_cast<quint32>(value.kind) << value.device
             << value.detail << value.serviceUuid << value.entered;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PairingPrompt &value)
{
    quint32 kind = 0;
    argument.beginStructure();
    argument >> value.promptId >> kind >> value.device >> value.detail
        >> value.serviceUuid >> value.entered;
    argument.endStructure();
    value.kind = static_cast<PairingPromptKind>(kind);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Bluetooth1Snapshot &value)
{
    argument.beginStructure();
    argument << value.schemaVersion << value.epoch << value.revision
             << static_cast<quint32>(value.availability)
             << static_cast<quint32>(value.capabilities.toInt()) << value.reasonCode
             << value.diagnostic;
    writeArray(argument, value.adapters);
    writeArray(argument, value.devices);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Bluetooth1Snapshot &value)
{
    quint32 availability = 0;
    quint32 capabilities = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.schemaVersion >> value.epoch >> value.revision >> availability
        >> capabilities >> value.reasonCode >> value.diagnostic;
    readBoundedArray(argument, value.adapters, kMaxAdapters, value.wireValid);
    readBoundedArray(argument, value.devices, kMaxDevices, value.wireValid);
    argument.endStructure();
    value.availability = static_cast<Availability>(availability);
    value.capabilities = Capabilities::fromInt(capabilities);
    return argument;
}

Bluetooth1Snapshot bluetooth1Projection(const Snapshot &value)
{
    Bluetooth1Snapshot result;
    result.schemaVersion = kBluetooth1SchemaVersion;
    result.epoch = value.epoch;
    result.revision = value.revision;
    result.availability = value.availability;
    result.capabilities = Capabilities::fromInt(
        value.capabilities.toInt()
        & (static_cast<quint32>(Capability::SetAdapterPower)
           | static_cast<quint32>(Capability::DiscoveryLease)
           | static_cast<quint32>(Capability::ConnectPaired)
           | static_cast<quint32>(Capability::DisconnectPaired)));
    result.reasonCode = value.reasonCode;
    result.diagnostic = value.diagnostic;
    result.adapters = value.adapters;
    result.wireValid = value.wireValid;
    result.devices.reserve(value.devices.size());
    for (const Device &device : value.devices) {
        result.devices.push_back({.handle = device.handle,
                                  .adapterHandle = device.adapterHandle,
                                  .address = device.address,
                                  .name = device.name,
                                  .deviceClass = device.deviceClass,
                                  .role = device.role,
                                  .paired = device.paired,
                                  .connected = device.connected,
                                  .rssiKnown = device.rssiKnown,
                                  .rssi = device.rssi,
                                  .batteryKnown = device.batteryKnown,
                                  .batteryPercent = device.batteryPercent});
    }
    return result;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value)
{
    argument.beginStructure();
    argument << value.schemaVersion << value.epoch << value.revision
             << static_cast<quint32>(value.availability)
             << static_cast<quint32>(value.capabilities.toInt()) << value.reasonCode
             << value.diagnostic;
    writeArray(argument, value.adapters);
    writeArray(argument, value.devices);
    argument << value.pairingPrompt;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value)
{
    quint32 availability = 0;
    quint32 capabilities = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.schemaVersion >> value.epoch >> value.revision >> availability
        >> capabilities >> value.reasonCode >> value.diagnostic;
    readBoundedArray(argument, value.adapters, kMaxAdapters, value.wireValid);
    readBoundedArray(argument, value.devices, kMaxDevices, value.wireValid);
    argument >> value.pairingPrompt;
    argument.endStructure();
    value.availability = static_cast<Availability>(availability);
    value.capabilities = Capabilities::fromInt(capabilities);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value)
{
    argument.beginStructure();
    argument << static_cast<quint32>(value.kind) << static_cast<quint32>(value.status)
             << value.initiatingEpoch << value.initiatingRevision << value.observedEpoch
             << value.observedRevision << value.reasonCode << value.diagnostic;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value)
{
    quint32 kind = 0;
    quint32 status = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> kind >> status >> value.initiatingEpoch >> value.initiatingRevision
        >> value.observedEpoch >> value.observedRevision >> value.reasonCode
        >> value.diagnostic;
    argument.endStructure();
    value.kind = static_cast<OperationKind>(kind);
    value.status = static_cast<OperationStatus>(status);
    return argument;
}

} // namespace QindaQt::Bluetooth
