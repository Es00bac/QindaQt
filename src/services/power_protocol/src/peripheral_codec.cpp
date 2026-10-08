// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QStringDecoder>
namespace QindaQt::Power {
namespace {
constexpr quint32 kMagic = 0x51503150; // QP1P; never the existing QP1S snapshot.
void writeText(QDataStream &stream, const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    stream << quint32(bytes.size());
    stream.writeRawData(bytes.constData(), bytes.size());
}
bool readText(QDataStream &stream, QString &text, quint32 maximum)
{
    quint32 length = 0; stream >> length;
    if (stream.status() != QDataStream::Ok || length > maximum
        || qint64(length) > stream.device()->bytesAvailable()) return false;
    QByteArray bytes(qsizetype(length), Qt::Uninitialized);
    if (stream.readRawData(bytes.data(), qsizetype(length)) != qsizetype(length)) return false;
    QStringDecoder decoder(QStringDecoder::Utf8);
    text = decoder(bytes);
    return !decoder.hasError();
}
bool readFlag(QDataStream &stream, bool &value)
{
    quint8 raw = 0; stream >> raw; value = raw == 1;
    return stream.status() == QDataStream::Ok && raw <= 1;
}
}
bool encodePeripheralSnapshot(const PeripheralSnapshot &value, QByteArray &output)
{
    if (!validatePeripheralSnapshot(value)) return false;
    QByteArray bytes; QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kMagic << value.schemaVersion << value.epoch << value.revision
           << quint32(value.availability);
    writeText(stream, value.reasonCode);
    stream << value.omittedCount << quint8(value.truncated) << quint32(value.devices.size());
    for (const auto &device : value.devices) {
        stream << device.handle.epoch; writeText(stream, device.handle.opaqueId);
        stream << quint32(device.kind); writeText(stream, device.vendor); writeText(stream, device.model);
        stream << quint8(device.present) << quint8(device.percentageKnown) << device.percentage
               << quint32(device.level) << quint32(device.state)
               << quint8(device.timeToEmptyKnown) << device.timeToEmptySeconds
               << quint8(device.timeToFullKnown) << device.timeToFullSeconds;
    }
    if (stream.status() != QDataStream::Ok || bytes.size() > kMaxPeripheralPayloadBytes) return false;
    output = std::move(bytes); return true;
}
bool decodePeripheralSnapshot(const QByteArray &input, PeripheralSnapshot &output)
{
    if (input.size() > kMaxPeripheralPayloadBytes) return false;
    QDataStream stream(input); stream.setVersion(QDataStream::Qt_6_0);
    PeripheralSnapshot value; quint32 magic = 0, availability = 0, count = 0;
    stream >> magic >> value.schemaVersion >> value.epoch >> value.revision >> availability;
    if (magic != kMagic || !readText(stream, value.reasonCode, 64)) return false;
    value.availability = static_cast<Availability>(availability);
    stream >> value.omittedCount;
    if (!readFlag(stream, value.truncated)) return false;
    stream >> count;
    if (stream.status() != QDataStream::Ok || count > quint32(kMaxPeripheralBatteries)) return false;
    for (quint32 index = 0; index < count; ++index) {
        PeripheralBattery device; quint32 kind = 0, level = 0, state = 0;
        stream >> device.handle.epoch;
        if (!readText(stream, device.handle.opaqueId, 128)) return false;
        stream >> kind; device.kind = static_cast<PeripheralKind>(kind);
        if (!readText(stream, device.vendor, 256) || !readText(stream, device.model, 256)
            || !readFlag(stream, device.present) || !readFlag(stream, device.percentageKnown)) return false;
        stream >> device.percentage >> level >> state;
        device.level = static_cast<BatteryLevel>(level); device.state = static_cast<ChargeState>(state);
        if (!readFlag(stream, device.timeToEmptyKnown)) return false;
        stream >> device.timeToEmptySeconds;
        if (!readFlag(stream, device.timeToFullKnown)) return false;
        stream >> device.timeToFullSeconds;
        value.devices.append(std::move(device));
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd() || !validatePeripheralSnapshot(value)) return false;
    output = std::move(value); return true;
}
} // namespace QindaQt::Power
