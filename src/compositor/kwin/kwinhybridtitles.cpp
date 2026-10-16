// SPDX-License-Identifier: GPL-3.0-or-later
//
// The caption path of KWinHybridSession (ADR-0383). A title-only change
// patches the published chrome plans and repaints at most each affected
// container's title band; it never re-plans a container, never re-renders a
// full chrome frame, and never runs synchronizeChrome() unless the title
// would move geometry.
//
// AGENT-NOTE: a translation unit of KWinHybridSession like the other
// kwinhybrid*.cpp files; it owns no state of its own.

#include "kwinhybridsession.h"

#include "hybridchromeplanbuilder.h"
#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "managedwindowregistry.h"

#include <window.h>

namespace QindaQt::Compositor::KWinIntegration {

void KWinHybridSession::refreshWindowTitles(const QStringList &windowIds)
{
    // A synchronization in flight samples every caption itself, and a session
    // that is not ready has no published chrome to patch.
    if (m_shutdown || !ready() || m_synchronizingChrome || !m_chromeManager) {
        return;
    }
    const auto &topology = m_runtime->topology();
    QStringList containerIds;
    for (const auto &windowId : windowIds) {
        // ADR-0203: an iconified window's chip shows "App — Title".
        if (isWindowIconified(windowId)) {
            QString error;
            if (!publishIconChip(windowId, &error)) {
                qWarning("QindaQt could not retitle the chip of '%s': %s",
                         qPrintable(windowId), qPrintable(error));
            }
        }
        const auto owner = topology.ownerOf(windowId);
        if (owner && !containerIds.contains(*owner)) {
            containerIds.append(*owner);
        }
    }
    // AGENT-CONTRACT: the same caption read synchronizeChrome() hands to
    // HybridChromePlanBuilder::build(), so a patched plan equals the plan a
    // full synchronization would have published.
    const HybridWindowTitleLookup titleLookup = [this](const QString &windowId) {
        const auto *window = m_registry.window(windowId);
        return window ? window->caption() : QString{};
    };
    bool patched = false;
    for (const auto &containerId : std::as_const(containerIds)) {
        const auto *container = topology.container(containerId);
        const auto published = m_chromeManager->plan(containerId);
        if (!container || !published) {
            continue;
        }
        HybridChromePlanOptions options;
        options.containerTitle = published->containerTitle;
        const auto refresh = HybridChromePlanBuilder::refreshTitles(
            *container, *published, options, titleLookup);
        using Outcome = HybridChromePlanBuilder::TitleRefresh::Outcome;
        if (refresh.outcome == Outcome::NeedsReplan) {
            synchronizeChrome();
            return;
        }
        if (refresh.outcome != Outcome::Patched) {
            continue;
        }
        QString error;
        if (!m_chromeManager->updateTitles(containerId, refresh.plan, &error)) {
            qWarning("QindaQt Hybrid title update fell back to a full resync: %s",
                     qPrintable(error));
            synchronizeChrome();
            return;
        }
        patched = true;
    }
    if (patched) {
        // Accessible tab names are the plan's tab titles.
        synchronizeAccessibility();
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
