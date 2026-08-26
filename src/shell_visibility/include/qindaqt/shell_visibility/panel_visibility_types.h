// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_types.h"

#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::ShellVisibility {

// Identifies one expanded panel surface. Wildcard profile panels share a
// panelId, so outputId is always part of runtime identity.
struct PanelSurfaceIdentity {
  QString panelId;
  QString outputId;

  friend bool operator==(const PanelSurfaceIdentity &,
                         const PanelSurfaceIdentity &) = default;
};

struct LogicalOutputSnapshot {
  // IDs are unique within one inventory; geometry is desktop-logical and may
  // be negative or overlap another output for mirroring.
  QString id;
  QRect geometry;

  friend bool operator==(const LogicalOutputSnapshot &,
                         const LogicalOutputSnapshot &) = default;
};

enum class PanelReservationPolicy {
  NeverReserve,
  ReserveWhenVisible,
};

struct PanelVisibilitySnapshot {
  PanelSurfaceIdentity identity;
  // Geometry is the complete panel surface in desktop-logical coordinates.
  // Overlap policy never substitutes the output work area for this rectangle.
  QRect surfaceGeometry;
  Profiles::HideMode hideMode = Profiles::HideMode::Never;
  PanelReservationPolicy reservationPolicy =
      PanelReservationPolicy::ReserveWhenVisible;

  friend bool operator==(const PanelVisibilitySnapshot &,
                         const PanelVisibilitySnapshot &) = default;
};

struct LogicalWindowSnapshot {
  QString id;
  // The compositor-selected output used by maximized-mode policy. A spanning
  // window can still overlap a panel on another output through frameGeometry.
  QString outputId;
  QRect frameGeometry;
  // When onAllWorkspaces is true, workspaceId must be empty. Otherwise it is
  // the one workspace containing the window.
  QString workspaceId;
  // An empty list means the window is present on every activity.
  QStringList activityIds;
  bool onAllWorkspaces = false;
  bool active = false;
  // True means fully maximized on both axes; partial maximize and quick-tile
  // states are false at this boundary.
  bool maximized = false;
  bool minimized = false;
  bool hidden = false;

  friend bool operator==(const LogicalWindowSnapshot &,
                         const LogicalWindowSnapshot &) = default;
};

struct DesktopScopeSnapshot {
  QString workspaceId;
  QString activityId;

  friend bool operator==(const DesktopScopeSnapshot &,
                         const DesktopScopeSnapshot &) = default;
};

struct PanelInteractionSnapshot {
  PanelSurfaceIdentity identity;
  // Reveal is the current edge/shortcut reveal request. Hold is supplied by
  // shell-owned interactions such as an open menu or pointer containment.
  bool revealRequested = false;
  bool visibilityHeld = false;

  friend bool operator==(const PanelInteractionSnapshot &,
                         const PanelInteractionSnapshot &) = default;
};

struct PanelVisibilityInventory {
  // AGENT-NOTE: These are value snapshots, not live object handles. The
  // caller must finish copying one compositor generation before evaluation
  // and must not mutate that generation concurrently.
  QVector<LogicalOutputSnapshot> outputs;
  QVector<PanelVisibilitySnapshot> panels;
  QVector<LogicalWindowSnapshot> windows;
  DesktopScopeSnapshot scope;
  // Missing panel identities have both interaction flags false.
  QVector<PanelInteractionSnapshot> interactions;
};

enum class PanelVisibility {
  Visible,
  Hidden,
};

enum class PanelReservationIntent {
  Reserve,
  Release,
};

enum class PanelVisibilityReason {
  NeverMode,
  VisibilityHeld,
  RevealRequested,
  AlwaysMode,
  ActiveWindowOverlap,
  AnyWindowOverlap,
  MaximizedWindowOnOutput,
  NoConflict,
};

struct PanelVisibilityDecision {
  PanelSurfaceIdentity identity;
  PanelVisibility visibility = PanelVisibility::Visible;
  PanelReservationIntent reservation = PanelReservationIntent::Release;
  PanelVisibilityReason reason = PanelVisibilityReason::NoConflict;
  // Populated only for a window-triggered hidden decision. Inventory order
  // deterministically selects the witness when several windows qualify.
  QString triggerWindowId;

  friend bool operator==(const PanelVisibilityDecision &,
                         const PanelVisibilityDecision &) = default;
};

enum class PanelVisibilityErrorCode {
  None,
  InvalidScope,
  EmptyOutputInventory,
  InvalidOutputId,
  DuplicateOutputId,
  InvalidOutputGeometry,
  InvalidPanelIdentity,
  DuplicatePanelIdentity,
  UnknownPanelOutput,
  InvalidPanelGeometry,
  PanelOutsideOutput,
  InvalidHideMode,
  InvalidReservationPolicy,
  InvalidWindowId,
  DuplicateWindowId,
  UnknownWindowOutput,
  InvalidWindowGeometry,
  WindowOutsideAssignedOutput,
  InvalidWindowScope,
  InvalidWindowState,
  MultipleActiveWindows,
  InvalidInteractionIdentity,
  DuplicateInteractionIdentity,
  UnknownInteractionPanel,
};

struct PanelVisibilityError {
  PanelVisibilityErrorCode code = PanelVisibilityErrorCode::None;
  PanelSurfaceIdentity panel;
  QString windowId;
  QString outputId;
  QString message;

  [[nodiscard]] bool hasError() const noexcept {
    return code != PanelVisibilityErrorCode::None;
  }

  friend bool operator==(const PanelVisibilityError &,
                         const PanelVisibilityError &) = default;
};

struct PanelVisibilityEvaluation {
  QVector<PanelVisibilityDecision> decisions;
  PanelVisibilityError error;

  [[nodiscard]] bool ok() const noexcept { return !error.hasError(); }
};

} // namespace QindaQt::ShellVisibility
