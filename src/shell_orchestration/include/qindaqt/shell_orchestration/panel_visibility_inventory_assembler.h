// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_types.h"
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"
#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QString>

#include <optional>

namespace QindaQt::ShellOrchestration {

enum class PanelVisibilityAssemblyErrorCode {
    None,
    InvalidProfile,
    RejectedLayout,
    OutputMismatch,
    DuplicateSurface,
    MissingSurface,
    UnknownPanel,
    SurfaceContractMismatch,
    InvalidInventory,
};

struct PanelVisibilityAssemblyError {
    PanelVisibilityAssemblyErrorCode code = PanelVisibilityAssemblyErrorCode::None;
    ShellVisibility::PanelSurfaceIdentity identity;
    QString message;
    ShellVisibility::PanelVisibilityError inventoryError;

    [[nodiscard]] bool hasError() const noexcept
    {
        return code != PanelVisibilityAssemblyErrorCode::None;
    }
};

struct PanelVisibilityAssemblyResult {
    std::optional<ShellVisibility::PanelVisibilityInventory> inventory;
    ShellVisibility::PanelVisibilityEvaluation evaluation;
    PanelVisibilityAssemblyError error;

    [[nodiscard]] bool ok() const noexcept
    {
        return inventory.has_value() && evaluation.ok() && !error.hasError();
    }
};

class PanelVisibilityInventoryAssembler final {
public:
    // Requires exact compositor/layout output identity, geometry, and scale.
    // One mismatch rejects the whole generation before window policy runs.
    [[nodiscard]] static PanelVisibilityAssemblyResult assemble(
        const Profiles::LayoutProfile &profile,
        const ShellLayout::PanelLayoutResult &layout,
        const ShellVisibility::CompositorVisibilitySnapshot &compositor,
        const QVector<ShellVisibility::PanelInteractionSnapshot> &interactions);
};

} // namespace QindaQt::ShellOrchestration
