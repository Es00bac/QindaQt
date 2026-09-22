// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QVector>

namespace QindaQt::ShellOrchestration {

// The runtime boundary between a stored profile and one output generation.
//
// PanelLayoutSolver and PanelVisibilityInventoryAssembler are strict about a
// panel that names an output the generation does not have, and must stay that
// way: that rejection is how the customization editor refuses a user who picks
// a disconnected display. Runtime is a different question. A pin that was
// valid when it was made must not destroy the whole layout the moment its
// display is unplugged or reconfigured - an absent output otherwise took every
// panel on the surviving displays with it, because the solve failed and the
// shell kept its stale surface set.
//
// A panel pinned to an absent output simply has nowhere to be this generation,
// exactly like a wildcard panel on a removed output, and it returns on the
// generation that brings its display back. Give the solver and the assembler
// the same filtered profile; disagreeing makes the assembler report a missing
// surface for a panel the solve deliberately skipped.
class PanelProfileOutputOccupancy final {
public:
    [[nodiscard]] static Profiles::LayoutProfile presentOutputsOnly(
        const Profiles::LayoutProfile &profile,
        const QVector<ShellLayout::LogicalOutput> &outputs);
};

} // namespace QindaQt::ShellOrchestration
