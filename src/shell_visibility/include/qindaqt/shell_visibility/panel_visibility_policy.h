// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/panel_visibility_types.h"

namespace QindaQt::ShellVisibility {

class PanelVisibilityPolicy final {
public:
  // Semantics after atomic validation:
  // - relevant windows are non-minimized/non-hidden application windows on
  //   the current workspace and activity;
  // - Never is visible; Always is hidden; reveal or hold forces every other
  //   mode visible, with hold taking priority when both are supplied;
  // - DodgeActive hides for the one active window intersecting the panel,
  //   while DodgeAll hides for the first intersecting relevant window;
  // - Maximized hides for a fully maximized relevant window whose assigned
  //   output matches the panel, regardless of rectangle overlap; and
  // - Intelligent first applies active overlap, then maximized-on-output.
  // Intersections are inclusive and use surfaceGeometry, never a work area.
  // A visible ReserveWhenVisible panel requests Reserve; every hidden or
  // NeverReserve panel requests Release.
  //
  // AGENT-CONTRACT: The compositor/shell controller supplies one immutable,
  // coherent logical snapshot. Evaluation retains nothing, has no timers or
  // side effects, and is safe for concurrent calls with independent values.
  // A malformed member rejects the complete batch and returns no decisions.
  [[nodiscard]] static PanelVisibilityEvaluation
  evaluate(const PanelVisibilityInventory &inventory);
};

} // namespace QindaQt::ShellVisibility
