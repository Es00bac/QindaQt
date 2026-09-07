// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/places_controller.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] QVariantMap placeMap(const QString &id, const QString &name,
                                   const QString &path) {
  return QVariantMap{{QStringLiteral("id"), id},
                     {QStringLiteral("name"), name},
                     {QStringLiteral("path"), path}};
}

} // namespace

class TestPlacesController final : public QObject {
  Q_OBJECT

private slots:
  void fixedPlacesArePublishedInOrder();
  void addBookmarkTrimsCleansAndPersistsAcrossControllers();
  void duplicateBookmarkPathIsANoOp();
  void invalidBookmarkSetsStoreError();
  void bookmarkCapSetsStoreError();
  void removeBookmarkByIndex();
  void poisonedRootSurfacesARecoverableStoreError();
};

void TestPlacesController::fixedPlacesArePublishedInOrder() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PlacesController controller(
      std::make_unique<BookmarksStore>(directory.filePath(QStringLiteral("state"))));

  QVariantList expected = {
      placeMap(QStringLiteral("home"), QStringLiteral("Home"), QDir::homePath()),
      placeMap(QStringLiteral("root"), QStringLiteral("File System"),
               QStringLiteral("/")),
  };
  // The Trash place is omitted only when the platform publishes no generic
  // data location at all; mirror that single conditional.
  const QString dataHome =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (!dataHome.isEmpty()) {
    expected.append(placeMap(QStringLiteral("trash"), QStringLiteral("Trash"),
                             QDir(dataHome).filePath(QStringLiteral("Trash/files"))));
  }
  QCOMPARE(controller.places(), expected);
}

void TestPlacesController::addBookmarkTrimsCleansAndPersistsAcrossControllers() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString stateRoot = directory.filePath(QStringLiteral("state"));
  {
    PlacesController controller(std::make_unique<BookmarksStore>(stateRoot));
    QSignalSpy bookmarksSpy(&controller, &PlacesController::bookmarksChanged);
    controller.addBookmark(QStringLiteral("  Docs  "),
                           QStringLiteral("/home/user/./docs/"));
    QCOMPARE(bookmarksSpy.count(), 1);
    QCOMPARE(controller.bookmarkValues(),
             QVector<Bookmark>(
                 {Bookmark{QStringLiteral("Docs"), QStringLiteral("/home/user/docs")}}));
    QVERIFY(controller.storeError().isEmpty());

    const QVariantList published = controller.bookmarks();
    QCOMPARE(published.size(), 1);
    QCOMPARE(published.constFirst().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Docs"));
    QCOMPARE(published.constFirst().toMap().value(QStringLiteral("index")).toInt(), 0);
  }
  // A second controller over the same root observes the persisted inventory.
  PlacesController reloaded(std::make_unique<BookmarksStore>(stateRoot));
  QCOMPARE(reloaded.bookmarkValues(),
           QVector<Bookmark>(
               {Bookmark{QStringLiteral("Docs"), QStringLiteral("/home/user/docs")}}));
  QVERIFY(reloaded.storeError().isEmpty());
}

void TestPlacesController::duplicateBookmarkPathIsANoOp() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PlacesController controller(
      std::make_unique<BookmarksStore>(directory.filePath(QStringLiteral("state"))));
  QSignalSpy bookmarksSpy(&controller, &PlacesController::bookmarksChanged);
  controller.addBookmark(QStringLiteral("Docs"), QStringLiteral("/home/user/docs"));
  QCOMPARE(bookmarksSpy.count(), 1);

  // Dedup keys on the cleaned path only: a second add, even with a different
  // name or an unclean spelling, neither republishes nor reports an error.
  controller.addBookmark(QStringLiteral("Docs again"), QStringLiteral("/home/user/docs"));
  controller.addBookmark(QStringLiteral("Docs unclean"),
                         QStringLiteral("/home/user/docs/"));
  QCOMPARE(bookmarksSpy.count(), 1);
  QCOMPARE(controller.bookmarkValues().size(), 1);
  QVERIFY(controller.storeError().isEmpty());
}

