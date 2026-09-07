// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include <QString>

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
};

enum class SortDirection {
  Ascending,
  Descending,
};

struct ListingOrder final {
  SortColumn column = SortColumn::Name;
  SortDirection direction = SortDirection::Ascending;
  bool directoriesFirst = true;

  [[nodiscard]] bool operator==(const ListingOrder &) const = default;
};

// Stable string keys cross the QML boundary and appear in tests; never
// reorder or rename existing keys.
[[nodiscard]] QString sortColumnKey(SortColumn column);
[[nodiscard]] SortColumn sortColumnFromKey(const QString &key,
                                           bool *ok = nullptr);

// Strict weak ordering for DirectoryEntry under one ListingOrder. The
// direction applies only to the requested column comparison; the name
// tiebreak always stays ascending so equal entries never flip relative order
// when the direction changes.
[[nodiscard]] bool listingEntryLessThan(const DirectoryEntry &a,
                                        const DirectoryEntry &b,
                                        const ListingOrder &order);

} // namespace QindaQt::Apps::FileManager
