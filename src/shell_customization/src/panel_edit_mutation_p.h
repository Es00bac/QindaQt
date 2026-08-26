// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "applet_placement_validator_p.h"

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/editing_result.h"

#include <optional>

namespace QindaQt::ShellCustomization::PanelEditMutation {

[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const AddPanelCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const RemovePanelCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const MovePanelCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const ConfigurePanelCommand &command,
    const AppletPlacementValidator &placementValidator);

} // namespace QindaQt::ShellCustomization::PanelEditMutation
