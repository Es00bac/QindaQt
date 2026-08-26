// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "applet_placement_validator_p.h"

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/editing_result.h"

#include <optional>

namespace QindaQt::ShellCustomization {

class LayoutEditMutation final {
public:
    [[nodiscard]] static std::optional<EditingError> apply(
        Profiles::LayoutProfile &candidate,
        const EditingCommand &command,
        const AppletPlacementValidator &placementValidator);
};

} // namespace QindaQt::ShellCustomization
