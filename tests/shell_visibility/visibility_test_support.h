// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/panel_visibility_policy.h"

#include <utility>

namespace QindaQt::ShellVisibility::TestSupport {

inline LogicalOutputSnapshot output(QString id = QStringLiteral("eDP-1"),
                                    QRect geometry = QRect(0, 0, 1920, 1080)) {
  return {std::move(id), geometry};
}

inline PanelVisibilitySnapshot
panel(Profiles::HideMode mode, QString id = QStringLiteral("panel"),
      QString outputId = QStringLiteral("eDP-1"),
      QRect geometry = QRect(0, 0, 1920, 32),
      PanelReservationPolicy reservation =
          PanelReservationPolicy::ReserveWhenVisible) {
  return {{std::move(id), std::move(outputId)}, geometry, mode, reservation};
}

inline LogicalWindowSnapshot
window(QString id = QStringLiteral("window"),
       QRect geometry = QRect(0, 0, 800, 600),
       QString outputId = QStringLiteral("eDP-1")) {
  LogicalWindowSnapshot snapshot;
  snapshot.id = std::move(id);
  snapshot.outputId = std::move(outputId);
  snapshot.frameGeometry = geometry;
  snapshot.workspaceIds = {QStringLiteral("workspace-1")};
  snapshot.activityIds = {QStringLiteral("activity-a")};
  return snapshot;
}

inline PanelVisibilityInventory inventory(Profiles::HideMode mode) {
  PanelVisibilityInventory snapshot;
  snapshot.outputs = {output()};
  snapshot.panels = {panel(mode)};
  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};
  return snapshot;
}

inline PanelInteractionSnapshot
interaction(bool reveal, bool hold, QString panelId = QStringLiteral("panel"),
            QString outputId = QStringLiteral("eDP-1")) {
  return {{std::move(panelId), std::move(outputId)}, reveal, hold};
}

} // namespace QindaQt::ShellVisibility::TestSupport
