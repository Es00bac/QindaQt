// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/system_status_presentation.h"

#include <QStringList>

namespace QindaQt::Shell::DesktopControls {

SystemStatusModel projectSystemStatus(const QList<StatusLaneInput> &lanes)
{
  SystemStatusModel model;
  QStringList availableSummaries;
  QStringList unavailableLabels;
  for (const StatusLaneInput &lane : lanes) {
    if (!lane.granted || !lane.present) {
      continue;
    }
    StatusLaneRow row;
    row.id = lane.id;
    row.label = lane.label;
    row.phase = lane.phase;
    row.summary = lane.summary;
    row.iconName = lane.iconName;
    row.accessibleName = lane.accessibleName.isEmpty() ? lane.label
                                                       : lane.accessibleName;
    row.accessibleDescription = lane.accessibleDescription;
    row.available = lane.phase == QLatin1StringView("ready")
        || lane.phase == QLatin1StringView("degraded");
    row.attention = lane.attention;
    if (row.available) {
      ++model.availableLaneCount;
      availableSummaries.append(row.summary.isEmpty() ? row.label : row.summary);
    } else {
      unavailableLabels.append(row.label);
    }
    model.lanes.append(row);
  }
  if (model.lanes.isEmpty()) {
    model.accessibleName = QStringLiteral("System status");
    model.accessibleDescription =
        QStringLiteral("No system status lanes are granted to this applet");
    return model;
  }
  model.accessibleName = availableSummaries.isEmpty()
      ? QStringLiteral("System status")
      : QStringLiteral("System status: %1").arg(availableSummaries.join(QStringLiteral(", ")));
  model.accessibleDescription = unavailableLabels.isEmpty()
      ? QStringLiteral("Opens quick controls for %1 services").arg(model.lanes.size())
      : QStringLiteral("Unavailable: %1").arg(unavailableLabels.join(QStringLiteral(", ")));
  return model;
}

} // namespace QindaQt::Shell::DesktopControls
