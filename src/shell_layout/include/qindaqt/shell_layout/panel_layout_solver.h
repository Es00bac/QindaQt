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
};

} // namespace QindaQt::ShellLayout