void TestPlacesController::invalidBookmarkSetsStoreError() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PlacesController controller(
      std::make_unique<BookmarksStore>(directory.filePath(QStringLiteral("state"))));
  QSignalSpy errorSpy(&controller, &PlacesController::storeErrorChanged);
  QSignalSpy bookmarksSpy(&controller, &PlacesController::bookmarksChanged);

  controller.addBookmark(QStringLiteral("   "), QStringLiteral("/home/user/docs"));
  controller.addBookmark(QStringLiteral("Relative"), QStringLiteral("relative/dir"));
  QCOMPARE(errorSpy.count(), 2);
  QCOMPARE(bookmarksSpy.count(), 0);
  QVERIFY(controller.bookmarkValues().isEmpty());
  QVERIFY(!controller.storeError().isEmpty());

  controller.clearStoreError();
  QCOMPARE(errorSpy.count(), 3);
  QVERIFY(controller.storeError().isEmpty());
  // Clearing an empty error is a no-op.
  controller.clearStoreError();
  QCOMPARE(errorSpy.count(), 3);
}

void TestPlacesController::bookmarkCapSetsStoreError() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PlacesController controller(
      std::make_unique<BookmarksStore>(directory.filePath(QStringLiteral("state"))));
  for (int i = 0; i < BookmarksStore::maximumBookmarks; ++i) {
    controller.addBookmark(QStringLiteral("b%1").arg(i),
                           QStringLiteral("/dir-%1").arg(i));
  }
  QCOMPARE(controller.bookmarkValues().size(), BookmarksStore::maximumBookmarks);
  QVERIFY(controller.storeError().isEmpty());

  QSignalSpy errorSpy(&controller, &PlacesController::storeErrorChanged);
  controller.addBookmark(QStringLiteral("overflow"), QStringLiteral("/overflow"));
  QCOMPARE(controller.bookmarkValues().size(), BookmarksStore::maximumBookmarks);
  QCOMPARE(controller.storeError(), QStringLiteral("The bookmark list is full"));
  QCOMPARE(errorSpy.count(), 1);
}

void TestPlacesController::removeBookmarkByIndex() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PlacesController controller(
      std::make_unique<BookmarksStore>(directory.filePath(QStringLiteral("state"))));
  controller.addBookmark(QStringLiteral("One"), QStringLiteral("/one"));
  controller.addBookmark(QStringLiteral("Two"), QStringLiteral("/two"));

  QSignalSpy bookmarksSpy(&controller, &PlacesController::bookmarksChanged);
  controller.removeBookmark(0);
  QCOMPARE(bookmarksSpy.count(), 1);
  QCOMPARE(controller.bookmarkValues(),
           QVector<Bookmark>({Bookmark{QStringLiteral("Two"), QStringLiteral("/two")}}));

  // Out-of-range removals are silent no-ops.
  controller.removeBookmark(-1);
  controller.removeBookmark(1);
  QCOMPARE(bookmarksSpy.count(), 1);
  QCOMPARE(controller.bookmarkValues().size(), 1);
}

void TestPlacesController::poisonedRootSurfacesARecoverableStoreError() {
  // AGENT-NOTE: A state root that is a regular file makes openStateDirectory
  // fail with ENOTDIR -> InvalidRoot on both load and store. The controller
  // must surface that as a typed, dismissible banner string instead of
  // losing bookmarks silently, and an empty inventory must stay usable.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString fileRoot = directory.filePath(QStringLiteral("not-a-directory"));
  QFile file(fileRoot);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();

  PlacesController controller(std::make_unique<BookmarksStore>(fileRoot));
  QVERIFY(!controller.storeError().isEmpty());
  QVERIFY(controller.bookmarkValues().isEmpty());

  QSignalSpy errorSpy(&controller, &PlacesController::storeErrorChanged);
  controller.addBookmark(QStringLiteral("Docs"), QStringLiteral("/home/user/docs"));
  QCOMPARE(errorSpy.count(), 1);
  QVERIFY(!controller.storeError().isEmpty());

  controller.clearStoreError();
  QCOMPARE(errorSpy.count(), 2);
  QVERIFY(controller.storeError().isEmpty());
}

QTEST_GUILESS_MAIN(TestPlacesController)
#include "tst_places_controller.moc"
