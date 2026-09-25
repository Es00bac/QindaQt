// SPDX-License-Identifier: GPL-3.0-or-later
#include "column_listing.h"

#include "listing_order.h"
#include "preview/local_preview.h"

#include <QDate>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Apps::FileManager {

ColumnListing::ColumnListing(DirectoryListerPtr lister, QObject *parent)
    : QObject(parent), m_lister(std::move(lister)) {
  Q_ASSERT(m_lister);
}

QVariantList ColumnListing::children(const QString &path, bool showHidden,
                                     const QString &sortColumn,
                                     const QString &sortDirection,
                                     bool directoriesFirst) const {
  if (!path.startsWith(QLatin1Char('/'))) {
    return {};
  }
  const ListingResult listed = m_lister->list(path);
  if (!listed.ok()) {
    return {};
  }
  QVector<DirectoryEntry> entries;
  entries.reserve(listed.entries.size());
  for (const DirectoryEntry &entry : listed.entries) {
    if (showHidden || !entry.isHidden) {
      entries.append(entry);
    }
  }
  ListingOrder order;
  order.column = sortColumnFromKey(sortColumn);
  order.direction = sortDirection == QLatin1String("descending") ? SortDirection::Descending
                                                                 : SortDirection::Ascending;
  order.directoriesFirst = directoriesFirst;
  sortListing(entries, order, QDate::currentDate());
  QVariantList rows;
  rows.reserve(entries.size());
  for (const DirectoryEntry &entry : entries) {
    rows.append(QVariantMap{{QStringLiteral("name"), entry.name},
                            {QStringLiteral("path"), entry.absolutePath},
                            {QStringLiteral("isDirectory"), entry.isDirectory},
                            {QStringLiteral("isSymlink"), entry.isSymlink},
                            {QStringLiteral("isHidden"), entry.isHidden},
                            {QStringLiteral("iconName"), entryIconName(entry)}});
  }
  return rows;
}

} // namespace QindaQt::Apps::FileManager
