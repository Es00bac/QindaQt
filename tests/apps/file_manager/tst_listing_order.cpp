// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/listing_order.h"

#include <QDateTime>
#include <QTest>

#include <algorithm>

using QindaQt::Apps::FileManager::DirectoryEntry;
using QindaQt::Apps::FileManager::ListingOrder;
using QindaQt::Apps::FileManager::SortColumn;
using QindaQt::Apps::FileManager::SortDirection;
using QindaQt::Apps::FileManager::listingEntryLessThan;
using QindaQt::Apps::FileManager::sortColumnFromKey;
using QindaQt::Apps::FileManager::sortColumnKey;

namespace {

[[nodiscard]] DirectoryEntry makeEntry(const QString &name) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = QStringLiteral("/fixture/%1").arg(name);
  return entry;
}

[[nodiscard]] QStringList sortedNames(QVector<DirectoryEntry> entries,
                                      const ListingOrder &order) {
  std::sort(entries.begin(), entries.end(), [&order](const auto &a, const auto &b) {
    return listingEntryLessThan(a, b, order);
  });
  QStringList names;
  names.reserve(entries.size());
  for (const auto &entry : entries) {
    names.append(entry.name);
  }
  return names;
}

} // namespace

class TestListingOrder final : public QObject {
  Q_OBJECT

private slots:
  void columnKeysRoundTripAndRejectUnknownKeys();
  void defaultOrderReproducesTheLegacyHardCodedOrder();
  void nameColumnHonoursDirection();
  void sizeColumnHonoursDirection();
  void kindColumnRanksDirectoriesSymlinksThenFilesBySuffix();
  void modifiedColumnHonoursDirection();
  void invalidModifiedDatesTieAndFallBackToNames();
  void directionNeverInvertsTheNameTiebreak();
  void directoriesFirstCanBeDisabled();
};

void TestListingOrder::columnKeysRoundTripAndRejectUnknownKeys() {
  for (const SortColumn column :
       {SortColumn::Name, SortColumn::Size, SortColumn::Kind, SortColumn::Modified}) {
    bool ok = false;
    QCOMPARE(sortColumnFromKey(sortColumnKey(column), &ok), column);
    QVERIFY(ok);
  }
  QCOMPARE(sortColumnKey(SortColumn::Name), QStringLiteral("name"));
  QCOMPARE(sortColumnKey(SortColumn::Size), QStringLiteral("size"));
  QCOMPARE(sortColumnKey(SortColumn::Kind), QStringLiteral("kind"));
  QCOMPARE(sortColumnKey(SortColumn::Modified), QStringLiteral("modified"));

  // AGENT-CONTRACT: Unknown keys must report ok=false and fall back to Name;
  // NavigationController::setSortColumn and the QML header row rely on this to
  // ignore stale or hostile column keys without reordering the listing.
  for (const QString &key : {QStringLiteral("bogus"), QStringLiteral("Name"),
                             QStringLiteral("SIZE"), QString()}) {
    bool ok = true;
    QCOMPARE(sortColumnFromKey(key, &ok), SortColumn::Name);
    QVERIFY2(!ok, qPrintable(QStringLiteral("key %1 must be rejected").arg(key)));
  }
  // The nullptr out-parameter overload stays usable.
  QCOMPARE(sortColumnFromKey(QStringLiteral("size")), SortColumn::Size);
  QCOMPARE(sortColumnFromKey(QStringLiteral("bogus")), SortColumn::Name);
}

void TestListingOrder::defaultOrderReproducesTheLegacyHardCodedOrder() {
  // AGENT-GUARD: A default-constructed ListingOrder is the pre-S2 listing
  // order: directories first, then case-insensitive names, then a
  // case-sensitive tiebreak. NavigationController constructs exactly this
  // default; changing it silently re-orders every existing folder view.
  QVector<DirectoryEntry> entries;
  auto dir = makeEntry(QStringLiteral("zed"));
  dir.isDirectory = true;
  entries.append(makeEntry(QStringLiteral("Beta")));
  entries.append(makeEntry(QStringLiteral("alpha")));
  entries.append(makeEntry(QStringLiteral("ALPHA")));
  entries.append(dir);

  QCOMPARE(sortedNames(entries, ListingOrder{}),
           (QStringList{QStringLiteral("zed"), QStringLiteral("ALPHA"),
                        QStringLiteral("alpha"), QStringLiteral("Beta")}));

  // The comparator is a strict weak ordering: irreflexive and antisymmetric.
  const DirectoryEntry a = makeEntry(QStringLiteral("alpha"));
  QVERIFY(!listingEntryLessThan(a, a, ListingOrder{}));
  QVERIFY(listingEntryLessThan(a, makeEntry(QStringLiteral("Beta")), ListingOrder{}));
  QVERIFY(!listingEntryLessThan(makeEntry(QStringLiteral("Beta")), a, ListingOrder{}));
}

void TestListingOrder::nameColumnHonoursDirection() {
  const QVector<DirectoryEntry> entries = {makeEntry(QStringLiteral("beta")),
                                           makeEntry(QStringLiteral("alpha")),
                                           makeEntry(QStringLiteral("gamma"))};
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Name,
                                 .direction = SortDirection::Ascending}),
           (QStringList{QStringLiteral("alpha"), QStringLiteral("beta"),
                        QStringLiteral("gamma")}));
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Name,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("gamma"), QStringLiteral("beta"),
                        QStringLiteral("alpha")}));
}

