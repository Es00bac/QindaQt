// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_dbus.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Audio
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
    qRegisterMetaType<Device>();
    qRegisterMetaType<Stream>();
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<Handle>();
    qDBusRegisterMetaType<Device>();
    qDBusRegisterMetaType<Stream>();
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

QDBusArgument &operator<<(QDBusArgument &argument, const Device &value)
{
    argument.beginStructure();
    argument << value.handle << static_cast<quint32>(value.kind) << value.name
             << value.description << value.volume << value.volumeKnown << value.muted
             << value.muteKnown << value.isDefault << value.canSetVolume
             << value.canSetMute;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value)
{
    quint32 kind = 0;
    argument.beginStructure();
    argument >> value.handle >> kind >> value.name >> value.description >> value.volume
        >> value.volumeKnown >> value.muted >> value.muteKnown >> value.isDefault
        >> value.canSetVolume >> value.canSetMute;
    argument.endStructure();
    value.kind = static_cast<DeviceKind>(kind);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Stream &value)
{
    argument.beginStructure();
    argument << value.handle << static_cast<quint32>(value.direction)
             << value.applicationName << value.mediaName << value.target
             << value.targetKnown << value.volume << value.volumeKnown << value.muted
             << value.muteKnown << value.canSetVolume << value.canSetMute << value.canMove;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Stream &value)
{
    quint32 direction = 0;
    argument.beginStructure();
    argument >> value.handle >> direction >> value.applicationName >> value.mediaName
        >> value.target >> value.targetKnown >> value.volume >> value.volumeKnown
        >> value.muted >> value.muteKnown >> value.canSetVolume >> value.canSetMute
        >> value.canMove;
    argument.endStructure();
    value.direction = static_cast<StreamDirection>(direction);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value)
{
    argument.beginStructure();
    argument << value.schemaVersion << value.epoch << value.revision
             << static_cast<quint32>(value.availability)
             << static_cast<quint32>(value.capabilities.toInt()) << value.reasonCode
             << value.diagnostic << value.defaultOutput << value.defaultInput;
    writeArray(argument, value.outputs);
    writeArray(argument, value.inputs);
    writeArray(argument, value.streams);
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
        >> capabilities >> value.reasonCode >> value.diagnostic >> value.defaultOutput
        >> value.defaultInput;
    readBoundedArray(argument, value.outputs, kMaxOutputs, value.wireValid);
    readBoundedArray(argument, value.inputs, kMaxInputs, value.wireValid);
    readBoundedArray(argument, value.streams, kMaxStreams, value.wireValid);
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

} // namespace QindaQt::Audio
