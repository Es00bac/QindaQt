// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/listing_order.h"

#include <QDateTime>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] DirectoryEntry makeEntry(const QString &name) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = QStringLiteral("/fixture/%1").arg(name);
  return entry;
}

[[nodiscard]] QStringList namesOf(const QVector<DirectoryEntry> &entries) {
  QStringList names;
  for (const DirectoryEntry &entry : entries) {
    names.append(entry.name);
  }
  return names;
}

[[nodiscard]] QStringList sorted(QVector<DirectoryEntry> entries, const ListingOrder &order,
                                 const QDate &today = QDate(2026, 9, 24)) {
  sortListing(entries, order, today);
  return namesOf(entries);
}

[[nodiscard]] QDateTime at(const QDate &day) {
  return QDateTime(day, QTime(12, 0));
}

} // namespace

// ADR-0270: the Details view's extra sort columns and Group By, which is part
// of the listing's order so every view shares one index space.
class TestListingGroups final : public QObject {
  Q_OBJECT

private slots:
  void newColumnAndGroupKeysRoundTrip();
  void createdAndAccessedSortLikeModified();
  void extensionPathAndPermissionsSort();
  void kindGroupsPutFoldersFirstThenKindsAtoZ();
  void dateGroupsFollowTheCalendar();
  void sizeGroupsRunLargestFirst();
  void groupedOrderKeepsTheSortWithinEachGroup();
  void noGroupIsThePlainSort();
};

void TestListingGroups::newColumnAndGroupKeysRoundTrip() {
  for (const SortColumn column : {SortColumn::Created, SortColumn::Accessed,
                                  SortColumn::Extension, SortColumn::Path,
                                  SortColumn::Permissions}) {
    bool ok = false;
    QCOMPARE(sortColumnFromKey(sortColumnKey(column), &ok), column);
    QVERIFY(ok);
  }
  QCOMPARE(sortColumnKey(SortColumn::Created), QStringLiteral("created"));
  QCOMPARE(sortColumnKey(SortColumn::Permissions), QStringLiteral("permissions"));
  // Lazily read columns are never sort keys: sorting needs every row.
  for (const QString &key : {QStringLiteral("owner"), QStringLiteral("items"),
                             QStringLiteral("dimensions")}) {
    bool ok = true;
    QCOMPARE(sortColumnFromKey(key, &ok), SortColumn::Name);
    QVERIFY2(!ok, qPrintable(key));
  }
  for (const EntryGroup group : {EntryGroup::None, EntryGroup::Kind, EntryGroup::Date,
                                 EntryGroup::Size}) {
    bool ok = false;
    QCOMPARE(entryGroupFromKey(entryGroupKey(group), &ok), group);
    QVERIFY(ok);
  }
  bool ok = true;
  QCOMPARE(entryGroupFromKey(QStringLiteral("colour"), &ok), EntryGroup::None);
  QVERIFY(!ok);
}

void TestListingGroups::createdAndAccessedSortLikeModified() {
  auto early = makeEntry(QStringLiteral("early"));
  early.created = at(QDate(2026, 1, 1));
  early.accessed = at(QDate(2026, 9, 1));
  auto late = makeEntry(QStringLiteral("late"));
  late.created = at(QDate(2026, 2, 1));
  late.accessed = at(QDate(2026, 3, 1));
  const QVector<DirectoryEntry> entries = {late, early};
  QCOMPARE(sorted(entries, {.column = SortColumn::Created}),
           (QStringList{QStringLiteral("early"), QStringLiteral("late")}));
  QCOMPARE(sorted(entries, {.column = SortColumn::Accessed}),
           (QStringList{QStringLiteral("late"), QStringLiteral("early")}));
  QCOMPARE(sorted(entries, {.column = SortColumn::Created,
                            .direction = SortDirection::Descending}),
           (QStringList{QStringLiteral("late"), QStringLiteral("early")}));
}

void TestListingGroups::extensionPathAndPermissionsSort() {
  auto text = makeEntry(QStringLiteral("b.txt"));
  auto image = makeEntry(QStringLiteral("a.png"));
  // A dot file has no extension (ui/EntryText.js shows none either).
  auto dotFile = makeEntry(QStringLiteral(".zshrc"));
  const QVector<DirectoryEntry> entries = {text, image, dotFile};
  QCOMPARE(sorted(entries, {.column = SortColumn::Extension}),
           (QStringList{QStringLiteral(".zshrc"), QStringLiteral("a.png"),
                        QStringLiteral("b.txt")}));

  auto deep = makeEntry(QStringLiteral("same"));
  deep.absolutePath = QStringLiteral("/fixture/z/same");
  auto shallow = makeEntry(QStringLiteral("same"));
  shallow.absolutePath = QStringLiteral("/fixture/a/same");
  QCOMPARE(sorted({deep, shallow}, {.column = SortColumn::Path}).size(), 2);
  QVector<DirectoryEntry> byPath = {deep, shallow};
  sortListing(byPath, {.column = SortColumn::Path}, QDate(2026, 9, 24));
  QCOMPARE(byPath.first().absolutePath, QStringLiteral("/fixture/a/same"));

  // Permission bits only: a directory's type bit does not decide the order.
  auto open = makeEntry(QStringLiteral("open"));
  open.mode = 0100755U;
  auto closed = makeEntry(QStringLiteral("closed"));
  closed.mode = 0100600U;
  QCOMPARE(sorted({open, closed}, {.column = SortColumn::Permissions}),
           (QStringList{QStringLiteral("closed"), QStringLiteral("open")}));
}

