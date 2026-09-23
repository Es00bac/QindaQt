// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

namespace QindaQt::Audio
{

namespace {

bool plainVbanName(const QString &name)
{
    if (name.isEmpty() || name.toUtf8().size() > kMaxVbanNameUtf8Bytes) return false;
    for (const QChar character : name) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7e
            || character == QLatin1Char('"') || character == QLatin1Char('\\'))
            return false;
    }
    return true;
}

bool plainVbanHost(const QString &host)
{
    if (host.isEmpty() || host.toUtf8().size() > kMaxVbanHostUtf8Bytes) return false;
    for (const QChar character : host) {
        if (!(character.isLetterOrNumber() || character == QLatin1Char('.')
              || character == QLatin1Char('-'))) return false;
    }
    return true;
}

bool exactIpv4(const QString &host)
{
    const QStringList octets = host.split(QLatin1Char('.'));
    if (octets.size() != 4) return false;
    for (const QString &octet : octets) {
        bool ok = false;
        const int value = octet.toInt(&ok);
        if (!ok || value < 0 || value > 255 || QString::number(value) != octet) return false;
    }
    return host != QStringLiteral("0.0.0.0")
        && host != QStringLiteral("255.255.255.255");
}

bool plainOutputNode(const QString &name)
{
    if (name.isEmpty() || !isBoundedText(name, kMaxVbanOutputNodeUtf8Bytes)) return false;
    for (const QChar character : name) {
        const ushort code = character.unicode();
        if (code <= 0x1f || code == 0x7f || character == QLatin1Char('"')
            || character == QLatin1Char('\\')) return false;
    }
    return true;
}

} // namespace

bool isPhysicalPeerOutput(const Device &device)
{
    return device.kind == DeviceKind::Output && !device.virtualDevice
        && !device.nodeName.startsWith(QStringLiteral("qindaqt.console."))
        && !device.nodeName.startsWith(QLatin1String(kVirtualDeviceNamePrefix))
        && plainOutputNode(device.nodeName);
}

ValidationResult validateVbanDefinition(const VbanStream &stream)
{
    if (!plainVbanName(stream.name) || stream.port == 0 || stream.port > 65535)
        return {.accepted = false, .reasonCode = QStringLiteral("invalid-vban-stream")};
    if (stream.outgoing) {
        if (stream.busId.isEmpty()
            || !isBoundedText(stream.busId, kMaxConsoleIdUtf8Bytes)
            || !plainVbanHost(stream.host) || !stream.outputNodeName.isEmpty())
            return {.accepted = false, .reasonCode = QStringLiteral("invalid-vban-stream")};
    } else {
        // AGENT-GUARD: a receiver is never a wildcard LAN listener. Its
        // consent names one source IPv4 and one physical output node exactly.
        if (!stream.busId.isEmpty() || !exactIpv4(stream.host)
            || !plainOutputNode(stream.outputNodeName))
            return {.accepted = false, .reasonCode = QStringLiteral("invalid-vban-stream")};
    }
    return {.accepted = true, .reasonCode = {}};
}

} // namespace QindaQt::Audio
