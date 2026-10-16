// SPDX-License-Identifier: GPL-3.0-or-later
#include "coalescedchangenotifier.h"

#include <QScopedValueRollback>

#include <algorithm>
#include <chrono>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

qint64 monotonicMilliseconds()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

CoalescedChangeNotifier::CoalescedChangeNotifier(qint64 intervalMilliseconds,
                                                 Delivery delivery, Clock clock,
                                                 QObject *parent)
    : QObject(parent)
    , m_limiter(intervalMilliseconds)
    , m_delivery(std::move(delivery))
    , m_clock(clock ? std::move(clock) : Clock(monotonicMilliseconds))
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &CoalescedChangeNotifier::deliverDue);
}

qint64 CoalescedChangeNotifier::now() const
{
    return m_clock();
}

void CoalescedChangeNotifier::notify(const QString &key)
{
    if (m_limiter.admit(key, now())) {
        m_immediate.insert(key);
    }
    arm();
}

void CoalescedChangeNotifier::forget(const QString &key)
{
    m_limiter.forget(key);
    m_immediate.remove(key);
    arm();
}

void CoalescedChangeNotifier::deliverDue()
{
    // A delivery callback that notifies again re-arms through arm(); it must
    // not recurse into a second delivery of the same batch.
    if (m_delivering) {
        return;
    }
    QStringList keys = m_limiter.takeDue(now());
    for (const auto &key : std::as_const(m_immediate)) {
        if (!keys.contains(key)) {
            keys.append(key);
        }
    }
    m_immediate.clear();
    keys.sort();
    if (!keys.isEmpty() && m_delivery) {
        QScopedValueRollback<bool> delivering(m_delivering, true);
        ++m_deliveries;
        m_delivery(keys);
    }
    arm();
}

std::optional<qint64> CoalescedChangeNotifier::nextDeadline() const
{
    if (!m_immediate.isEmpty()) {
        return now();
    }
    return m_limiter.nextDeadline();
}

bool CoalescedChangeNotifier::hasPendingDelivery() const
{
    return !m_immediate.isEmpty() || m_limiter.nextDeadline().has_value();
}

void CoalescedChangeNotifier::arm()
{
    const auto deadline = nextDeadline();
    if (!deadline) {
        m_timer.stop();
        return;
    }
    const qint64 delay = std::max<qint64>(0, *deadline - now());
    // Restart only when the new deadline is earlier than the armed one, so a
    // steady stream of changes cannot keep pushing a trailing delivery out.
    if (!m_timer.isActive() || delay < m_timer.remainingTime()) {
        m_timer.start(static_cast<int>(std::min<qint64>(delay, 60'000)));
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
