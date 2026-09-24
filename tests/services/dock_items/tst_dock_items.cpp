// SPDX-License-Identifier: LGPL-3.0-or-later
// ADR-0265: the pure dock value — edits, bounds, uniqueness, the strict
// Settings1 codec, and the one-time migration from ADR-0076's id list.
#include <qindaqt/services/dock_items/dock_items.h>

#include <QtTest>

using namespace QindaQt::Services::DockItems;

namespace {

QString app(int index)
{
  return QStringLiteral("app.%1").arg(index);
}

DockItems appsDock(const QStringList &ids)
{
  DockItems dock;
  for (const QString &id : ids)
    (void)dock.insert(dock.size(), DockItem::application(id));
  return dock;
}

QVariantMap record(const QString &kind, const QVariantMap &fields = {})
{
  QVariantMap result = fields;
  result.insert(QStringLiteral("kind"), kind);
  return result;
}

QVariantMap document(const QVariantList &items, const QVariant &version = qint64(1))
{
  return {{QStringLiteral("version"), version}, {QStringLiteral("items"), items}};
}

} // namespace

class DockItemsTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void insertsEveryKindOnceAtTheGap();
  void refusesMalformedItems();
  void enforcesItemApplicationAndGroupBounds();
  void movesToGapsIncludingNoOps();
  void groupsCombineRenameUngroupAndEmpty();
  void externalApplicationsJoinGroups();
  void membersMoveOutAtTheGap();
  void insertAllSkipsDuplicatesAtomically();
  void codecRoundTripsEveryKind();
  void codecRejectsHostileValuesWholesale_data();
  void codecRejectsHostileValuesWholesale();
  void absentAndDefaultValuesAreUnmigrated();
  void legacyPinnedListMigratesOnce();
  void snapshotValuesPreferTheMigratedDock();
};

void DockItemsTests::insertsEveryKindOnceAtTheGap()
{
  DockItems dock = appsDock({app(1), app(2)});
  QCOMPARE(dock.insert(1, DockItem::folder(QStringLiteral("/home/u/Documents"))),
           DockEditError::None);
  QCOMPARE(dock.insert(0, DockItem::file(QStringLiteral("/home/u/notes.txt"))),
           DockEditError::None);
  QCOMPARE(dock.insert(dock.size(), DockItem::trash()), DockEditError::None);
  QCOMPARE(dock.size(), 5);
  QCOMPARE(dock.items().at(0).kind, DockItemKind::File);
  QCOMPARE(dock.items().at(2).kind, DockItemKind::Folder);
  QCOMPARE(dock.applicationIds(), QStringList({app(1), app(2)}));

  // One of each: an application, a path, and the Trash appear once.
  QCOMPARE(dock.insert(0, DockItem::application(app(2))), DockEditError::AlreadyInDock);
  QCOMPARE(dock.insert(0, DockItem::folder(QStringLiteral("/home/u/Documents"))),
           DockEditError::AlreadyInDock);
  QCOMPARE(dock.insert(0, DockItem::trash()), DockEditError::AlreadyInDock);
  QCOMPARE(dock.insert(-1, DockItem::application(app(9))), DockEditError::OutOfRange);
  QCOMPARE(dock.insert(dock.size() + 1, DockItem::application(app(9))),
           DockEditError::OutOfRange);
  QCOMPARE(dock.size(), 5);
  QCOMPARE(dock.indexOfTrash(), 4);
  QCOMPARE(dock.indexOfPath(QStringLiteral("/home/u/notes.txt")), 0);
}

