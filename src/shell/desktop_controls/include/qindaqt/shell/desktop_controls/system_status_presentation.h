// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace QindaQt::Shell::DesktopControls {

// One service lane as observed from an existing applet facade. `granted` is
// this applet's own manifest grant for the lane; `present` says whether the
// facade object exists at all. Phase strings are the facades' own phase
// vocabulary (loading | ready | degraded | unavailable | empty).
struct StatusLaneInput {
  QString id;
  QString label;
  bool granted = false;
  bool present = false;
  QString phase;
  QString summary;
  QString iconName;
  QString accessibleName;
  QString accessibleDescription;
  bool attention = false;

  friend bool operator==(const StatusLaneInput &, const StatusLaneInput &) =
      default;
};

struct StatusLaneRow {
  QString id;
  QString label;
  QString phase;
  QString summary;
  QString iconName;
  QString accessibleName;
  QString accessibleDescription;
  bool available = false;
  bool attention = false;

  friend bool operator==(const StatusLaneRow &, const StatusLaneRow &) = default;
};

struct SystemStatusModel {
  QList<StatusLaneRow> lanes;
  int availableLaneCount = 0;
  QString accessibleName;
  QString accessibleDescription;

  friend bool operator==(const SystemStatusModel &, const SystemStatusModel &) =
      default;
};

// Pure, deterministic projection. Lanes that are not granted or whose facade
// is absent are omitted entirely (never rendered as a fake indicator); lanes
// in ready or degraded phase count as available. Input order is preserved.
[[nodiscard]] SystemStatusModel projectSystemStatus(
    const QList<StatusLaneInput> &lanes);

} // namespace QindaQt::Shell::DesktopControls
