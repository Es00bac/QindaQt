// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositoroutputauthority.h"

#include "qindaqt/shell_layout/panel_layout_types.h"
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

#include <QString>

#include <optional>

namespace QindaQt::Shell {

enum class NotificationOutputSelectionError {
    None,
    MissingAuthority,
    MissingVisibility,
    GenerationMismatch,
    InventoryMismatch,
};

struct NotificationOutputSelection final {
    QString outputId;
    NotificationOutputSelectionError error = NotificationOutputSelectionError::None;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NotificationOutputSelectionError::None
            && !outputId.isEmpty();
    }
};

class NotificationOutputSelector final {
public:
    // Outputs() order is KWin's public semantic order. Selection is permitted
    // only when that projection, ShellVisibilitySnapshot, and the current Qt
    // inventory describe one exact generation and output-ID set.
    [[nodiscard]] static NotificationOutputSelection select(
        const std::optional<CompositorOutputAuthorityFrame> &authority,
        const ShellVisibility::CompositorVisibilitySnapshot *visibility,
        const QVector<ShellLayout::LogicalOutput> &qtOutputs);
};

} // namespace QindaQt::Shell