void TestListingGroups::kindGroupsPutFoldersFirstThenKindsAtoZ() {
  auto folder = makeEntry(QStringLiteral("Folder"));
  folder.isDirectory = true;
  const QDate today(2026, 9, 24);
  QCOMPARE(entryGroupFor(folder, EntryGroup::Kind, today).rank, 0);
  QCOMPARE(entryGroupFor(folder, EntryGroup::Kind, today).label, QStringLiteral("Folder"));
  QCOMPARE(entryGroupFor(makeEntry(QStringLiteral("a.png")), EntryGroup::Kind, today).label,
           QStringLiteral("PNG File"));
  // ADR-0262: a row that names its own kind (an application's category)
  // groups under that name.
  auto application = makeEntry(QStringLiteral("Editor"));
  application.kindText = QStringLiteral("Development");
  QCOMPARE(entryGroupFor(application, EntryGroup::Kind, today).label,
           QStringLiteral("Development"));

  const QVector<DirectoryEntry> entries = {makeEntry(QStringLiteral("z.txt")),
                                           makeEntry(QStringLiteral("b.png")), folder,
                                           makeEntry(QStringLiteral("a.txt"))};
  QCOMPARE(sorted(entries, {.group = EntryGroup::Kind}),
           (QStringList{QStringLiteral("Folder"), QStringLiteral("b.png"),
                        QStringLiteral("a.txt"), QStringLiteral("z.txt")}));
}

void TestListingGroups::dateGroupsFollowTheCalendar() {
  const QDate today(2026, 9, 24);
  const auto labelFor = [&today](const QDate &day) {
    auto entry = makeEntry(QStringLiteral("x"));
    entry.lastModified = at(day);
    return entryGroupFor(entry, EntryGroup::Date, today).label;
  };
  QCOMPARE(labelFor(today), QStringLiteral("Today"));
  QCOMPARE(labelFor(today.addDays(-1)), QStringLiteral("Yesterday"));
  QCOMPARE(labelFor(today.addDays(-5)), QStringLiteral("Previous 7 Days"));
  QCOMPARE(labelFor(today.addDays(-20)), QStringLiteral("Previous 30 Days"));
  QCOMPARE(labelFor(QDate(2026, 2, 1)), QStringLiteral("Earlier This Year"));
  QCOMPARE(labelFor(QDate(2023, 6, 1)), QStringLiteral("2023"));
  QCOMPARE(labelFor(today.addDays(3)), QStringLiteral("Future Dates"));
  // Unknown is its own group, last -- never "long ago".
  const DirectoryEntry unknown = makeEntry(QStringLiteral("unknown"));
  QCOMPARE(entryGroupFor(unknown, EntryGroup::Date, today).label, QStringLiteral("No Date"));

  auto recent = makeEntry(QStringLiteral("recent"));
  recent.lastModified = at(today);
  auto old = makeEntry(QStringLiteral("old"));
  old.lastModified = at(QDate(2023, 6, 1));
  auto older = makeEntry(QStringLiteral("older"));
  older.lastModified = at(QDate(2021, 6, 1));
  QCOMPARE(sorted({older, unknown, old, recent}, {.group = EntryGroup::Date}, today),
           (QStringList{QStringLiteral("recent"), QStringLiteral("old"),
                        QStringLiteral("older"), QStringLiteral("unknown")}));
}

void TestListingGroups::sizeGroupsRunLargestFirst() {
  const QDate today(2026, 9, 24);
  const auto sized = [](const QString &name, qint64 size) {
    auto entry = makeEntry(name);
    entry.size = size;
    return entry;
  };
  auto folder = makeEntry(QStringLiteral("folder"));
  folder.isDirectory = true;
  const QVector<DirectoryEntry> entries = {
      sized(QStringLiteral("empty"), 0), sized(QStringLiteral("tiny"), 10),
      sized(QStringLiteral("huge"), qint64(2) * 1024 * 1024 * 1024), folder,
      sized(QStringLiteral("medium"), 5 * 1024 * 1024)};
  QCOMPARE(entryGroupFor(entries.at(2), EntryGroup::Size, today).label,
           QStringLiteral("1 GiB or More"));
  QCOMPARE(sorted(entries, {.group = EntryGroup::Size}),
           (QStringList{QStringLiteral("folder"), QStringLiteral("huge"),
                        QStringLiteral("medium"), QStringLiteral("tiny"),
                        QStringLiteral("empty")}));
}

void TestListingGroups::groupedOrderKeepsTheSortWithinEachGroup() {
  // Group by Kind, sort by Name descending: groups keep their own order and
  // the names reverse inside each.
  const QVector<DirectoryEntry> entries = {
      makeEntry(QStringLiteral("a.txt")), makeEntry(QStringLiteral("b.png")),
      makeEntry(QStringLiteral("c.txt")), makeEntry(QStringLiteral("a.png"))};
  QCOMPARE(sorted(entries, {.direction = SortDirection::Descending, .group = EntryGroup::Kind}),
           (QStringList{QStringLiteral("b.png"), QStringLiteral("a.png"),
                        QStringLiteral("c.txt"), QStringLiteral("a.txt")}));
}

void TestListingGroups::noGroupIsThePlainSort() {
  auto folder = makeEntry(QStringLiteral("zed"));
  folder.isDirectory = true;
  QCOMPARE(sorted({makeEntry(QStringLiteral("b")), folder, makeEntry(QStringLiteral("a"))},
                  ListingOrder{}),
           (QStringList{QStringLiteral("zed"), QStringLiteral("a"), QStringLiteral("b")}));
  QCOMPARE(entryGroupFor(folder, EntryGroup::None, QDate(2026, 9, 24)).label, QString());
}

QTEST_APPLESS_MAIN(TestListingGroups)
#include "tst_listing_groups.moc"
