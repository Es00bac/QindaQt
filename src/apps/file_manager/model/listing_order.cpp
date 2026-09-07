// SPDX-License-Identifier: GPL-3.0-or-later
#include "listing_order.h"

#include <QFileInfo>

namespace QindaQt::Apps::FileManager {

namespace {

// Name ordering never depends on filesystem enumeration order: ties break
// case-insensitively, then case-sensitively.
[[nodiscard]] int compareNames(const DirectoryEntry &a, const DirectoryEntry &b) {
  const int caseInsensitive = a.name.compare(b.name, Qt::CaseInsensitive);
  if (caseInsensitive != 0) {
    return caseInsensitive;
  }
  return a.name.compare(b.name, Qt::CaseSensitive);
}

// Kind ranks directories above symlinks above plain files; same-rank entries
// compare by extension so grouping is stable and human-readable.
[[nodiscard]] int kindRank(const DirectoryEntry &entry) {
  if (entry.isDirectory) {
    return 0;
  }
  if (entry.isSymlink) {
    return 1;
  }
  return 2;
}

[[nodiscard]] int compareColumn(const DirectoryEntry &a, const DirectoryEntry &b,
                                SortColumn column) {
  switch (column) {
  case SortColumn::Name:
    return compareNames(a, b);
  case SortColumn::Size:
    if (a.size != b.size) {
      return a.size < b.size ? -1 : 1;
    }
    return 0;
  case SortColumn::Kind: {
    const int rankA = kindRank(a);
    const int rankB = kindRank(b);
    if (rankA != rankB) {
      return rankA < rankB ? -1 : 1;
    }
    const QString suffixA = QFileInfo(a.name).suffix().toLower();
    const QString suffixB = QFileInfo(b.name).suffix().toLower();
    if (suffixA != suffixB) {
      return suffixA < suffixB ? -1 : 1;
    }
    return 0;
  }
  case SortColumn::Modified:
    if (a.lastModified != b.lastModified) {
      return a.lastModified < b.lastModified ? -1 : 1;
    }
    return 0;
  }
  return 0;
}

} // namespace

QString sortColumnKey(SortColumn column) {
  switch (column) {
  case SortColumn::Name:
    return QStringLiteral("name");
  case SortColumn::Size:
    return QStringLiteral("size");
  case SortColumn::Kind:
    return QStringLiteral("kind");
  case SortColumn::Modified:
    return QStringLiteral("modified");
  }
  return QStringLiteral("name");
}

SortColumn sortColumnFromKey(const QString &key, bool *ok) {
  if (ok != nullptr) {
    *ok = true;
  }
  if (key == QStringLiteral("size")) {
    return SortColumn::Size;
  }
  if (key == QStringLiteral("kind")) {
    return SortColumn::Kind;
  }
  if (key == QStringLiteral("modified")) {
    return SortColumn::Modified;
  }
  if (key != QStringLiteral("name") && ok != nullptr) {
    *ok = false;
  }
  return SortColumn::Name;
}

bool listingEntryLessThan(const DirectoryEntry &a, const DirectoryEntry &b,
                          const ListingOrder &order) {
  if (order.directoriesFirst && a.isDirectory != b.isDirectory) {
    return a.isDirectory;
  }
  const int column = compareColumn(a, b, order.column);
  if (column != 0) {
    const bool ascending = order.direction == SortDirection::Ascending;
    return ascending ? column < 0 : column > 0;
  }
  if (order.column != SortColumn::Name) {
    return compareNames(a, b) < 0;
  }
  return false;
}

} // namespace QindaQt::Apps::FileManager
