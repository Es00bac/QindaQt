// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupstackingpolicy.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

std::optional<HybridGroupStackingPlan> fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return std::nullopt;
}

} // namespace

std::optional<HybridGroupStackingPlan> planHybridGroupStacking(
    const QStringList &windowsBottomToTop,
    const QMap<QString, HybridGroupStackingInput> &groups,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const QSet<QString> stackIds(windowsBottomToTop.cbegin(),
                                 windowsBottomToTop.cend());
    if (stackIds.size() != windowsBottomToTop.size()
        || stackIds.contains(QString{})) {
        return fail(error, QStringLiteral("window stack IDs must be unique and nonempty"));
    }

    QHash<QString, QString> owners;
    QMap<QString, qsizetype> topIndices;
    QHash<QString, bool> transientFlags;
    for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
        if (group.key().isEmpty() || group.value().activeMembers.isEmpty()) {
            return fail(error, QStringLiteral("stack groups must have a container and active members"));
        }
        QSet<QString> local;
        const auto registerWindow = [&](const QString &windowId, bool transient) {
            if (windowId.isEmpty() || local.contains(windowId)
                || !stackIds.contains(windowId) || owners.contains(windowId)) {
                return false;
            }
            local.insert(windowId);
            owners.insert(windowId, group.key());
            transientFlags.insert(windowId, transient);
            topIndices.insert(group.key(),
                              std::max(topIndices.value(group.key(), qsizetype(-1)),
                                       windowsBottomToTop.indexOf(windowId)));
            return true;
        };
        for (const auto &memberId : group.value().activeMembers) {
            if (!registerWindow(memberId, false)) {
                return fail(error,
                            QStringLiteral("invalid or multiply-owned stack member '%1'")
                                .arg(memberId));
            }
        }
        for (const auto &transientId : group.value().associatedTransients) {
            if (!registerWindow(transientId, true)) {
                return fail(error,
                            QStringLiteral("invalid or multiply-owned stack transient '%1'")
                                .arg(transientId));
            }
        }
        if (group.value().transientOwnerById.size()
                != group.value().associatedTransients.size()) {
            return fail(error,
                        QStringLiteral("stack transients must name their grouped owners"));
        }
        for (auto owner = group.value().transientOwnerById.cbegin();
             owner != group.value().transientOwnerById.cend(); ++owner) {
            if (!group.value().associatedTransients.contains(owner.key())
                || !group.value().activeMembers.contains(owner.value())) {
                return fail(error,
                            QStringLiteral("stack transient '%1' has an invalid grouped owner")
                                .arg(owner.key()));
            }
        }
    }

    QMap<QString, QStringList> orderedMembers;
    QMap<QString, QStringList> orderedTransients;
    for (const auto &windowId : windowsBottomToTop) {
        const auto owner = owners.value(windowId);
        if (!owner.isEmpty()) {
            (transientFlags.value(windowId) ? orderedTransients[owner]
                                            : orderedMembers[owner])
                .append(windowId);
        }
    }

    HybridGroupStackingPlan result;
    for (qsizetype index = 0; index < windowsBottomToTop.size(); ++index) {
        const auto &windowId = windowsBottomToTop[index];
        const auto owner = owners.value(windowId);
        if (owner.isEmpty()) {
            result.windowsBottomToTop.append(windowId);
            continue;
        }
        if (index != topIndices.value(owner)) {
            continue;
        }
        auto members = orderedMembers.value(owner);
        const auto transients = orderedTransients.value(owner);
        // Process bottom-to-top so the owner of the topmost transient becomes
        // the top member when different transient subtrees have different
        // grouped roots. KWin can then satisfy every native transient-above-
        // owner constraint without interleaving a dialog between members.
        for (const auto &transientId : transients) {
            const auto memberId = groups.value(owner)
                                      .transientOwnerById.value(transientId);
            members.removeAll(memberId);
            members.append(memberId);
        }
        HybridGroupStackingBlock block{owner, std::move(members), transients};
        result.windowsBottomToTop.append(block.membersBottomToTop);
        result.windowsBottomToTop.append(block.transientsBottomToTop);
        result.blocksBottomToTop.append(std::move(block));
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
