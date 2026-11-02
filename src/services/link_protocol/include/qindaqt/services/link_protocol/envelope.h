// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <cstdint>

namespace QindaQt::Link {

// One QindaLink frame envelope (docs/wiki/reference/qindalink-protocol-v1.md).
// Value type; no ownership or threading constraints.
//
// AGENT-CONTRACT: `id` and `re` use 0 for "absent" because the wire range is
// 1..2^53-1. An empty `b` is encoded as an absent body.
struct Envelope {
    QString t;
    qint64 id = 0;
    qint64 re = 0;
    QJsonObject b;

    [[nodiscard]] bool operator==(const Envelope &other) const = default;
};

inline constexpr qint64 kMaxEnvelopeId = (qint64{1} << 53) - 1;
inline constexpr qint64 kMaxPayloadBytes = 4 * 1024 * 1024;

// True when `t` matches ^[a-z][a-z0-9]*(\.[a-z][a-z0-9-]*)*$ and is 1..64 chars.
[[nodiscard]] bool isValidFrameType(const QString &t);

// Convenience constructors for the replies the state machine sends often.
[[nodiscard]] Envelope makeError(const QString &code, const QString &message, qint64 re = 0);
[[nodiscard]] Envelope makeBye(const QString &reason);

} // namespace QindaQt::Link
