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

void readChannelVolumes(const QDBusArgument &argument, QVector<double> &values,
                        bool &wireValid)
{
    readBoundedArray<double>(argument, values, kMaxChannelsPerDevice, wireValid);
}

void writeChannelVolumes(QDBusArgument &argument, const QVector<double> &values)
{
    writeArray<double>(argument, values);
}

void readChannelMap(const QDBusArgument &argument, QStringList &labels,
                    bool &wireValid)
{
    labels.clear();
    argument.beginArray();
    while (!argument.atEnd()) {
        QString label;
        argument >> label;
        if (labels.size() >= kMaxChannelsPerDevice) {
            wireValid = false;
            continue;
        }
        if (label.toUtf8().size() > kMaxChannelNameUtf8Bytes) {
            // Retain at most the bound; the invalid marker keeps the whole
            // snapshot fail-closed even though the array was fully consumed.
            QByteArray bytes = label.toUtf8();
            bytes.truncate(kMaxChannelNameUtf8Bytes);
            while (!QString::fromUtf8(bytes).toUtf8().startsWith(bytes) && !bytes.isEmpty()) {
                bytes.chop(1);
            }
            label = QString::fromUtf8(bytes);
            wireValid = false;
        }
        labels.push_back(std::move(label));
    }
    argument.endArray();
}

void writeChannelMap(QDBusArgument &argument, const QStringList &labels)
{
    argument.beginArray(QMetaType::fromType<QString>());
    for (const QString &label : labels) {
        argument << label;
    }
    argument.endArray();
}

} // namespace

void registerDBusTypes()
{
    qRegisterMetaType<Handle>();
    qRegisterMetaType<Device>();
    qRegisterMetaType<Stream>();
    qRegisterMetaType<Level>();
    qRegisterMetaType<MatrixSend>();
    qRegisterMetaType<Strip>();
    qRegisterMetaType<Bus>();
    qRegisterMetaType<Console>();
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<Handle>();
    qDBusRegisterMetaType<Device>();
    qDBusRegisterMetaType<Stream>();
    // AGENT-GUARD: the console's members must be registered BEFORE Console and
    // Snapshot. qDBusRegisterMetaType builds a signature by marshalling a
    // default value, so an unregistered nested type would be baked into the
    // parent's signature as a variant and every console would fail to decode.
    qDBusRegisterMetaType<Level>();
    qDBusRegisterMetaType<MatrixSend>();
    qDBusRegisterMetaType<Strip>();
    qDBusRegisterMetaType<Bus>();
    qDBusRegisterMetaType<Console>();
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
    writeChannelVolumes(argument, value.channelVolumes);
    writeChannelMap(argument, value.channelMap);
    argument << value.virtualDevice;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Device &value)
{
    quint32 kind = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.handle >> kind >> value.name >> value.description >> value.volume
        >> value.volumeKnown >> value.muted >> value.muteKnown >> value.isDefault
        >> value.canSetVolume >> value.canSetMute;
    readChannelVolumes(argument, value.channelVolumes, value.wireValid);
    readChannelMap(argument, value.channelMap, value.wireValid);
    argument >> value.virtualDevice;
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
    writeChannelVolumes(argument, value.channelVolumes);
    writeChannelMap(argument, value.channelMap);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Stream &value)
{
    quint32 direction = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.handle >> direction >> value.applicationName >> value.mediaName
        >> value.target >> value.targetKnown >> value.volume >> value.volumeKnown
        >> value.muted >> value.muteKnown >> value.canSetVolume >> value.canSetMute
        >> value.canMove;
    readChannelVolumes(argument, value.channelVolumes, value.wireValid);
    readChannelMap(argument, value.channelMap, value.wireValid);
    argument.endStructure();
    value.direction = static_cast<StreamDirection>(direction);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Level &value)
{
    argument.beginStructure();
    argument << value.peakDb << value.rmsDb << value.known;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Level &value)
{
    argument.beginStructure();
    argument >> value.peakDb >> value.rmsDb >> value.known;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const MatrixSend &value)
{
    argument.beginStructure();
    argument << value.busIndex << value.enabled << value.gainDb;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MatrixSend &value)
{
    argument.beginStructure();
    argument >> value.busIndex >> value.enabled >> value.gainDb;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Strip &value)
{
    argument.beginStructure();
    argument << value.id << static_cast<quint32>(value.kind) << value.index
             << value.label << value.sourceEpoch << value.sourceSerial
             << value.sourceKnown << value.gainDb << value.muted << value.soloed
             << value.mono << value.pan;
    writeChannelVolumes(argument, value.channelTrimDb);
    writeArray(argument, value.sends);
    argument << value.level;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Strip &value)
{
    quint32 kind = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.id >> kind >> value.index >> value.label >> value.sourceEpoch
        >> value.sourceSerial >> value.sourceKnown >> value.gainDb >> value.muted
        >> value.soloed >> value.mono >> value.pan;
    readChannelVolumes(argument, value.channelTrimDb, value.wireValid);
    readBoundedArray(argument, value.sends, kMaxSendsPerStrip, value.wireValid);
    argument >> value.level;
    argument.endStructure();
    value.kind = static_cast<StripKind>(kind);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Bus &value)
{
    argument.beginStructure();
    argument << value.id << static_cast<quint32>(value.kind) << value.index
             << value.label << value.targetEpoch << value.targetSerial
             << value.targetKnown << value.gainDb << value.muted << value.mono
             << value.level;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Bus &value)
{
    quint32 kind = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.id >> kind >> value.index >> value.label >> value.targetEpoch
        >> value.targetSerial >> value.targetKnown >> value.gainDb >> value.muted
        >> value.mono >> value.level;
    argument.endStructure();
    value.kind = static_cast<BusKind>(kind);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Console &value)
{
    argument.beginStructure();
    writeArray(argument, value.strips);
    writeArray(argument, value.buses);
    argument << value.soloActive;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Console &value)
{
    value.wireValid = true;
    argument.beginStructure();
    readBoundedArray(argument, value.strips, kMaxStrips, value.wireValid);
    readBoundedArray(argument, value.buses, kMaxBuses, value.wireValid);
    argument >> value.soloActive;
    argument.endStructure();
    // AGENT-GUARD: a member that overflowed its own bound must invalidate the
    // whole console, or a client would publish a strip whose routing silently
    // lost sends.
    for (const Strip &strip : value.strips) {
        value.wireValid = value.wireValid && strip.wireValid;
    }
    for (const Bus &bus : value.buses) {
        value.wireValid = value.wireValid && bus.wireValid;
    }
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
    argument << value.console;
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
    for (const Device &device : value.outputs) {
        value.wireValid = value.wireValid && device.wireValid;
    }
    for (const Device &device : value.inputs) {
        value.wireValid = value.wireValid && device.wireValid;
    }
    for (const Stream &stream : value.streams) {
        value.wireValid = value.wireValid && stream.wireValid;
    }
    argument >> value.console;
    value.wireValid = value.wireValid && value.console.wireValid;
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