void DockItemsTests::refusesMalformedItems()
{
  DockItems dock;
  // Paths are data: absolute, clean, bounded, one line.
  for (const QString &path : {QString{}, QStringLiteral("relative/file"),
                              QStringLiteral("/home/u/../etc"), QStringLiteral("/home//u"),
                              QStringLiteral("/home/u/"), QStringLiteral("/home/u/\nx"),
                              QStringLiteral("/") + QString(Bounds::maxPathLength, QLatin1Char('a'))}) {
    QCOMPARE(dock.insert(0, DockItem::folder(path)), DockEditError::InvalidItem);
  }
  QCOMPARE(dock.insert(0, DockItem::application(QString{})), DockEditError::InvalidItem);
  QCOMPARE(dock.insert(0, DockItem::application(QString(300, QLatin1Char('x')))),
           DockEditError::InvalidItem);
  QCOMPARE(dock.insert(0, DockItem::group(QStringLiteral("Office"), {})),
           DockEditError::InvalidItem);
  QCOMPARE(dock.insert(0, DockItem::group(QStringLiteral(" padded"), {app(1)})),
           DockEditError::InvalidItem);
  QCOMPARE(dock.insert(0, DockItem::group(QStringLiteral("Dup"), {app(1), app(1)})),
           DockEditError::InvalidItem);
  // A value carrying fields of another kind is malformed, not ignored.
  DockItem mixed = DockItem::application(app(1));
  mixed.path = QStringLiteral("/tmp");
  QCOMPARE(dock.insert(0, mixed), DockEditError::InvalidItem);
  QVERIFY(dock.isEmpty());
}

void DockItemsTests::enforcesItemApplicationAndGroupBounds()
{
  DockItems dock;
  for (int index = 0; index < Bounds::maxItems; ++index)
    QCOMPARE(dock.insert(dock.size(), DockItem::application(app(index))), DockEditError::None);
  QCOMPARE(dock.insert(0, DockItem::trash()), DockEditError::DockFull);

  // Groups carry the application ceiling beyond the item ceiling.
  DockItems grouped;
  int next = 0;
  while (grouped.size() < 4) {
    QStringList members;
    for (int member = 0; member < Bounds::maxGroupApplications; ++member)
      members.append(app(next++));
    QCOMPARE(grouped.insert(grouped.size(), DockItem::group(QStringLiteral("G"), members)),
             DockEditError::None);
  }
  QCOMPARE(grouped.applicationIds().size(), Bounds::maxApplications);
  QCOMPARE(grouped.insert(0, DockItem::application(app(next))), DockEditError::DockFull);
  QCOMPARE(grouped.combineWith(0, app(next), QStringLiteral("G")), DockEditError::DockFull);

  // A full group refuses one more member while the dock still has room.
  DockItems oneGroup;
  QStringList full;
  for (int member = 0; member < Bounds::maxGroupApplications; ++member)
    full.append(app(500 + member));
  QCOMPARE(oneGroup.insert(0, DockItem::group(QStringLiteral("Full"), full)),
           DockEditError::None);
  QCOMPARE(oneGroup.insert(1, DockItem::application(app(600))), DockEditError::None);
  QCOMPARE(oneGroup.combineWith(0, app(601), QString{}), DockEditError::GroupFull);
  QCOMPARE(oneGroup.combine(0, 1, QString{}), DockEditError::GroupFull);

  QStringList tooMany;
  for (int member = 0; member <= Bounds::maxGroupApplications; ++member)
    tooMany.append(app(1000 + member));
  QCOMPARE(DockItems{}.insert(0, DockItem::group(QStringLiteral("Big"), tooMany)),
           DockEditError::InvalidItem);
}

void DockItemsTests::movesToGapsIncludingNoOps()
{
  DockItems dock = appsDock({app(0), app(1), app(2), app(3)});
  // Its own slot and the gap right behind it keep the order.
  QCOMPARE(dock.moveToGap(1, 1), DockEditError::None);
  QCOMPARE(dock.moveToGap(1, 2), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(0), app(1), app(2), app(3)}));
  QCOMPARE(dock.moveToGap(0, 4), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(1), app(2), app(3), app(0)}));
  QCOMPARE(dock.moveToGap(3, 0), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(0), app(1), app(2), app(3)}));
  QCOMPARE(dock.moveToGap(2, 1), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(0), app(2), app(1), app(3)}));
  QCOMPARE(dock.moveToGap(4, 0), DockEditError::OutOfRange);
  QCOMPARE(dock.moveToGap(0, 5), DockEditError::OutOfRange);
  QCOMPARE(dock.removeAt(4), DockEditError::OutOfRange);
  QCOMPARE(dock.removeAt(0), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(2), app(1), app(3)}));
}

