// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

// ADR-0384: one post-publication diff that forgets every dissolved container.
//
// Containers dissolve along many paths (closing a member, a native title
// drag, the context-menu ungroup, a shared-chrome drag, keyboard detach and
// keyboard group/tile). Each path used to forget per-container state by hand
// and several did not, leaving a dissolved container's maximize record behind
// to fail "no valid maximize area" on every work-area change (350 times in one
// session). The session reconciles this ledger against every topology it
// publishes; any id that vanished is forgotten exactly once, whatever path
// removed it.
//
// Pure value object; the forget callback is borrowed and runs synchronously
// inside reconcile() and forget().
class ContainerLifetimeLedger final
{
public:
    using Forget = std::function<void(const QString &containerId)>;

    explicit ContainerLifetimeLedger(Forget forget);

    // Records `current` as the published container set and forgets, in
    // sorted order, every previously published id it no longer contains.
    // Returns the forgotten ids.
    QStringList reconcile(const QStringList &current);
    // Forgets one id now (a path that must clear state before the next
    // publication) and keeps the ledger from forgetting it a second time.
    void forget(const QString &containerId);
    [[nodiscard]] bool knows(const QString &containerId) const
    {
        return m_published.contains(containerId);
    }

private:
    Forget m_forget;
    QSet<QString> m_published;
};

} // namespace QindaQt::Compositor::KWinIntegration
