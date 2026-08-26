// SPDX-License-Identifier: LGPL-3.0-or-later
#include "visibility_inventory_validator_p.h"

#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QHash>
#include <QSet>
#include <QStringView>

#include <limits>
#include <cmath>
#include <utility>

namespace QindaQt::ShellVisibility::Private {
namespace {

PanelVisibilityError failure(PanelVisibilityErrorCode code, QString message,
                             PanelSurfaceIdentity panel = {},
                             QString windowId = {}, QString outputId = {}) {
  return {code, std::move(panel), std::move(windowId), std::move(outputId),
          std::move(message)};
}

bool hasWellFormedUtf16(QStringView text) {
  for (qsizetype index = 0; index < text.size(); ++index) {
    const QChar current = text[index];
    if (current.isHighSurrogate()) {
      if (index + 1 >= text.size() || !text[index + 1].isLowSurrogate()) {
        return false;
      }
      ++index;
    } else if (current.isLowSurrogate()) {
      return false;
    }
  }
  return true;
}

bool validIdentifier(const QString &value) {
  const QStringView view(value);
  return !view.isEmpty() && view == view.trimmed() && hasWellFormedUtf16(view);
}

bool hasSafeExtent(const QRect &geometry) {
  if (!geometry.isValid()) {
    return false;
  }
  const qint64 width =
      static_cast<qint64>(geometry.right()) - geometry.left() + 1;
  const qint64 height =
      static_cast<qint64>(geometry.bottom()) - geometry.top() + 1;
  return width > 0 && width <= std::numeric_limits<int>::max() && height > 0 &&
         height <= std::numeric_limits<int>::max();
}

bool contains(const QRect &outer, const QRect &inner) {
  return static_cast<qint64>(outer.left()) <= inner.left() &&
         static_cast<qint64>(inner.right()) <= outer.right() &&
         static_cast<qint64>(outer.top()) <= inner.top() &&
         static_cast<qint64>(inner.bottom()) <= outer.bottom();
}

bool intersects(const QRect &left, const QRect &right) {
  return static_cast<qint64>(left.left()) <= right.right() &&
         static_cast<qint64>(right.left()) <= left.right() &&
         static_cast<qint64>(left.top()) <= right.bottom() &&
         static_cast<qint64>(right.top()) <= left.bottom();
}

bool validHideMode(Profiles::HideMode mode) {
  switch (mode) {
  case Profiles::HideMode::Never:
  case Profiles::HideMode::Intelligent:
  case Profiles::HideMode::DodgeActive:
  case Profiles::HideMode::DodgeAll:
  case Profiles::HideMode::Maximized:
  case Profiles::HideMode::Always:
    return true;
  }
  return false;
}

bool validReservationPolicy(PanelReservationPolicy policy) {
  switch (policy) {
  case PanelReservationPolicy::NeverReserve:
  case PanelReservationPolicy::ReserveWhenVisible:
    return true;
  }
  return false;
}

PanelVisibilityError validateScope(const DesktopScopeSnapshot &scope) {
  if (!validIdentifier(scope.workspaceId) ||
      !validIdentifier(scope.activityId)) {
    return failure(
        PanelVisibilityErrorCode::InvalidScope,
        QStringLiteral("the current workspace and activity ids must be "
                       "non-empty canonical strings"));
  }
  return {};
}

PanelVisibilityError
validateOutputs(const QVector<LogicalOutputSnapshot> &outputs,
                QHash<QString, QRect> *geometries) {
  if (outputs.isEmpty()) {
    return failure(PanelVisibilityErrorCode::EmptyOutputInventory,
                   QStringLiteral("the logical output inventory is empty"));
  }
  geometries->reserve(outputs.size());
  for (const auto &output : outputs) {
    if (!validIdentifier(output.id)) {
      return failure(
          PanelVisibilityErrorCode::InvalidOutputId,
          QStringLiteral("an output id is empty, padded, or malformed"), {}, {},
          output.id);
    }
    if (geometries->contains(output.id)) {
      return failure(PanelVisibilityErrorCode::DuplicateOutputId,
                     QStringLiteral("duplicate output id '%1'").arg(output.id),
                     {}, {}, output.id);
    }
    if (!hasSafeExtent(output.geometry)) {
      return failure(PanelVisibilityErrorCode::InvalidOutputGeometry,
                     QStringLiteral("output '%1' has invalid logical geometry")
                         .arg(output.id),
                     {}, {}, output.id);
    }
    if (!std::isfinite(output.scale) || output.scale <= 0.0 ||
        output.scale > ShellVisibilityProtocol::WireLimits::MaxOutputScale) {
      return failure(PanelVisibilityErrorCode::InvalidOutputScale,
                     QStringLiteral("output '%1' has invalid scale metadata")
                         .arg(output.id),
                     {}, {}, output.id);
    }
    geometries->insert(output.id, output.geometry);
  }
  return {};
}

PanelVisibilityError
validatePanels(const QVector<PanelVisibilitySnapshot> &panels,
               const QHash<QString, QRect> &outputGeometries,
               QHash<QString, QSet<QString>> *panelIdsByOutput) {
  for (const auto &panel : panels) {
    if (!validIdentifier(panel.identity.panelId) ||
        !validIdentifier(panel.identity.outputId)) {
      return failure(
          PanelVisibilityErrorCode::InvalidPanelIdentity,
          QStringLiteral("a panel identity is empty, padded, or malformed"),
          panel.identity);
    }
    auto output = outputGeometries.constFind(panel.identity.outputId);
    if (output == outputGeometries.cend()) {
      return failure(PanelVisibilityErrorCode::UnknownPanelOutput,
                     QStringLiteral("panel '%1' references unknown output '%2'")
                         .arg(panel.identity.panelId, panel.identity.outputId),
                     panel.identity, {}, panel.identity.outputId);
    }
    auto &ids = (*panelIdsByOutput)[panel.identity.outputId];
    if (ids.contains(panel.identity.panelId)) {
      return failure(PanelVisibilityErrorCode::DuplicatePanelIdentity,
                     QStringLiteral("duplicate panel surface '(%1, %2)'")
                         .arg(panel.identity.panelId, panel.identity.outputId),
                     panel.identity);
    }
    ids.insert(panel.identity.panelId);
    if (!hasSafeExtent(panel.surfaceGeometry)) {
      return failure(PanelVisibilityErrorCode::InvalidPanelGeometry,
                     QStringLiteral("panel '%1' has invalid logical geometry")
                         .arg(panel.identity.panelId),
                     panel.identity);
    }
    if (!contains(output.value(), panel.surfaceGeometry)) {
      return failure(
          PanelVisibilityErrorCode::PanelOutsideOutput,
          QStringLiteral("panel '%1' is not contained by output '%2'")
              .arg(panel.identity.panelId, panel.identity.outputId),
          panel.identity, {}, panel.identity.outputId);
    }
    if (!validHideMode(panel.hideMode)) {
      return failure(PanelVisibilityErrorCode::InvalidHideMode,
                     QStringLiteral("panel '%1' has an invalid hide mode")
                         .arg(panel.identity.panelId),
                     panel.identity);
    }
    if (!validReservationPolicy(panel.reservationPolicy)) {
      return failure(
          PanelVisibilityErrorCode::InvalidReservationPolicy,
          QStringLiteral("panel '%1' has an invalid reservation policy")
              .arg(panel.identity.panelId),
          panel.identity);
    }
  }
  return {};
}

PanelVisibilityError validateWindowScope(const LogicalWindowSnapshot &window) {
  if ((window.onAllWorkspaces && !window.workspaceIds.isEmpty()) ||
      (!window.onAllWorkspaces && window.workspaceIds.isEmpty())) {
    return failure(
        PanelVisibilityErrorCode::InvalidWindowScope,
        QStringLiteral(
            "window '%1' must use one or more workspaces or the canonical "
            "all-workspaces form")
            .arg(window.id),
        {}, window.id, window.outputId);
  }
  QSet<QString> workspaces;
  for (const QString &workspaceId : window.workspaceIds) {
    if (!validIdentifier(workspaceId) || workspaces.contains(workspaceId)) {
      return failure(
          PanelVisibilityErrorCode::InvalidWindowScope,
          QStringLiteral("window '%1' has an invalid or duplicate workspace")
              .arg(window.id),
          {}, window.id, window.outputId);
    }
    workspaces.insert(workspaceId);
  }
  QSet<QString> activities;
  for (const QString &activityId : window.activityIds) {
    if (!validIdentifier(activityId) || activities.contains(activityId)) {
      return failure(
          PanelVisibilityErrorCode::InvalidWindowScope,
          QStringLiteral("window '%1' has an invalid or duplicate activity")
              .arg(window.id),
          {}, window.id, window.outputId);
    }
    activities.insert(activityId);
  }
  return {};
}

PanelVisibilityError
validateWindows(const QVector<LogicalWindowSnapshot> &windows,
                const QHash<QString, QRect> &outputGeometries) {
  QSet<QString> windowIds;
  QString activeWindowId;
  windowIds.reserve(windows.size());
  for (const auto &window : windows) {
    if (!validIdentifier(window.id)) {
      return failure(
          PanelVisibilityErrorCode::InvalidWindowId,
          QStringLiteral("a window id is empty, padded, or malformed"), {},
          window.id, window.outputId);
    }
    if (windowIds.contains(window.id)) {
      return failure(PanelVisibilityErrorCode::DuplicateWindowId,
                     QStringLiteral("duplicate window id '%1'").arg(window.id),
                     {}, window.id, window.outputId);
    }
    windowIds.insert(window.id);
    const auto output = outputGeometries.constFind(window.outputId);
    if (output == outputGeometries.cend()) {
      return failure(
          PanelVisibilityErrorCode::UnknownWindowOutput,
          QStringLiteral("window '%1' references unknown output '%2'")
              .arg(window.id, window.outputId),
          {}, window.id, window.outputId);
    }
    if (!hasSafeExtent(window.frameGeometry)) {
      return failure(PanelVisibilityErrorCode::InvalidWindowGeometry,
                     QStringLiteral("window '%1' has invalid logical geometry")
                         .arg(window.id),
                     {}, window.id, window.outputId);
    }
    if (!intersects(output.value(), window.frameGeometry)) {
      return failure(
          PanelVisibilityErrorCode::WindowOutsideAssignedOutput,
          QStringLiteral("window '%1' does not intersect assigned output '%2'")
              .arg(window.id, window.outputId),
          {}, window.id, window.outputId);
    }
    if (const auto error = validateWindowScope(window); error.hasError()) {
      return error;
    }
    if (window.active && (window.minimized || window.hidden)) {
      return failure(
          PanelVisibilityErrorCode::InvalidWindowState,
          QStringLiteral("active window '%1' cannot be minimized or hidden")
              .arg(window.id),
          {}, window.id, window.outputId);
    }
    if (window.active) {
      if (!activeWindowId.isEmpty()) {
        return failure(PanelVisibilityErrorCode::MultipleActiveWindows,
                       QStringLiteral("windows '%1' and '%2' are both active")
                           .arg(activeWindowId, window.id),
                       {}, window.id, window.outputId);
      }
      activeWindowId = window.id;
    }
  }
  return {};
}

PanelVisibilityError
validateInteractions(const QVector<PanelInteractionSnapshot> &interactions,
                     const QHash<QString, QSet<QString>> &panelIdsByOutput) {
  QHash<QString, QSet<QString>> seen;
  for (const auto &interaction : interactions) {
    if (!validIdentifier(interaction.identity.panelId) ||
        !validIdentifier(interaction.identity.outputId)) {
      return failure(
          PanelVisibilityErrorCode::InvalidInteractionIdentity,
          QStringLiteral("an interaction identity is empty, padded, or "
                         "malformed"),
          interaction.identity);
    }
    auto panels = panelIdsByOutput.constFind(interaction.identity.outputId);
    if (panels == panelIdsByOutput.cend() ||
        !panels->contains(interaction.identity.panelId)) {
      return failure(
          PanelVisibilityErrorCode::UnknownInteractionPanel,
          QStringLiteral("interaction references unknown panel '(%1, %2)'")
              .arg(interaction.identity.panelId, interaction.identity.outputId),
          interaction.identity);
    }
    auto &seenIds = seen[interaction.identity.outputId];
    if (seenIds.contains(interaction.identity.panelId)) {
      return failure(
          PanelVisibilityErrorCode::DuplicateInteractionIdentity,
          QStringLiteral("duplicate interaction for panel '(%1, %2)'")
              .arg(interaction.identity.panelId, interaction.identity.outputId),
          interaction.identity);
    }
    seenIds.insert(interaction.identity.panelId);
  }
  return {};
}

} // namespace

PanelVisibilityError
validateInventory(const PanelVisibilityInventory &inventory) {
  if (const auto error = validateScope(inventory.scope); error.hasError()) {
    return error;
  }

  QHash<QString, QRect> outputGeometries;
  if (const auto error = validateOutputs(inventory.outputs, &outputGeometries);
      error.hasError()) {
    return error;
  }

  QHash<QString, QSet<QString>> panelIdsByOutput;
  if (const auto error =
          validatePanels(inventory.panels, outputGeometries, &panelIdsByOutput);
      error.hasError()) {
    return error;
  }
  if (const auto error = validateWindows(inventory.windows, outputGeometries);
      error.hasError()) {
    return error;
  }
  return validateInteractions(inventory.interactions, panelIdsByOutput);
}

} // namespace QindaQt::ShellVisibility::Private
