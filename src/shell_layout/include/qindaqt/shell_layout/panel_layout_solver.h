// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

namespace QindaQt::ShellLayout {

class PanelLayoutSolver final {
public:
    // The solver owns no input or output state and is safe to call concurrently
    // when callers do not mutate their vectors. Errors never expose a partial
    // layout: both result vectors are empty unless ok() is true.
    [[nodiscard]] static PanelLayoutResult solve(const QVector<Profiles::PanelSpec> &panels,
                                                 const QVector<LogicalOutput> &outputs);

    // True when this output generation can host the panel at all: a wildcard
    // panel always can (it simply expands to however many outputs exist), and
    // a pinned panel exactly when its named output is present.
    //
    // AGENT-CONTRACT: this is the same match solve() performs before failing
    // with MissingOutput. Runtime callers that must partition a stored profile
    // into "placeable now" and "waiting for its display" ask here rather than
    // re-implementing the rule; changing one without the other reintroduces
    // the class of defect where a solve and its caller disagree about which
    // panels exist this generation.
    [[nodiscard]] static bool outputsCanHost(const Profiles::PanelSpec &panel,
                                             const QVector<LogicalOutput> &outputs);
};

} // namespace QindaQt::ShellLayout
