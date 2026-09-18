// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "containercloseprompt.h"

#include <QString>
#include <Qt>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// The compositor's half of the windowManagement.* live bridge (ADR-0209):
// the `[QindaQt]` group qindaqt-session writes into kwinrc, decoded here
// without KConfig so the mapping is unit-testable. Spellings are the
// Settings1 enum values verbatim; anything unrecognised falls back to the
// documented default for that one entry, never the whole group.
struct WindowManagementConfig final {
    // The exact pointer chord for QindaQt docking, or nullopt when disabled.
    std::optional<Qt::KeyboardModifiers> dockingModifiers = Qt::MetaModifier | Qt::ShiftModifier;
    // nullopt means "ask": the close prompt decides. Otherwise the decision
    // taken without a prompt.
    std::optional<ContainerCloseDecision> closeDecision;
    bool sessionRestore = true;

    [[nodiscard]] bool operator==(const WindowManagementConfig &) const = default;

    // `dockingModifier` in super|alt|control|disabled, `closeContainerPolicy`
    // in ask|close-all|ungroup, `sessionRestore` as KConfig spells a bool.
    [[nodiscard]] static WindowManagementConfig fromEntries(const QString &dockingModifier,
                                                            const QString &closeContainerPolicy,
                                                            const QString &sessionRestore);
    [[nodiscard]] static std::optional<Qt::KeyboardModifiers> dockingModifiersFor(
        const QString &name);
};

} // namespace QindaQt::Compositor::KWinIntegration
