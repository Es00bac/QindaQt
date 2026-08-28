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
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<Handle>();
    qDBusRegisterMetaType<Adapter>();
    qDBusRegisterMetaType<Device>();
    qDBusRegisterMetaType<Snapshot>();
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
    argument << value.handle << value.address << value.name
             << static_cast<quint32>(value.state) << value.discoveringActive
             << static_cast<quint32>(value.capabilities.toInt());
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Adapter &value)
{
    quint32 state = 0;
    quint32 capabilities = 0;
    argument.beginStructure();
    argument >> value.handle >> value.address >> value.name >> state
        >> value.discoveringActive >> capabilities;
    argument.endStructure();
    value.state = static_cast<AdapterState>(state);
    value.capabilities = AdapterCapabilities::fromInt(capabilities);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Device &value)
{
    argument.beginStructure();
    argument << value.handle << value.adapterHandle << value.address << value.name
             << static_cast<quint32>(value.state) << value.rssi << value.rssiKnown
             << value.paired << value.trusted
             << static_cast<quint32>(value.capabilities.toInt());
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value)
{
    quint32 state = 0;
    quint32 capabilities = 0;
    argument.beginStructure();
    argument >> value.handle >> value.adapterHandle >> value.address >> value.name
        >> state >> value.rssi >> value.rssiKnown >> value.paired >> value.trusted
        >> capabilities;
    argument.endStructure();
    value.state = static_cast<DeviceState>(state);
    value.capabilities = DeviceCapabilities::fromInt(capabilities);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value)
{
    argument.beginStructure();
    argument << value.schemaVersion << value.epoch << value.revision
             << value.reasonCode << value.diagnostic;
    writeArray(argument, value.adapters);
    writeArray(argument, value.devices);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value)
{
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.schemaVersion >> value.epoch >> value.revision
        >> value.reasonCode >> value.diagnostic;
    readBoundedArray(argument, value.adapters, kMaxAdapters, value.wireValid);
    readBoundedArray(argument, value.devices, kMaxDevices, value.wireValid);
    argument.endStructure();
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
