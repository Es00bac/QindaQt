// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt-kwin/capture-authority/protocol.h>
#include <QByteArray>
#include <QString>
#include <optional>
namespace QindaQt::Services::Portal::CaptureAuthority {
namespace Wire = QindaQt::CaptureAuthority::Wire;
static_assert(Wire::Version == 1 && Wire::HeaderBytes == 32 && Wire::MaxPayloadBytes == 16384);
// Pure bounded framing; descriptors/credentials are owned by the transport.
// No source-tree fork include or caller payload can confer authority.
struct Packet {
    Wire::Message message;
    quint64 generation = 0, job = 0;
    QByteArray payload;
    quint16 descriptors = 0;
};
QByteArray encode(const Packet &);
std::optional<Packet> decode(const QByteArray &);
QByteArray readyPayload(const QString &uniqueName);
QByteArray startPayload(Wire::Scope, const QString &frontend, const QString &caller);
bool validUniqueName(const QByteArray &);
}
