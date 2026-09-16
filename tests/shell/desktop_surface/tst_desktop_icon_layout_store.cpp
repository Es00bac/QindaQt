// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

using QindaQt::Shell::DesktopSurface::DesktopIconLayoutStore;

class DesktopIconLayoutStoreTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void persistsOneGlobalCoordinatePerIcon();
  void oneIconHasOnePlaceForTheWholeDesktop();
  void clearAllForgetsEveryPlacement();
  void migratesTheKeptOutputIntoGlobalCoordinates();
  void migrationDropsTheOtherOutputsConflictingArrangement();
  void migrationIsIdempotentAndPreservesNewerPlacements();
  void legacyDocumentsSurviveUntilMigrated();
  void dragPositionsAreVolatileAndNeverPersisted();
  void malformedAndUnboundedDocumentsFailClosed();
};

void DesktopIconLayoutStoreTests::persistsOneGlobalCoordinatePerIcon() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString path = root.filePath(QStringLiteral("layout.json"));
  {
    DesktopIconLayoutStore store(path);
    QSignalSpy changed(&store, &DesktopIconLayoutStore::changed);
    QVERIFY(store.setPosition(QStringLiteral("1:42"), 3195.5, 77.0));
    QCOMPARE(changed.count(), 1);
  }
  DesktopIconLayoutStore restored(path);
  const QVariantMap point = restored.position(QStringLiteral("1:42"));
  QCOMPARE(point.value(QStringLiteral("x")).toReal(), 3195.5);
  QCOMPARE(point.value(QStringLiteral("y")).toReal(), 77.0);
}

// ADR-0167: the desktop is one desktop. There is no per-output namespace any
// more, so an icon cannot hold two different places at once - which is exactly
// what produced two independent sets of icons on a two-output session.
void DesktopIconLayoutStoreTests::oneIconHasOnePlaceForTheWholeDesktop() {
  QTemporaryDir root;
  DesktopIconLayoutStore store(root.filePath(QStringLiteral("layout.json")));
  QVERIFY(store.setPosition(QStringLiteral("1:1"), 10, 20));
  QVERIFY(store.setPosition(QStringLiteral("1:1"), 3082, 20));
  QCOMPARE(store.position(QStringLiteral("1:1"))
               .value(QStringLiteral("x"))
               .toReal(),
           3082.0);
}

void DesktopIconLayoutStoreTests::clearAllForgetsEveryPlacement() {
  QTemporaryDir root;
  DesktopIconLayoutStore store(root.filePath(QStringLiteral("layout.json")));
  QVERIFY(store.setPosition(QStringLiteral("1:1"), 1, 2));
  QVERIFY(store.setPosition(QStringLiteral("1:2"), 3, 4));
  QVERIFY(store.clearAll());
  QVERIFY(store.position(QStringLiteral("1:1")).isEmpty());
  QVERIFY(store.position(QStringLiteral("1:2")).isEmpty());
}

void DesktopIconLayoutStoreTests::migratesTheKeptOutputIntoGlobalCoordinates() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(
      R"({"schemaVersion":1,"screens":{"HDMI-A-1":{"1:7":{"x":48,"y":96}}}})");
  file.close();

  DesktopIconLayoutStore store(path);
  QVERIFY(store.hasLegacyLayout());
  QVERIFY(store.position(QStringLiteral("1:7")).isEmpty());
  // The kept output's origin in the global frame is added, so an icon the user
  // placed 48 px into their primary output stays 48 px into it.
  QVERIFY(store.migrateLegacyLayout(QStringLiteral("HDMI-A-1"), 0, 0));
  QCOMPARE(store.position(QStringLiteral("1:7"))
               .value(QStringLiteral("x"))
               .toReal(),
           48.0);
  QVERIFY(!store.hasLegacyLayout());

  DesktopIconLayoutStore restored(path);
  QVERIFY(!restored.hasLegacyLayout());
  QCOMPARE(restored.position(QStringLiteral("1:7"))
               .value(QStringLiteral("y"))
               .toReal(),
           96.0);
}

void DesktopIconLayoutStoreTests::
    migrationDropsTheOtherOutputsConflictingArrangement() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  // The same icon identity held two different places, one per output. Only the
  // kept output's arrangement can survive into a single desktop.
  file.write(R"({"schemaVersion":1,"screens":{
      "HDMI-A-1":{"1:7":{"x":48,"y":96}},
      "DP-1":{"1:7":{"x":600,"y":12}}}})");
  file.close();

  DesktopIconLayoutStore store(path);
  QVERIFY(store.migrateLegacyLayout(QStringLiteral("DP-1"), 3072, 0));
  const QVariantMap point = store.position(QStringLiteral("1:7"));
  QCOMPARE(point.value(QStringLiteral("x")).toReal(), 3672.0);
  QCOMPARE(point.value(QStringLiteral("y")).toReal(), 12.0);
}

