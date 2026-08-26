// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridstackingorder.h"

#include <QSet>

namespace QindaQt::Compositor::KWinIntegration {

QStringList topmostMemberContainerOrder(const QStringList &ownersBottomToTop,
                                        const QStringList &containerIds)
{
    const QSet<QString> known(containerIds.cbegin(), containerIds.cend());
    QStringList ranked;
    for (const auto &owner : ownersBottomToTop) {
        if (owner.isEmpty() || !known.contains(owner)) {
            continue;
        }
        // AGENT-GUARD: KWin's list is bottom-to-top, so replacing an earlier
        // occurrence makes the container's rank equal to its topmost member.
        // Keeping the first occurrence routes overlapping chrome to a group
        // whose last member may actually be below another group.
        ranked.removeAll(owner);
        ranked.append(owner);
    }

    QStringList result;
    for (const auto &containerId : containerIds) {
        if (!containerId.isEmpty() && !ranked.contains(containerId)
            && !result.contains(containerId)) {
            result.append(containerId);
        }
    }
    result.append(ranked);
    return result;
}

QStringList topmostActiveMemberContainerOrder(
    const QStringList &windowIdsBottomToTop,
    const QHash<QString, QString> &activeMemberOwners,
    const QStringList &containerIds)
{
    QStringList ownersBottomToTop;
    ownersBottomToTop.reserve(windowIdsBottomToTop.size());
    for (const auto &windowId : windowIdsBottomToTop) {
        ownersBottomToTop.append(activeMemberOwners.value(windowId));
    }
    return topmostMemberContainerOrder(ownersBottomToTop, containerIds);
}

} // namespace QindaQt::Compositor::KWinIntegration
