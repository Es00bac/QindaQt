// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"

namespace QindaQt::Apps::FileManager {

QStringList Preferences::viewModes() {
  return {QStringLiteral("list"), QStringLiteral("grid")};
}

QStringList Preferences::sortColumns() {
  return {QStringLiteral("name"), QStringLiteral("size"), QStringLiteral("kind"),
          QStringLiteral("modified")};
}

QStringList Preferences::sortDirections() {
  return {QStringLiteral("ascending"), QStringLiteral("descending")};
}

// AGENT-CONTRACT: the same ladder NavigationController::zoomBy() steps
// through. A saved size outside it is refused rather than snapped, so a
// hand-edited file cannot put the view at a size the zoom controls can never
// return to.
QList<int> Preferences::iconSizes() { return {16, 24, 32, 48, 64, 96, 128}; }

bool Preferences::isValid() const {
  return viewModes().contains(defaultViewMode) && sortColumns().contains(sortColumn) &&
         sortDirections().contains(sortDirection) && iconSizes().contains(iconSize) &&
         (defaultConnectScheme == QLatin1String("sftp") ||
          defaultConnectScheme == QLatin1String("smb"));
}

} // namespace QindaQt::Apps::FileManager
