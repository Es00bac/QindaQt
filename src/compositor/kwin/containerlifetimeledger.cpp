// SPDX-License-Identifier: GPL-3.0-or-later
#include "containerlifetimeledger.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

ContainerLifetimeLedger::ContainerLifetimeLedger(Forget forget)
    : m_forget(std::move(forget))
{
}

QStringList ContainerLifetimeLedger::reconcile(const QStringList &current)
{
    const QSet<QString> next(current.cbegin(), current.cend());
    QStringList vanished;
    for (const auto &containerId : std::as_const(m_published)) {
        if (!next.contains(containerId)) {
            vanished.append(containerId);
        }
    }
    vanished.sort();
    // AGENT-GUARD: publish the new set before running callbacks; a forget
    // callback may re-enter the session and must not see the stale set.
    m_published = next;
    if (m_forget) {
        for (const auto &containerId : std::as_const(vanished)) {
            m_forget(containerId);
        }
    }
    return vanished;
}

void ContainerLifetimeLedger::forget(const QString &containerId)
{
    m_published.remove(containerId);
    if (m_forget) {
        m_forget(containerId);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
