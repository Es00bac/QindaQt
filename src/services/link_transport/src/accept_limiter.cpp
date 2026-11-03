// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/accept_limiter.h>

namespace QindaQt::Link {

AcceptLimiter::AcceptLimiter(int perSource, int global, int windowMs, Clock clock)
    : m_perSource(perSource), m_global(global), m_windowMs(windowMs), m_clock(std::move(clock))
{
}

void AcceptLimiter::prune(qint64 now)
{
    const qint64 cutoff = now - m_windowMs;
    while (!m_all.isEmpty() && m_all.first() <= cutoff) m_all.removeFirst();
    for (auto it = m_bySource.begin(); it != m_bySource.end();) {
        while (!it->isEmpty() && it->first() <= cutoff) it->removeFirst();
        it = it->isEmpty() ? m_bySource.erase(it) : std::next(it);
    }
}

bool AcceptLimiter::allow(const QHostAddress &source)
{
    const qint64 now = m_clock();
    prune(now);
    QList<qint64> &mine = m_bySource[source.toString()];
    if (mine.size() >= m_perSource || m_all.size() >= m_global) {
        if (mine.isEmpty()) m_bySource.remove(source.toString());
        return false;
    }
    mine.append(now);
    m_all.append(now);
    return true;
}

} // namespace QindaQt::Link
