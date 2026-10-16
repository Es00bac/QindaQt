// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace QindaQt::Compositor {

// Bounded per-unique-name memo of a session-bus peer's process id (ADR-0383).
//
// A bus daemon never reuses a unique name, and a connection's PID is fixed for
// the connection's lifetime, so a cached entry stays true until its owner
// disconnects. The transport adapter watches every cached name and calls
// forget() when NameOwnerChanged reports the owner gone. Only the first
// request from a connection pays for a GetConnectionCredentials round trip;
// every later authorization (each task-list read, each targeted change hint,
// each window action) is a hash lookup.
//
// Failed lookups are not cached: a transient bus failure must not deny a
// peer forever. Well-known (non-colon) names are never cached either.
//
// Threading: confined to the owner's thread, like every credential source.
class ShellCredentialCache final : public ShellWindowCredentialSource
{
public:
    using Lookup = std::function<std::optional<qint64>(const QString &uniqueName)>;
    // Told when a name enters (true) or leaves (false) the cache, so the
    // adapter can add or drop its NameOwnerChanged watch.
    using WatchChange = std::function<void(const QString &uniqueName, bool watched)>;

    explicit ShellCredentialCache(Lookup lookup, WatchChange watchChange = {},
                                  qsizetype capacity = 16);

    [[nodiscard]] std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const override;
    // The owner of `uniqueName` disconnected.
    void forget(const QString &uniqueName);

    [[nodiscard]] qsizetype lookupCount() const noexcept { return m_lookups; }
    [[nodiscard]] qsizetype size() const noexcept { return m_processIds.size(); }

private:
    Lookup m_lookup;
    WatchChange m_watchChange;
    qsizetype m_capacity;
    // processIdForUniqueName() is const in the source interface; the memo is
    // an implementation detail of answering it.
    mutable QHash<QString, qint64> m_processIds;
    mutable QStringList m_insertionOrder;
    mutable qsizetype m_lookups = 0;
};

} // namespace QindaQt::Compositor
