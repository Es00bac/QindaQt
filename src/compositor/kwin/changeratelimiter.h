// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Pure leading-and-trailing rate limiter keyed by an opaque string.
//
// A change for a quiet key is delivered at once (leading edge). Further
// changes inside the interval fold into one trailing delivery due exactly one
// interval after the previous delivery. A key is therefore delivered at most
// once per interval, and the last change of a burst is never lost.
//
// AGENT-NOTE: time is an explicit argument so this policy is deterministic and
// testable without a clock or timer. CoalescedChangeNotifier adapts it to Qt.
// See ADR-0383 for the 4 Hz budget the compositor uses it for.
class ChangeRateLimiter final
{
public:
    explicit ChangeRateLimiter(qint64 intervalMilliseconds);

    // Records a change at `now`. True means deliver it now: the key's previous
    // delivery is at least one interval old and nothing is pending. False
    // means it joined the key's pending trailing delivery.
    [[nodiscard]] bool admit(const QString &key, qint64 nowMilliseconds);
    // Keys whose trailing delivery is due at `now`, sorted; each is marked
    // delivered at `now`. Quiet keys older than one interval are pruned here.
    [[nodiscard]] QStringList takeDue(qint64 nowMilliseconds);
    // Earliest pending deadline, or nullopt when nothing is pending.
    [[nodiscard]] std::optional<qint64> nextDeadline() const;
    // Drops a key and any pending delivery (for example, its window closed).
    void forget(const QString &key);
    [[nodiscard]] bool isPending(const QString &key) const;
    [[nodiscard]] qsizetype trackedKeyCount() const noexcept { return m_entries.size(); }
    [[nodiscard]] qint64 intervalMilliseconds() const noexcept { return m_interval; }

private:
    struct Entry final
    {
        qint64 lastDelivery = 0;
        bool pending = false;
    };

    QHash<QString, Entry> m_entries;
    qint64 m_interval;
};

} // namespace QindaQt::Compositor::KWinIntegration
