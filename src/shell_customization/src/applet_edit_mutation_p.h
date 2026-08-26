// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "applet_placement_validator_p.h"

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/editing_result.h"

#include <optional>

namespace QindaQt::ShellCustomization::AppletEditMutation {

[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const InsertAppletCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const MoveAppletCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const RemoveAppletCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const DuplicateAppletCommand &command,
    const AppletPlacementValidator &placementValidator);
[[nodiscard]] std::optional<EditingError> apply(
    Profiles::LayoutProfile &profile,
    const UpdateAppletSettingsCommand &command,
    const AppletPlacementValidator &placementValidator);

} // namespace QindaQt::ShellCustomization::AppletEditMutation