void DesktopIconLayoutStoreTests::
    migrationIsIdempotentAndPreservesNewerPlacements() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(
      R"({"schemaVersion":1,"screens":{"DP-1":{"1:7":{"x":600,"y":12}}}})");
  file.close();

  DesktopIconLayoutStore store(path);
  // A placement already made in the new frame must win over the legacy one.
  QVERIFY(store.setPosition(QStringLiteral("1:7"), 5, 5));
  QVERIFY(store.migrateLegacyLayout(QStringLiteral("DP-1"), 3072, 0));
  QCOMPARE(store.position(QStringLiteral("1:7"))
               .value(QStringLiteral("x"))
               .toReal(),
           5.0);
  // Running it again is a no-op that still reports success.
  QVERIFY(store.migrateLegacyLayout(QStringLiteral("DP-1"), 3072, 0));
  QCOMPARE(store.position(QStringLiteral("1:7"))
               .value(QStringLiteral("x"))
               .toReal(),
           5.0);
}

void DesktopIconLayoutStoreTests::legacyDocumentsSurviveUntilMigrated() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(
      R"({"schemaVersion":1,"screens":{"DP-1":{"1:7":{"x":600,"y":12}}}})");
  file.close();

  {
    // Writing a new placement must not silently destroy the unmigrated
    // arrangement; an older shell still reading this file keeps working.
    DesktopIconLayoutStore store(path);
    QVERIFY(store.setPosition(QStringLiteral("1:8"), 1, 1));
  }
  DesktopIconLayoutStore reloaded(path);
  QVERIFY(reloaded.hasLegacyLayout());
  QVERIFY(reloaded.migrateLegacyLayout(QStringLiteral("DP-1"), 0, 0));
  QCOMPARE(reloaded.position(QStringLiteral("1:7"))
               .value(QStringLiteral("x"))
               .toReal(),
           600.0);
}

void DesktopIconLayoutStoreTests::dragPositionsAreVolatileAndNeverPersisted() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  DesktopIconLayoutStore store(path);
  QSignalSpy dragChanged(&store, &DesktopIconLayoutStore::dragChanged);
  QSignalSpy changed(&store, &DesktopIconLayoutStore::changed);

  QVERIFY(!store.isDragging());
  store.updateDrag({{QStringLiteral("1:7"),
                     QVariantMap{{QStringLiteral("x"), 3100.0},
                                 {QStringLiteral("y"), 40.0}}}});
  QVERIFY(store.isDragging());
  QCOMPARE(dragChanged.count(), 1);
  QCOMPARE(store.dragPosition(QStringLiteral("1:7"))
               .value(QStringLiteral("x"))
               .toReal(),
           3100.0);
  // A drag publishes nothing to the saved arrangement.
  QCOMPARE(changed.count(), 0);
  QVERIFY(store.position(QStringLiteral("1:7")).isEmpty());

  // Republishing the identical batch must not wake every other output again.
  store.updateDrag({{QStringLiteral("1:7"),
                     QVariantMap{{QStringLiteral("x"), 3100.0},
                                 {QStringLiteral("y"), 40.0}}}});
  QCOMPARE(dragChanged.count(), 1);

  // A non-finite or out-of-range point is dropped rather than published.
  store.updateDrag({{QStringLiteral("1:9"),
                     QVariantMap{{QStringLiteral("x"),
                                  std::numeric_limits<qreal>::infinity()},
                                 {QStringLiteral("y"), 1.0}}}});
  QVERIFY(store.dragPosition(QStringLiteral("1:9")).isEmpty());

  store.endDrag();
  QVERIFY(!store.isDragging());
  QVERIFY(store.dragPosition(QStringLiteral("1:7")).isEmpty());
  // Nothing about the drag reached the disk.
  QVERIFY(!QFile::exists(path));
}

void DesktopIconLayoutStoreTests::malformedAndUnboundedDocumentsFailClosed() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(R"({"schemaVersion":2,"icons":{"1:2":{"x":"oops","y":4}}})");
  file.close();
  DesktopIconLayoutStore store(path);
  QVERIFY(store.position(QStringLiteral("1:2")).isEmpty());
  QVERIFY(!store.setPosition(QString(300, QLatin1Char('x')), 1, 2));
  QVERIFY(!store.setPosition(QStringLiteral("1:2"),
                             std::numeric_limits<qreal>::infinity(), 2));
  QVERIFY(!store.setPosition(QStringLiteral("1:2"), 2000000.0, 2));
}

QTEST_GUILESS_MAIN(DesktopIconLayoutStoreTests)
#include "tst_desktop_icon_layout_store.moc"
