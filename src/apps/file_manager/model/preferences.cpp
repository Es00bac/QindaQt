// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"

#include <QSet>

#include <algorithm>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] bool columnsAreValid(const QList<DetailsColumn> &columns) {
  // Name leads and is always shown; every other key at most once.
  if (columns.isEmpty() || columns.constFirst().key != QLatin1String("name") ||
      columns.size() > Preferences::columnKeys().size()) {
    return false;
  }
  const QStringList known = Preferences::columnKeys();
  QSet<QString> seen;
  for (const DetailsColumn &column : columns) {
    const bool widthOk = column.width == 0 ||
                         (column.width >= Preferences::minimumColumnWidth &&
                          column.width <= Preferences::maximumColumnWidth);
    if (!known.contains(column.key) || seen.contains(column.key) || !widthOk) {
      return false;
    }
    seen.insert(column.key);
  }
  return true;
}

[[nodiscard]] bool locationIsValid(const QString &location) {
  if (location.isEmpty() || location.toUtf8().size() > Preferences::maximumLocationBytes) {
    return false;
  }
  return std::none_of(location.cbegin(), location.cend(),
                      [](QChar character) { return character.category() == QChar::Other_Control; });
}

} // namespace

QList<DetailsColumn> FolderView::defaultColumns() {
  return {{QStringLiteral("name"), 0},
          {QStringLiteral("size"), 0},
          {QStringLiteral("kind"), 0},
          {QStringLiteral("modified"), 0}};
}

bool FolderView::isValid() const {
  return Preferences::viewModes().contains(viewMode) &&
         Preferences::sortColumns().contains(sortColumn) &&
         Preferences::sortDirections().contains(sortDirection) &&
         Preferences::groupKeys().contains(groupBy) &&
         Preferences::iconSizes().contains(iconSize) && columnsAreValid(columns);
}

// AGENT-CONTRACT: the same four modes NavigationController::setViewMode()
// accepts. "list" and "grid" are the historical names of Details and Icons.
QStringList Preferences::viewModes() {
  return {QStringLiteral("list"), QStringLiteral("grid"), QStringLiteral("columns"),
          QStringLiteral("gallery")};
}

// AGENT-CONTRACT: the keys ListingOrder sorts by (listing_order.cpp).
QStringList Preferences::sortColumns() {
  return {QStringLiteral("name"),     QStringLiteral("size"),      QStringLiteral("kind"),
          QStringLiteral("modified"), QStringLiteral("created"),   QStringLiteral("accessed"),
          QStringLiteral("extension"), QStringLiteral("path"),     QStringLiteral("permissions")};
}

QStringList Preferences::sortDirections() {
  return {QStringLiteral("ascending"), QStringLiteral("descending")};
}

// AGENT-CONTRACT: the same ladder NavigationController::zoomBy() steps
// through. A saved size outside it is refused rather than snapped, so a
// hand-edited file cannot put the view at a size the zoom controls can never
// return to.
QList<int> Preferences::iconSizes() { return {16, 24, 32, 48, 64, 96, 128}; }

// AGENT-CONTRACT: ListingOrder's entryGroupKey() values.
QStringList Preferences::groupKeys() {
  return {QStringLiteral("none"), QStringLiteral("kind"), QStringLiteral("date"),
          QStringLiteral("size")};
}

// AGENT-CONTRACT: the Details view's column keys (ui/DetailsColumnSet.qml).
QStringList Preferences::columnKeys() {
  return {QStringLiteral("name"),     QStringLiteral("size"),        QStringLiteral("kind"),
          QStringLiteral("modified"), QStringLiteral("created"),     QStringLiteral("accessed"),
          QStringLiteral("permissions"), QStringLiteral("owner"),    QStringLiteral("group"),
          QStringLiteral("extension"), QStringLiteral("path"),       QStringLiteral("items"),
          QStringLiteral("dimensions")};
}

QStringList Preferences::rowDensities() {
  return {QStringLiteral("comfortable"), QStringLiteral("compact")};
}

QStringList Preferences::fileManagerStyles() {
  return {QStringLiteral("finder"), QStringLiteral("explorer"), QStringLiteral("commander")};
}

QString Preferences::styleViewMode(const QString &style) {
  return style == QLatin1String("explorer") || style == QLatin1String("commander")
             ? QStringLiteral("list")
             : QStringLiteral("grid");
}

bool Preferences::isValid() const {
  const bool stylesValid =
      (fileManagerStyle.isEmpty() || fileManagerStyles().contains(fileManagerStyle)) &&
      fileManagerStyles().contains(layoutStyle);
  if (!stylesValid || !defaultFolderView().isValid() || !rowDensities().contains(rowDensity) ||
      folderViews.size() > maximumFolderViews ||
      (defaultConnectScheme != QLatin1String("sftp") &&
       defaultConnectScheme != QLatin1String("smb"))) {
    return false;
  }
  QSet<QString> locations;
  for (const RememberedFolderView &remembered : folderViews) {
    if (!locationIsValid(remembered.location) || locations.contains(remembered.location) ||
        !remembered.view.isValid()) {
      return false;
    }
    locations.insert(remembered.location);
  }
  return true;
}

FolderView Preferences::defaultFolderView() const {
  return {.viewMode = defaultViewMode,
          .sortColumn = sortColumn,
          .sortDirection = sortDirection,
          .groupBy = groupBy,
          .iconSize = iconSize,
          .columns = detailsColumns};
}

void Preferences::setDefaultFolderView(const FolderView &view) {
  defaultViewMode = view.viewMode;
  sortColumn = view.sortColumn;
  sortDirection = view.sortDirection;
  groupBy = view.groupBy;
  iconSize = view.iconSize;
  detailsColumns = view.columns;
}

FolderView Preferences::folderViewFor(const QString &location) const {
  for (const RememberedFolderView &remembered : folderViews) {
    if (remembered.location == location) {
      return remembered.view;
    }
  }
  return defaultFolderView();
}

bool Preferences::remembers(const QString &location) const {
  return std::any_of(folderViews.cbegin(), folderViews.cend(),
                     [&location](const RememberedFolderView &remembered) {
                       return remembered.location == location;
                     });
}

void Preferences::rememberFolderView(const QString &location, const FolderView &view) {
  folderViews.removeIf([&location](const RememberedFolderView &remembered) {
    return remembered.location == location;
  });
  if (view == defaultFolderView()) {
    return;
  }
  folderViews.prepend({location, view});
  while (folderViews.size() > maximumFolderViews) {
    folderViews.removeLast();
  }
}

} // namespace QindaQt::Apps::FileManager
