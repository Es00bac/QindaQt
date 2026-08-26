// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMap>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct HybridGroupStackingInput final
{
    QStringList activeMembers;
    // Dialog/transient descendants are not topology members. They travel with
    // the block but remain above every member and above scene chrome.
    QStringList associatedTransients;
    // Every transient names the grouped member at the root of its native
    // transient chain. KWin may force a dialog immediately above that owner;
    // planning the owner as the top member prevents the native constraint from
    // splitting the compact member block.
    QMap<QString, QString> transientOwnerById;
};

struct HybridGroupStackingBlock final
{
    QString containerId;
    QStringList membersBottomToTop;
    QStringList transientsBottomToTop;

    [[nodiscard]] QString topMemberId() const
    {
        return membersBottomToTop.isEmpty() ? QString{}
                                            : membersBottomToTop.constLast();
    }

    friend bool operator==(const HybridGroupStackingBlock &,
                           const HybridGroupStackingBlock &) = default;
};

struct HybridGroupStackingPlan final
{
    QStringList windowsBottomToTop;
    QVector<HybridGroupStackingBlock> blocksBottomToTop;

    friend bool operator==(const HybridGroupStackingPlan &,
                           const HybridGroupStackingPlan &) = default;
};

// Compacts each active-page member set at its former topmost member's rank.
// Ungrouped windows keep their relative order, so activating a member raises
// the collapsed group while a later unrelated activation can cover it again.
[[nodiscard]] std::optional<HybridGroupStackingPlan> planHybridGroupStacking(
    const QStringList &windowsBottomToTop,
    const QMap<QString, HybridGroupStackingInput> &groups,
    QString *error = nullptr);

} // namespace QindaQt::Compositor::KWinIntegration
