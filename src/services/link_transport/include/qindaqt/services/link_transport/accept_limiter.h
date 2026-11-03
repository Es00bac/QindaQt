// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtNetwork/QHostAddress>

#include <functional>

namespace QindaQt::Link {

// Sliding-window accept limiter for LAN listeners: at most `perSource` accepts per source address and
// `global` in total within `windowMs`. It bounds how many TLS handshakes an unauthenticated host can
// start, so a flood cannot hold the event loop or the Hub's unauthenticated slots (ADR-0441).
//
// Ownership: owned by the Hub, shared by its LAN listeners. Threading: one thread. The clock is
// injectable for tests. IPv4-mapped IPv6 sources are folded onto their IPv4 form by the caller.
class AcceptLimiter {
public:
    using Clock = std::function<qint64()>;   // milliseconds, monotonic
    AcceptLimiter(int perSource, int global, int windowMs, Clock clock);
    // Records and returns true when the accept is within budget; false (not recorded) otherwise.
    [[nodiscard]] bool allow(const QHostAddress &source);

private:
    void prune(qint64 now);
    int m_perSource, m_global, m_windowMs;
    Clock m_clock;
    QHash<QString, QList<qint64>> m_bySource;
    QList<qint64> m_all;
};

} // namespace QindaQt::Link
