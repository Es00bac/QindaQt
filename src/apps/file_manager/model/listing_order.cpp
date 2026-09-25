// SPDX-License-Identifier: GPL-3.0-or-later
#include "listing_order.h"

#include "entry_presentation.h"

#include <QFileInfo>

#include <algorithm>
#include <limits>

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

[[nodiscard]] int sign(int value) { return value == 0 ? 0 : (value < 0 ? -1 : 1); }

template <typename T>
[[nodiscard]] int compareValues(const T &a, const T &b) {
  if (a == b) {
    return 0;
  }
  return a < b ? -1 : 1;
}

// AGENT-CONTRACT: the rule ui/EntryText.js extensionOf() shows: text after
// the last dot of a file's name; folders, applications and dot files have
// none.
[[nodiscard]] QString extensionOf(const DirectoryEntry &entry) {
  const qsizetype dot = entry.name.lastIndexOf(QLatin1Char('.'));
  if (entry.isDirectory || !entry.applicationId.isEmpty() || dot <= 0 ||
      dot == entry.name.size() - 1) {
    return {};
  }
  return entry.name.mid(dot + 1).toLower();
}

[[nodiscard]] int compareKinds(const DirectoryEntry &a, const DirectoryEntry &b) {
  const int rankA = kindRank(a);
  const int rankB = kindRank(b);
  if (rankA != rankB) {
    return rankA < rankB ? -1 : 1;
  }
  // ADR-0262: a row that names its own kind (an application's category)
  // compares by that label, which is what grouping by category sorts on.
  if (!a.kindText.isEmpty() || !b.kindText.isEmpty()) {
    return sign(a.kindText.compare(b.kindText, Qt::CaseInsensitive));
  }
  // Unlike the Extension column, a folder's dotted name counts here: this is
  // the Kind order the listing has always had.
  return compareValues(QFileInfo(a.name).suffix().toLower(), QFileInfo(b.name).suffix().toLower());
}

[[nodiscard]] int compareColumn(const DirectoryEntry &a, const DirectoryEntry &b,
                                SortColumn column) {
  switch (column) {
  case SortColumn::Name:
    return compareNames(a, b);
  case SortColumn::Size:
    return compareValues(a.size, b.size);
  case SortColumn::Kind:
    return compareKinds(a, b);
  case SortColumn::Modified:
    return compareValues(a.lastModified, b.lastModified);
  case SortColumn::Created:
    return compareValues(a.created, b.created);
  case SortColumn::Accessed:
    return compareValues(a.accessed, b.accessed);
  case SortColumn::Extension:
    return compareValues(extensionOf(a), extensionOf(b));
  case SortColumn::Path:
    return sign(a.absolutePath.compare(b.absolutePath, Qt::CaseInsensitive));
  case SortColumn::Permissions:
    // Permission bits only: the file-type bits would sort by kind instead.
    return compareValues(a.mode & 07777U, b.mode & 07777U);
  }
  return 0;
}

// AGENT-CONTRACT: one table for the key <-> enum mapping. The keys cross the
// QML boundary, live in preferences-v2 and appear in tests; append only.
struct SortColumnName final {
  SortColumn column;
  const char *key;
};
constexpr SortColumnName sortColumnNames[] = {
    {SortColumn::Name, "name"},         {SortColumn::Size, "size"},
    {SortColumn::Kind, "kind"},         {SortColumn::Modified, "modified"},
    {SortColumn::Created, "created"},   {SortColumn::Accessed, "accessed"},
    {SortColumn::Extension, "extension"}, {SortColumn::Path, "path"},
    {SortColumn::Permissions, "permissions"},
};

struct EntryGroupName final {
  EntryGroup group;
  const char *key;
};
constexpr EntryGroupName entryGroupNames[] = {
    {EntryGroup::None, "none"},
    {EntryGroup::Kind, "kind"},
    {EntryGroup::Date, "date"},
    {EntryGroup::Size, "size"},
};

[[nodiscard]] EntryGroupLabel dateGroupFor(const DirectoryEntry &entry, const QDate &today) {
  if (!entry.lastModified.isValid()) {
    // Unknown is absent, never "long ago": it sorts last under its own name.
    return {std::numeric_limits<int>::max(), QStringLiteral("No Date")};
  }
  const QDate day = entry.lastModified.toLocalTime().date();
  const qint64 age = day.daysTo(today);
  if (age < 0) {
    return {-1, QStringLiteral("Future Dates")};
  }
  if (age == 0) {
    return {0, QStringLiteral("Today")};
  }
  if (age == 1) {
    return {1, QStringLiteral("Yesterday")};
  }
  if (age <= 7) {
    return {2, QStringLiteral("Previous 7 Days")};
  }
  if (age <= 30) {
    return {3, QStringLiteral("Previous 30 Days")};
  }
  if (day.year() == today.year()) {
    return {4, QStringLiteral("Earlier This Year")};
  }
  // Older years, newest first: one heading per calendar year.
  return {5 + (today.year() - day.year()), QString::number(day.year())};
}

