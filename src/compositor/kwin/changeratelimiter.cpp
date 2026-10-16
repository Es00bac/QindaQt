// SPDX-License-Identifier: GPL-3.0-or-later
#include "changeratelimiter.h"

#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {

ChangeRateLimiter::ChangeRateLimiter(qint64 intervalMilliseconds)
    : m_interval(std::max<qint64>(0, intervalMilliseconds))
{
}

bool ChangeRateLimiter::admit(const QString &key, qint64 nowMilliseconds)
{
    const auto found = m_entries.find(key);
    if (found == m_entries.end()) {
        m_entries.insert(key, {nowMilliseconds, false});
        return true;
    }
    if (found->pending) {
        return false;
    }
    // AGENT-GUARD: a clock that steps backwards (or a key re-admitted with an
    // older timestamp) must not wedge the key behind a deadline in the future.
    if (nowMilliseconds < found->lastDelivery
        || nowMilliseconds - found->lastDelivery >= m_interval) {
        found->lastDelivery = nowMilliseconds;
        return true;
    }
    found->pending = true;
    return false;
}

QStringList ChangeRateLimiter::takeDue(qint64 nowMilliseconds)
{
    QStringList due;
    for (auto iterator = m_entries.begin(); iterator != m_entries.end();) {
        auto &entry = iterator.value();
        if (entry.pending && entry.lastDelivery + m_interval <= nowMilliseconds) {
            entry.pending = false;
            entry.lastDelivery = nowMilliseconds;
            due.append(iterator.key());
            ++iterator;
        } else if (!entry.pending && nowMilliseconds - entry.lastDelivery >= m_interval) {
            // A quiet key past its interval behaves exactly like an unknown
            // key; dropping it keeps per-window state bounded.
            iterator = m_entries.erase(iterator);
        } else {
            ++iterator;
        }
    }
    due.sort();
    return due;
}

std::optional<qint64> ChangeRateLimiter::nextDeadline() const
{
    std::optional<qint64> earliest;
    for (const auto &entry : m_entries) {
        if (!entry.pending) {
            continue;
        }
        const qint64 deadline = entry.lastDelivery + m_interval;
        if (!earliest || deadline < *earliest) {
            earliest = deadline;
        }
    }
    return earliest;
}

void ChangeRateLimiter::forget(const QString &key)
{
    m_entries.remove(key);
}

bool ChangeRateLimiter::isPending(const QString &key) const
{
    const auto found = m_entries.constFind(key);
    return found != m_entries.cend() && found->pending;
}

} // namespace QindaQt::Compositor::KWinIntegration
