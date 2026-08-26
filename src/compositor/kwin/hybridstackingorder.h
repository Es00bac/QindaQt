// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QStringList>

namespace QindaQt::Compositor::KWinIntegration {

// Converts the owner observed for each KWin window, in KWin's bottom-to-top
// order, into one entry per Hybrid container. Containers missing from the
// window stack are kept deterministically below all ranked containers.
[[nodiscard]] QStringList topmostMemberContainerOrder(
    const QStringList &ownersBottomToTop,
    const QStringList &containerIds);

// Ranks only members in each container's active page. An inactive page leaf
// can retain a high native stack slot, but no scene chrome is anchored there.
[[nodiscard]] QStringList topmostActiveMemberContainerOrder(
    const QStringList &windowIdsBottomToTop,
    const QHash<QString, QString> &activeMemberOwners,
    const QStringList &containerIds);

} // namespace QindaQt::Compositor::KWinIntegration
