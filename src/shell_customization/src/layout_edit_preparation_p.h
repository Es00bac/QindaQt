// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "applet_placement_validator_p.h"
#include "layout_candidate_validator_p.h"

#include "qindaqt/shell_customization/editing_commands.h"

namespace QindaQt::ShellCustomization {

class LayoutEditPreparation final {
public:
    // AGENT-CONTRACT: execute() and evaluate() both use this function. Keeping
    // mutation and every candidate-validation gate here prevents highlighted
    // editor targets from accepting a different language than actual drops.
    [[nodiscard]] static CandidateValidation prepare(
        const Profiles::LayoutProfile &current,
        const EditingCommand &command,
        const AppletPlacementValidator &placementValidator,
        const QVector<ShellLayout::LogicalOutput> &outputs);
};

} // namespace QindaQt::ShellCustomization