void DockItemsTests::groupsCombineRenameUngroupAndEmpty()
{
  DockItems dock = appsDock({app(0), app(1), app(2)});
  dock.insert(dock.size(), DockItem::folder(QStringLiteral("/srv")));
  // Dropping app 2 onto app 0 makes a group in app 0's place, target first.
  QCOMPARE(dock.combine(0, 2, QStringLiteral("Office")), DockEditError::None);
  QCOMPARE(dock.size(), 3);
  QCOMPARE(dock.items().at(0), DockItem::group(QStringLiteral("Office"), {app(0), app(2)}));
  // Onto the group joins it; the name argument is ignored.
  QCOMPARE(dock.combine(0, 1, QStringLiteral("ignored")), DockEditError::None);
  QCOMPARE(dock.items().at(0).applications, QStringList({app(0), app(2), app(1)}));
  QCOMPARE(dock.items().at(0).name, QStringLiteral("Office"));
  // Folders and groups never become members; a group never nests.
  QCOMPARE(dock.combine(0, 1, QStringLiteral("x")), DockEditError::WrongKind);
  QCOMPARE(dock.combine(0, 0, QStringLiteral("x")), DockEditError::OutOfRange);

  QCOMPARE(dock.renameGroup(0, QStringLiteral("Work")), DockEditError::None);
  QCOMPARE(dock.renameGroup(0, QString{}), DockEditError::InvalidItem);
  QCOMPARE(dock.renameGroup(1, QStringLiteral("Folder")), DockEditError::WrongKind);
  QCOMPARE(dock.items().at(0).name, QStringLiteral("Work"));

  QCOMPARE(dock.ungroup(0), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(0), app(2), app(1)}));
  QCOMPARE(dock.size(), 4);

  // New Group wraps one application; removing its last member removes it.
  QCOMPARE(dock.makeGroup(1, QStringLiteral("Solo")), DockEditError::None);
  QCOMPARE(dock.items().at(1).kind, DockItemKind::Group);
  QCOMPARE(dock.removeApplication(app(2)), DockEditError::None);
  QCOMPARE(dock.size(), 3);
  QCOMPARE(dock.indexOfApplication(app(2)), -1);
  QCOMPARE(dock.removeApplication(app(2)), DockEditError::NotInDock);
}

void DockItemsTests::externalApplicationsJoinGroups()
{
  DockItems dock = appsDock({app(0)});
  QCOMPARE(dock.combineWith(0, app(5), QStringLiteral("Games")), DockEditError::None);
  QCOMPARE(dock.items().at(0), DockItem::group(QStringLiteral("Games"), {app(0), app(5)}));
  QCOMPARE(dock.combineWith(0, app(6), QString{}), DockEditError::None);
  QCOMPARE(dock.items().at(0).applications.size(), 3);
  QCOMPARE(dock.combineWith(0, app(6), QString{}), DockEditError::AlreadyInDock);
  dock.insert(dock.size(), DockItem::trash());
  QCOMPARE(dock.combineWith(1, app(7), QStringLiteral("x")), DockEditError::WrongKind);
}

void DockItemsTests::membersMoveOutAtTheGap()
{
  DockItems dock = appsDock({app(0)});
  dock.insert(0, DockItem::group(QStringLiteral("Office"), {app(1), app(2)}));
  // [Office(1,2), 0] -> member 2 out at the end.
  QCOMPARE(dock.moveOutOfGroup(0, app(2), 2), DockEditError::None);
  QCOMPARE(dock.applicationIds(), QStringList({app(1), app(0), app(2)}));
  QCOMPARE(dock.size(), 3);
  // The last member leaving removes the group; a gap beyond it shifts back.
  QCOMPARE(dock.moveOutOfGroup(0, app(1), 3), DockEditError::None);
  QCOMPARE(dock.size(), 3);
  QCOMPARE(dock.applicationIds(), QStringList({app(0), app(2), app(1)}));
  QCOMPARE(dock.moveOutOfGroup(0, app(0), 0), DockEditError::WrongKind);
}

void DockItemsTests::insertAllSkipsDuplicatesAtomically()
{
  DockItems dock = appsDock({app(0)});
  const QVector<DockItem> drop{DockItem::folder(QStringLiteral("/a")),
                               DockItem::application(app(0)),
                               DockItem::file(QStringLiteral("/a/b.txt"))};
  QCOMPARE(dock.insertAll(0, drop), DockEditError::None);
  QCOMPARE(dock.size(), 3);
  QCOMPARE(dock.items().at(0).path, QStringLiteral("/a"));
  QCOMPARE(dock.items().at(1).path, QStringLiteral("/a/b.txt"));
  QCOMPARE(dock.insertAll(0, drop), DockEditError::AlreadyInDock);
  const DockItems before = dock;
  QCOMPARE(dock.insertAll(0, {DockItem::folder(QStringLiteral("/c")),
                              DockItem::folder(QStringLiteral("bad"))}),
           DockEditError::InvalidItem);
  QCOMPARE(dock, before);
}

