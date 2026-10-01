// SPDX-License-Identifier: LGPL-3.0-or-later
#include "packet.h"
#include <QtEndian>
#include <QList>
namespace QindaQt::Services::Portal::CaptureAuthority {
namespace {
template<class T> T read(const QByteArray &bytes, qsizetype offset) { return qFromLittleEndian<T>(bytes.constData() + offset); }
template<class T> void put(QByteArray &bytes, qsizetype offset, T value) { qToLittleEndian<T>(value, bytes.data() + offset); }
bool payloadValid(const Packet &packet) {
    if (!packet.generation || packet.payload.size() > Wire::MaxPayloadBytes) return false;
    if (packet.descriptors != (packet.message == Wire::Message::JobStarted ? 2 : 0)) return false;
    switch (packet.message) {
    case Wire::Message::Hello: return packet.payload.isEmpty();
    case Wire::Message::Ready: {
        if (packet.payload.size() < 3) return false;
        const auto size = read<quint16>(packet.payload, 0);
        return size == packet.payload.size() - 2 && validUniqueName(packet.payload.mid(2));
    }
    case Wire::Message::StartJob: {
        if (!packet.job || packet.payload.size() < 10) return false;
        const auto scope = read<quint16>(packet.payload, 0), first = read<quint16>(packet.payload, 2), second = read<quint16>(packet.payload, 4);
        return (scope == static_cast<quint16>(Wire::Scope::Screenshot) || scope == static_cast<quint16>(Wire::Scope::ScreenCast))
            && !read<quint16>(packet.payload, 6) && packet.payload.size() == 8 + first + second
            && validUniqueName(packet.payload.mid(8, first)) && validUniqueName(packet.payload.mid(8 + first, second));
    }
    case Wire::Message::Error: {
        if (packet.payload.size() != 2) return false;
        const auto code = read<quint16>(packet.payload, 0);
        return code >= static_cast<quint16>(Wire::Error::Invalid) && code <= static_cast<quint16>(Wire::Error::Transport);
    }
    case Wire::Message::JobStarted:
    case Wire::Message::ParentReady:
    case Wire::Message::ConsentGranted:
    case Wire::Message::CaptureReady:
    case Wire::Message::RevokeJob:
    case Wire::Message::JobRevoked: return packet.job && packet.payload.isEmpty();
    }
    return false;
}
}
bool validUniqueName(const QByteArray &name) {
    if (name.size() < 2 || name.size() > 255 || name.front() != ':' || name.contains('\0')) return false;
    // Bus-daemon UID/PID checks authenticate the name. This only bounds syntax;
    // a unique name consists of at least two nonempty ASCII components.
    const auto parts = name.mid(1).split('.');
    if (parts.size() < 2) return false;
    for (const auto &part : parts) {
        if (part.isEmpty()) return false;
        for (const char c : part) if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}
QByteArray encode(const Packet &packet) {
    if (!payloadValid(packet)) return {};
    QByteArray bytes(Wire::HeaderBytes, '\0');
    put(bytes, 0, Wire::Magic); put(bytes, 4, Wire::Version); put(bytes, 6, static_cast<quint16>(packet.message));
    put(bytes, 8, packet.generation); put(bytes, 16, packet.job); put(bytes, 24, static_cast<quint32>(packet.payload.size())); put(bytes, 28, packet.descriptors);
    return bytes + packet.payload;
}
std::optional<Packet> decode(const QByteArray &bytes) {
    if (bytes.size() < Wire::HeaderBytes || bytes.size() > Wire::HeaderBytes + Wire::MaxPayloadBytes
        || read<quint32>(bytes, 0) != Wire::Magic || read<quint16>(bytes, 4) != Wire::Version || read<quint16>(bytes, 30)) return {};
    const auto size = read<quint32>(bytes, 24);
    if (size != bytes.size() - Wire::HeaderBytes) return {};
    Packet packet{static_cast<Wire::Message>(read<quint16>(bytes, 6)), read<quint64>(bytes, 8), read<quint64>(bytes, 16), bytes.mid(Wire::HeaderBytes), read<quint16>(bytes, 28)};
    return payloadValid(packet) ? std::optional(packet) : std::nullopt;
}
QByteArray readyPayload(const QString &name) {
    const auto utf8 = name.toUtf8(); if (!validUniqueName(utf8)) return {};
    QByteArray payload(2, '\0'); put(payload, 0, static_cast<quint16>(utf8.size())); return payload + utf8;
}
QByteArray startPayload(Wire::Scope scope, const QString &frontend, const QString &caller) {
    const auto first = frontend.toUtf8(), second = caller.toUtf8();
    if (!validUniqueName(first) || !validUniqueName(second) || (scope != Wire::Scope::Screenshot && scope != Wire::Scope::ScreenCast)) return {};
    QByteArray payload(8, '\0'); put(payload, 0, static_cast<quint16>(scope)); put(payload, 2, static_cast<quint16>(first.size())); put(payload, 4, static_cast<quint16>(second.size())); return payload + first + second;
}
}
