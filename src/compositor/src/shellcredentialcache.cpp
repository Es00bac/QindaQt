// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shellcredentialcache.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor {

ShellCredentialCache::ShellCredentialCache(Lookup lookup, WatchChange watchChange,
                                           qsizetype capacity)
    : m_lookup(std::move(lookup))
    , m_watchChange(std::move(watchChange))
    , m_capacity(std::max<qsizetype>(1, capacity))
{
}

std::optional<qint64> ShellCredentialCache::processIdForUniqueName(
    const QString &uniqueName) const
{
    if (uniqueName.isEmpty() || !m_lookup) {
        return std::nullopt;
    }
    const bool unique = uniqueName.startsWith(u':');
    if (unique) {
        const auto found = m_processIds.constFind(uniqueName);
        if (found != m_processIds.cend()) {
            return *found;
        }
    }
    ++m_lookups;
    const auto processId = m_lookup(uniqueName);
    if (!unique || !processId) {
        return processId;
    }
    if (m_processIds.size() >= m_capacity && !m_insertionOrder.isEmpty()) {
        // Peers that authenticate are few (the shell, a probe, a test). An
        // overflow means churn, and evicting the oldest costs it one lookup.
        const QString evicted = m_insertionOrder.takeFirst();
        m_processIds.remove(evicted);
        if (m_watchChange) {
            m_watchChange(evicted, false);
        }
    }
    m_processIds.insert(uniqueName, *processId);
    m_insertionOrder.append(uniqueName);
    if (m_watchChange) {
        m_watchChange(uniqueName, true);
    }
    return processId;
}

void ShellCredentialCache::forget(const QString &uniqueName)
{
    if (m_processIds.remove(uniqueName) == 0) {
        return;
    }
    m_insertionOrder.removeAll(uniqueName);
    if (m_watchChange) {
        m_watchChange(uniqueName, false);
    }
}

} // namespace QindaQt::Compositor