[[nodiscard]] EntryGroupLabel sizeGroupFor(const DirectoryEntry &entry) {
  // The same IEC units the Size column prints (QLocale::formattedDataSize).
  constexpr qint64 kibibyte = 1024;
  constexpr qint64 mebibyte = kibibyte * 1024;
  constexpr qint64 gibibyte = mebibyte * 1024;
  if (entry.isDirectory) {
    return {0, QStringLiteral("Folders")};
  }
  if (!entry.applicationId.isEmpty()) {
    return {7, QStringLiteral("No Size")};
  }
  if (entry.size >= gibibyte) {
    return {1, QStringLiteral("1 GiB or More")};
  }
  if (entry.size >= 128 * mebibyte) {
    return {2, QStringLiteral("128 MiB to 1 GiB")};
  }
  if (entry.size >= mebibyte) {
    return {3, QStringLiteral("1 MiB to 128 MiB")};
  }
  if (entry.size >= 16 * kibibyte) {
    return {4, QStringLiteral("16 KiB to 1 MiB")};
  }
  if (entry.size > 0) {
    return {5, QStringLiteral("Under 16 KiB")};
  }
  return {6, QStringLiteral("Empty")};
}

} // namespace

QString sortColumnKey(SortColumn column) {
  for (const SortColumnName &name : sortColumnNames) {
    if (name.column == column) {
      return QString::fromLatin1(name.key);
    }
  }
  return QStringLiteral("name");
}

SortColumn sortColumnFromKey(const QString &key, bool *ok) {
  for (const SortColumnName &name : sortColumnNames) {
    if (key == QLatin1String(name.key)) {
      if (ok != nullptr) {
        *ok = true;
      }
      return name.column;
    }
  }
  if (ok != nullptr) {
    *ok = false;
  }
  return SortColumn::Name;
}

QString entryGroupKey(EntryGroup group) {
  for (const EntryGroupName &name : entryGroupNames) {
    if (name.group == group) {
      return QString::fromLatin1(name.key);
    }
  }
  return QStringLiteral("none");
}

EntryGroup entryGroupFromKey(const QString &key, bool *ok) {
  for (const EntryGroupName &name : entryGroupNames) {
    if (key == QLatin1String(name.key)) {
      if (ok != nullptr) {
        *ok = true;
      }
      return name.group;
    }
  }
  if (ok != nullptr) {
    *ok = false;
  }
  return EntryGroup::None;
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

EntryGroupLabel entryGroupFor(const DirectoryEntry &entry, EntryGroup group,
                              const QDate &today) {
  switch (group) {
  case EntryGroup::None:
    return {};
  case EntryGroup::Kind:
    // Folders first, then one heading per kind label, A to Z.
    return {entry.isDirectory ? 0 : 1, EntryPresentation::kindTextFor(entry)};
  case EntryGroup::Date:
    return dateGroupFor(entry, today);
  case EntryGroup::Size:
    return sizeGroupFor(entry);
  }
  return {};
}

void sortListing(QVector<DirectoryEntry> &entries, const ListingOrder &order,
                 const QDate &today) {
  if (order.group == EntryGroup::None) {
    std::sort(entries.begin(), entries.end(),
              [&order](const DirectoryEntry &a, const DirectoryEntry &b) {
                return listingEntryLessThan(a, b, order);
              });
    return;
  }
  // Decorate once: a date bucket needs a conversion to local time, which is
  // too slow to repeat inside every comparison of a 20,000-entry sort.
  struct Keyed final {
    EntryGroupLabel group;
    qsizetype index = 0;
  };
  QVector<Keyed> keyed;
  keyed.reserve(entries.size());
  for (qsizetype i = 0; i < entries.size(); ++i) {
    keyed.append({entryGroupFor(entries.at(i), order.group, today), i});
  }
  std::sort(keyed.begin(), keyed.end(), [&entries, &order](const Keyed &a, const Keyed &b) {
    if (a.group.rank != b.group.rank) {
      return a.group.rank < b.group.rank;
    }
    const int label = a.group.label.compare(b.group.label, Qt::CaseInsensitive);
    if (label != 0) {
      return label < 0;
    }
    return listingEntryLessThan(entries.at(a.index), entries.at(b.index), order);
  });
  QVector<DirectoryEntry> sorted;
  sorted.reserve(entries.size());
  for (const Keyed &item : keyed) {
    sorted.append(std::move(entries[item.index]));
  }
  entries = std::move(sorted);
}

} // namespace QindaQt::Apps::FileManager
