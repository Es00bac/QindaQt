// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include <QDate>
#include <QString>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// AGENT-NOTE: Sorting is a pure policy so the lister, the controller, and the
// QML header row all share one tested ordering rule. The default-constructed
// ListingOrder reproduces the pre-S2 hard-coded order (directories first,
// case-insensitive name with a case-sensitive tiebreak) exactly; existing
// callers and tests rely on that default.
enum class SortColumn {
  Name,
  Size,
  Kind,
  Modified,
  // ADR-0270: the Details view's other listing-time columns. Columns whose
  // values are fetched lazily for visible rows only (owner, group, items,
  // dimensions) are deliberately not sortable: sorting needs every row.
  Created,
  Accessed,
  Extension,
  Path,
  Permissions,
};

enum class SortDirection {
  Ascending,
  Descending,
};

// ADR-0270: Group By. Grouping is part of the order -- every view shares the
// controller's one index space -- so groups are contiguous and the Details
// view only has to draw a heading where the label changes.
enum class EntryGroup {
  None,
  Kind,
  Date,
  Size,
};

struct ListingOrder final {
  SortColumn column = SortColumn::Name;
  SortDirection direction = SortDirection::Ascending;
  bool directoriesFirst = true;
  EntryGroup group = EntryGroup::None;

  [[nodiscard]] bool operator==(const ListingOrder &) const = default;
};

// Stable string keys cross the QML boundary and appear in tests; never
// reorder or rename existing keys.
[[nodiscard]] QString sortColumnKey(SortColumn column);
[[nodiscard]] SortColumn sortColumnFromKey(const QString &key,
                                           bool *ok = nullptr);
[[nodiscard]] QString entryGroupKey(EntryGroup group);
[[nodiscard]] EntryGroup entryGroupFromKey(const QString &key, bool *ok = nullptr);

// Strict weak ordering for DirectoryEntry under one ListingOrder, ignoring
// `group`. The direction applies only to the requested column comparison;
// the name tiebreak always stays ascending so equal entries never flip
// relative order when the direction changes.
[[nodiscard]] bool listingEntryLessThan(const DirectoryEntry &a,
                                        const DirectoryEntry &b,
                                        const ListingOrder &order);

// Which group an entry belongs to: groups order by `rank`, then by `label`
// (case-insensitively); the label is the heading the Details view draws.
// `today` anchors the date buckets (Today, Yesterday, ...). EntryGroup::None
// yields rank 0 and an empty label.
struct EntryGroupLabel final {
  int rank = 0;
  QString label;
};
[[nodiscard]] EntryGroupLabel entryGroupFor(const DirectoryEntry &entry, EntryGroup group,
                                            const QDate &today);

// Sorts `entries` in place: group-major when order.group is set, then by
// listingEntryLessThan within each group.
void sortListing(QVector<DirectoryEntry> &entries, const ListingOrder &order,
                 const QDate &today);

} // namespace QindaQt::Apps::FileManager
