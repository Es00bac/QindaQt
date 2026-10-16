// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// ADR-0384: which window to activate when focus landed on a hidden member of
// a container. A hidden member is never left active -- the visibility
// snapshot cannot describe a hidden active window, and the user cannot see
// what has focus. Prefers `preferred` (the container's chrome anchor, its
// topmost member) when it is shown, then the active page's members in layout
// order. Returns nullopt when the container shows no member at all (rolled up
// or minimized), in which case focus moves on to another window.
[[nodiscard]] inline std::optional<QString> visibleMemberToActivate(
    const QStringList &activePageMembers, const QString &preferred,
    const std::function<bool(const QString &windowId)> &shown)
{
    if (!shown) {
        return std::nullopt;
    }
    if (!preferred.isEmpty() && activePageMembers.contains(preferred) && shown(preferred)) {
        return preferred;
    }
    for (const auto &windowId : activePageMembers) {
        if (shown(windowId)) {
            return windowId;
        }
    }
    return std::nullopt;
}

} // namespace QindaQt::Compositor::KWinIntegration
