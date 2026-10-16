// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "changeratelimiter.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Qt adapter for ChangeRateLimiter: one precise single-shot timer armed at the
// next delivery. Calls and the delivery callback run on this object's thread;
// destroying the notifier cancels every pending delivery.
//
// AGENT-GUARD: a leading-edge change is delivered at the end of the current
// event-loop turn, never synchronously inside notify(). KWin emits a window's
// signals in bursts; a synchronous delivery would let consumers sample a
// half-updated window, which is what the old zero-delay coalescing avoided.
class CoalescedChangeNotifier final : public QObject
{
public:
    using Clock = std::function<qint64()>;
    using Delivery = std::function<void(const QStringList &keys)>;

    // An empty clock uses a monotonic millisecond clock. The delivery callback
    // is borrowed for this object's lifetime and must not destroy it.
    CoalescedChangeNotifier(qint64 intervalMilliseconds, Delivery delivery,
                            Clock clock = {}, QObject *parent = nullptr);

    void notify(const QString &key = {});
    void forget(const QString &key);
    // Delivers the leading-edge keys and every trailing delivery that is due.
    // The timer calls this; deterministic tests call it after moving their
    // injected clock instead of waiting for the timer.
    void deliverDue();
    [[nodiscard]] std::optional<qint64> nextDeadline() const;
    [[nodiscard]] bool hasPendingDelivery() const;
    [[nodiscard]] quint64 deliveryCount() const noexcept { return m_deliveries; }

private:
    void arm();
    [[nodiscard]] qint64 now() const;

    ChangeRateLimiter m_limiter;
    Delivery m_delivery;
    Clock m_clock;
    QSet<QString> m_immediate;
    QTimer m_timer;
    quint64 m_deliveries = 0;
    bool m_delivering = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