void TestListingOrder::sizeColumnHonoursDirection() {
  auto small = makeEntry(QStringLiteral("small"));
  small.size = 1;
  auto medium = makeEntry(QStringLiteral("medium"));
  medium.size = 3;
  auto large = makeEntry(QStringLiteral("large"));
  large.size = 5;
  const QVector<DirectoryEntry> entries = {medium, large, small};

  QCOMPARE(sortedNames(entries, {.column = SortColumn::Size,
                                 .direction = SortDirection::Ascending}),
           (QStringList{QStringLiteral("small"), QStringLiteral("medium"),
                        QStringLiteral("large")}));
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Size,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("large"), QStringLiteral("medium"),
                        QStringLiteral("small")}));
}

void TestListingOrder::kindColumnRanksDirectoriesSymlinksThenFilesBySuffix() {
  auto file = makeEntry(QStringLiteral("notes.md"));
  auto otherFile = makeEntry(QStringLiteral("picture.png"));
  auto symlink = makeEntry(QStringLiteral("alias"));
  symlink.isSymlink = true;
  auto dir = makeEntry(QStringLiteral("folder"));
  dir.isDirectory = true;
  const QVector<DirectoryEntry> entries = {otherFile, symlink, dir, file};

  // Rank order is directory < symlink < file; same-rank files order by
  // lower-cased suffix so grouping stays stable and human-readable.
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Kind,
                                 .direction = SortDirection::Ascending}),
           (QStringList{QStringLiteral("folder"), QStringLiteral("alias"),
                        QStringLiteral("notes.md"), QStringLiteral("picture.png")}));
  // AGENT-GUARD: directoriesFirst is a group split ahead of the column
  // comparison and is not inverted by the direction; only the in-group
  // column order flips.
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Kind,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("folder"), QStringLiteral("picture.png"),
                        QStringLiteral("notes.md"), QStringLiteral("alias")}));
}

void TestListingOrder::modifiedColumnHonoursDirection() {
  const QDateTime base(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);
  auto oldest = makeEntry(QStringLiteral("oldest"));
  oldest.lastModified = base;
  auto middle = makeEntry(QStringLiteral("middle"));
  middle.lastModified = base.addDays(1);
  auto newest = makeEntry(QStringLiteral("newest"));
  newest.lastModified = base.addDays(2);
  const QVector<DirectoryEntry> entries = {middle, newest, oldest};

  QCOMPARE(sortedNames(entries, {.column = SortColumn::Modified,
                                 .direction = SortDirection::Ascending}),
           (QStringList{QStringLiteral("oldest"), QStringLiteral("middle"),
                        QStringLiteral("newest")}));
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Modified,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("newest"), QStringLiteral("middle"),
                        QStringLiteral("oldest")}));
}

void TestListingOrder::invalidModifiedDatesTieAndFallBackToNames() {
  // AGENT-NOTE: A lister can publish an invalid QDateTime (e.g. a stat
  // failure); invalid timestamps compare equal here, so the deterministic
  // ascending name tiebreak decides instead of enumeration order.
  auto b = makeEntry(QStringLiteral("b.txt"));
  b.lastModified = QDateTime();
  auto a = makeEntry(QStringLiteral("a.txt"));
  a.lastModified = QDateTime();
  const QVector<DirectoryEntry> entries = {b, a};

  QCOMPARE(sortedNames(entries, {.column = SortColumn::Modified,
                                 .direction = SortDirection::Ascending}),
           (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Modified,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
}

void TestListingOrder::directionNeverInvertsTheNameTiebreak() {
  // Equal column values always fall back to ascending names, even when the
  // requested direction is descending; otherwise equal entries would flip
  // relative order every time the user toggles a column.
  auto b = makeEntry(QStringLiteral("b.txt"));
  b.size = 7;
  auto a = makeEntry(QStringLiteral("a.txt"));
  a.size = 7;
  const QVector<DirectoryEntry> entries = {b, a};

  QCOMPARE(sortedNames(entries, {.column = SortColumn::Size,
                                 .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
}

void TestListingOrder::directoriesFirstCanBeDisabled() {
  auto dir = makeEntry(QStringLiteral("zed"));
  dir.isDirectory = true;
  auto file = makeEntry(QStringLiteral("alpha"));
  const QVector<DirectoryEntry> entries = {file, dir};

  QCOMPARE(sortedNames(entries, ListingOrder{}),
           (QStringList{QStringLiteral("zed"), QStringLiteral("alpha")}));
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Name,
                                 .direction = SortDirection::Ascending,
                                 .directoriesFirst = false}),
           (QStringList{QStringLiteral("alpha"), QStringLiteral("zed")}));
  // With the group split disabled, a descending column direction applies to
  // directories and files alike.
  QCOMPARE(sortedNames(entries, {.column = SortColumn::Name,
                                 .direction = SortDirection::Descending,
                                 .directoriesFirst = false}),
           (QStringList{QStringLiteral("zed"), QStringLiteral("alpha")}));
}

QTEST_APPLESS_MAIN(TestListingOrder)
#include "tst_listing_order.moc"