void DockItemsTests::codecRoundTripsEveryKind()
{
  DockItems dock = appsDock({app(0)});
  dock.insert(1, DockItem::group(QStringLiteral("Office"), {app(1), app(2)}));
  dock.insert(2, DockItem::folder(QStringLiteral("/home/u/Projects")));
  dock.insert(3, DockItem::file(QStringLiteral("/home/u/plan.pdf")));
  dock.insert(4, DockItem::trash());
  const QVariantMap encoded = DockItems::encodeSettingsValue(dock);
  QCOMPARE(encoded.value(QStringLiteral("version")).toLongLong(), Bounds::formatVersion);
  // Group members encode JSON-native (a QVariantList of strings).
  const QVariantMap group = encoded.value(QStringLiteral("items")).toList().at(1).toMap();
  QCOMPARE(group.value(QStringLiteral("applications")).metaType().id(),
           int(QMetaType::QVariantList));
  const auto decoded = DockItems::decodeSettingsValue(encoded);
  QVERIFY2(decoded.ok(), qPrintable(decoded.error));
  QVERIFY(!decoded.unmigrated);
  QCOMPARE(*decoded.items, dock);

  // D-Bus hands string lists back as QStringList and integers as other
  // integral types; both still decode.
  QVariantMap relaxed = encoded;
  QVariantList items = relaxed.value(QStringLiteral("items")).toList();
  QVariantMap relaxedGroup = items.at(1).toMap();
  relaxedGroup.insert(QStringLiteral("applications"), QStringList({app(1), app(2)}));
  items[1] = relaxedGroup;
  relaxed.insert(QStringLiteral("items"), items);
  relaxed.insert(QStringLiteral("version"), 1.0);
  QCOMPARE(*DockItems::decodeSettingsValue(relaxed).items, dock);
}

void DockItemsTests::codecRejectsHostileValuesWholesale_data()
{
  QTest::addColumn<QVariant>("value");
  const QVariantMap valid = record(QStringLiteral("application"), {{QStringLiteral("id"), app(1)}});
  QTest::newRow("not-an-object") << QVariant(QStringList{app(1)});
  QTest::newRow("future-version") << QVariant(document({valid}, qint64(2)));
  QTest::newRow("extra-top-field") << QVariant([&] {
    QVariantMap doc = document({valid});
    doc.insert(QStringLiteral("extra"), true);
    return doc;
  }());
  QTest::newRow("items-not-list") << QVariant(QVariantMap{
      {QStringLiteral("version"), qint64(1)}, {QStringLiteral("items"), QStringLiteral("x")}});
  QTest::newRow("unknown-kind") << QVariant(document({record(QStringLiteral("widget"))}));
  QTest::newRow("unknown-field") << QVariant(document({record(
      QStringLiteral("application"),
      {{QStringLiteral("id"), app(1)}, {QStringLiteral("exec"), QStringLiteral("rm -rf ~")}})}));
  QTest::newRow("relative-path") << QVariant(document({record(
      QStringLiteral("file"), {{QStringLiteral("path"), QStringLiteral("etc/passwd")}})}));
  QTest::newRow("duplicate-app") << QVariant(document({valid, valid}));
  QTest::newRow("member-duplicates-top-level") << QVariant(document({valid, record(
      QStringLiteral("group"), {{QStringLiteral("name"), QStringLiteral("G")},
                                {QStringLiteral("applications"), QVariantList{app(1)}}})}));
  QTest::newRow("empty-group") << QVariant(document({record(
      QStringLiteral("group"), {{QStringLiteral("name"), QStringLiteral("G")},
                                {QStringLiteral("applications"), QVariantList{}}})}));
  QTest::newRow("nested-group") << QVariant(document({record(
      QStringLiteral("group"), {{QStringLiteral("name"), QStringLiteral("G")},
                                {QStringLiteral("applications"),
                                 QVariantList{QVariantMap{{QStringLiteral("kind"),
                                                           QStringLiteral("group")}}}}})}));
  QTest::newRow("two-trash") << QVariant(document({record(QStringLiteral("trash")),
                                                   record(QStringLiteral("trash"))}));
  QVariantList tooMany;
  for (int index = 0; index <= Bounds::maxItems; ++index)
    tooMany.append(record(QStringLiteral("application"), {{QStringLiteral("id"), app(index)}}));
  QTest::newRow("too-many-items") << QVariant(document(tooMany));
}

