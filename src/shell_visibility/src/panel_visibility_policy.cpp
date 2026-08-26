// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_visibility/panel_visibility_policy.h"

#include "visibility_inventory_validator_p.h"

#include <QHash>

#include <utility>

namespace QindaQt::ShellVisibility {
namespace {

bool rectanglesIntersect(const QRect &left, const QRect &right) {
  return static_cast<qint64>(left.left()) <= right.right() &&
         static_cast<qint64>(right.left()) <= left.right() &&
         static_cast<qint64>(left.top()) <= right.bottom() &&
         static_cast<qint64>(right.top()) <= left.bottom();
}

bool belongsToScope(const LogicalWindowSnapshot &window,
                    const DesktopScopeSnapshot &scope) {
  const bool workspaceMatches =
      window.onAllWorkspaces || window.workspaceIds.contains(scope.workspaceId);
  const bool activityMatches = window.activityIds.isEmpty() ||
                               window.activityIds.contains(scope.activityId);
  return workspaceMatches && activityMatches && !window.minimized &&
         !window.hidden;
}

const LogicalWindowSnapshot *
firstActiveOverlap(const PanelVisibilitySnapshot &panel,
                   const PanelVisibilityInventory &inventory) {
  for (const auto &window : inventory.windows) {
    // AGENT-CONTRACT: dodge overlap uses the actual panel surface rectangle,
    // never the reserved work area. A spanning window may therefore dodge a
    // panel even when its compositor-assigned output differs.
    if (belongsToScope(window, inventory.scope) && window.active &&
        rectanglesIntersect(window.frameGeometry, panel.surfaceGeometry)) {
      return &window;
    }
  }
  return nullptr;
}

const LogicalWindowSnapshot *
firstAnyOverlap(const PanelVisibilitySnapshot &panel,
                const PanelVisibilityInventory &inventory) {
  for (const auto &window : inventory.windows) {
    if (belongsToScope(window, inventory.scope) &&
        rectanglesIntersect(window.frameGeometry, panel.surfaceGeometry)) {
      return &window;
    }
  }
  return nullptr;
}

const LogicalWindowSnapshot *
firstMaximizedOnOutput(const PanelVisibilitySnapshot &panel,
                       const PanelVisibilityInventory &inventory) {
  for (const auto &window : inventory.windows) {
    if (belongsToScope(window, inventory.scope) && window.maximized &&
        window.outputId == panel.identity.outputId) {
      return &window;
    }
  }
  return nullptr;
}

PanelVisibilityDecision visibleDecision(const PanelVisibilitySnapshot &panel,
                                        PanelVisibilityReason reason) {
  const auto reservation =
      panel.reservationPolicy == PanelReservationPolicy::ReserveWhenVisible
          ? PanelReservationIntent::Reserve
          : PanelReservationIntent::Release;
  return {panel.identity, PanelVisibility::Visible, reservation, reason, {}};
}

PanelVisibilityDecision
hiddenDecision(const PanelVisibilitySnapshot &panel,
               PanelVisibilityReason reason,
               const LogicalWindowSnapshot *trigger = nullptr) {
  return {panel.identity, PanelVisibility::Hidden,
          PanelReservationIntent::Release, reason,
          trigger == nullptr ? QString{} : trigger->id};
}

PanelVisibilityDecision
evaluatePanel(const PanelVisibilitySnapshot &panel,
              const PanelInteractionSnapshot *interaction,
              const PanelVisibilityInventory &inventory) {
  // Never is intentionally first: no transient reveal/hold combination can
  // turn an always-visible panel into a hidden one.
  if (panel.hideMode == Profiles::HideMode::Never) {
    return visibleDecision(panel, PanelVisibilityReason::NeverMode);
  }
  if (interaction != nullptr && interaction->visibilityHeld) {
    return visibleDecision(panel, PanelVisibilityReason::VisibilityHeld);
  }
  if (interaction != nullptr && interaction->revealRequested) {
    return visibleDecision(panel, PanelVisibilityReason::RevealRequested);
  }

  const LogicalWindowSnapshot *trigger = nullptr;
  switch (panel.hideMode) {
  case Profiles::HideMode::Never:
    return visibleDecision(panel, PanelVisibilityReason::NeverMode);
  case Profiles::HideMode::Always:
    return hiddenDecision(panel, PanelVisibilityReason::AlwaysMode);
  case Profiles::HideMode::DodgeActive:
    trigger = firstActiveOverlap(panel, inventory);
    return trigger == nullptr
               ? visibleDecision(panel, PanelVisibilityReason::NoConflict)
               : hiddenDecision(panel,
                                PanelVisibilityReason::ActiveWindowOverlap,
                                trigger);
  case Profiles::HideMode::DodgeAll:
    trigger = firstAnyOverlap(panel, inventory);
    return trigger == nullptr
               ? visibleDecision(panel, PanelVisibilityReason::NoConflict)
               : hiddenDecision(panel, PanelVisibilityReason::AnyWindowOverlap,
                                trigger);
  case Profiles::HideMode::Maximized:
    trigger = firstMaximizedOnOutput(panel, inventory);
    return trigger == nullptr
               ? visibleDecision(panel, PanelVisibilityReason::NoConflict)
               : hiddenDecision(panel,
                                PanelVisibilityReason::MaximizedWindowOnOutput,
                                trigger);
  case Profiles::HideMode::Intelligent:
    trigger = firstActiveOverlap(panel, inventory);
    if (trigger != nullptr) {
      return hiddenDecision(panel, PanelVisibilityReason::ActiveWindowOverlap,
                            trigger);
    }
    trigger = firstMaximizedOnOutput(panel, inventory);
    return trigger == nullptr
               ? visibleDecision(panel, PanelVisibilityReason::NoConflict)
               : hiddenDecision(panel,
                                PanelVisibilityReason::MaximizedWindowOnOutput,
                                trigger);
  }

  // validateInventory() rejects unknown enum values before any decisions are
  // emitted. This fallback protects compilers that do not model that proof.
  return hiddenDecision(panel, PanelVisibilityReason::AlwaysMode);
}

} // namespace

PanelVisibilityEvaluation
PanelVisibilityPolicy::evaluate(const PanelVisibilityInventory &inventory) {
  if (auto error = Private::validateInventory(inventory); error.hasError()) {
    return {{}, std::move(error)};
  }

  QHash<QString, QHash<QString, const PanelInteractionSnapshot *>> interactions;
  for (const auto &interaction : inventory.interactions) {
    interactions[interaction.identity.outputId].insert(
        interaction.identity.panelId, &interaction);
  }

  PanelVisibilityEvaluation result;
  result.decisions.reserve(inventory.panels.size());
  for (const auto &panel : inventory.panels) {
    const PanelInteractionSnapshot *interaction = nullptr;
    const auto outputInteractions =
        interactions.constFind(panel.identity.outputId);
    if (outputInteractions != interactions.cend()) {
      interaction = outputInteractions->value(panel.identity.panelId, nullptr);
    }
    result.decisions.push_back(evaluatePanel(panel, interaction, inventory));
  }
  return result;
}

} // namespace QindaQt::ShellVisibility