void DockItemsTests::codecRejectsHostileValuesWholesale()
{
  QFETCH(QVariant, value);
  const auto decoded = DockItems::decodeSettingsValue(value);
  QVERIFY(!decoded.ok());
  QVERIFY(!decoded.unmigrated);
  QVERIFY(!decoded.error.isEmpty());
  QVERIFY(!DockItems::fromSettingsValues({{QLatin1StringView(DockItemsSettingsKey), value}}));
}

void DockItemsTests::absentAndDefaultValuesAreUnmigrated()
{
  for (const QVariant &value : {QVariant{}, QVariant::fromValue(nullptr), QVariant(QVariantMap{})}) {
    const auto decoded = DockItems::decodeSettingsValue(value);
    QVERIFY(decoded.ok());
    QVERIFY(decoded.unmigrated);
    QVERIFY(decoded.items->isEmpty());
  }
  // An emptied, migrated dock is not the default: its version marks it.
  const auto emptied = DockItems::decodeSettingsValue(DockItems::encodeSettingsValue(DockItems{}));
  QVERIFY(emptied.ok());
  QVERIFY(!emptied.unmigrated);
}

void DockItemsTests::legacyPinnedListMigratesOnce()
{
  const auto migrated = DockItems::fromLegacyValue(QVariantList{app(1), app(2)});
  QVERIFY(migrated.has_value());
  QCOMPARE(migrated->applicationIds(), QStringList({app(1), app(2)}));
  QVERIFY(DockItems::fromLegacyValue(QVariant{})->isEmpty());
  // ADR-0076's whole-list rule survives migration.
  QVERIFY(!DockItems::fromLegacyValue(QVariantList{app(1), 42}).has_value());
  QVERIFY(!DockItems::fromLegacyValue(QVariantList{app(1), app(1)}).has_value());
  QVERIFY(!DockItems::fromLegacyValue(QStringLiteral("app.1")).has_value());
  QStringList atCeiling;
  for (int index = 0; index < Bounds::maxLegacyPinned; ++index)
    atCeiling.append(app(index));
  QCOMPARE(DockItems::fromLegacyValue(atCeiling)->size(), Bounds::maxLegacyPinned);
  // ADR-0076 never allowed more; a longer legacy list is hostile.
  QVERIFY(!DockItems::fromLegacyValue(atCeiling + QStringList{app(99)}).has_value());
  // The direct migration is still capped at the item ceiling.
  QStringList many;
  for (int index = 0; index < Bounds::maxItems + 4; ++index)
    many.append(app(index));
  QCOMPARE(DockItems::fromLegacyPinned(many).size(), Bounds::maxItems);
}

void DockItemsTests::snapshotValuesPreferTheMigratedDock()
{
  const QString dockKey = QLatin1StringView(DockItemsSettingsKey);
  const QString legacyKey = QLatin1StringView(LegacyPinnedSettingsKey);
  const auto legacyOnly = DockItems::fromSettingsValues(
      {{dockKey, QVariantMap{}}, {legacyKey, QVariantList{app(1)}}});
  QCOMPARE(legacyOnly->applicationIds(), QStringList{app(1)});
  // Once written, the dock value wins and the legacy list is ignored,
  // even when the legacy list is malformed.
  const auto migrated = DockItems::fromSettingsValues(
      {{dockKey, DockItems::encodeSettingsValue(appsDock({app(2)}))},
       {legacyKey, QVariantList{42}}});
  QCOMPARE(migrated->applicationIds(), QStringList{app(2)});
  QVERIFY(!DockItems::fromSettingsValues({{dockKey, QVariantMap{}}, {legacyKey, QVariantList{42}}})
               .has_value());
}

QTEST_GUILESS_MAIN(DockItemsTests)
#include "tst_dock_items.moc"
