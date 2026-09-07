// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"

#include <QHash>
#include <QLocale>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

using QindaQt::Apps::FileManager::DirectoryEntry;
using QindaQt::Apps::FileManager::FileLauncherPtr;
using QindaQt::Apps::FileManager::LaunchError;
using QindaQt::Apps::FileManager::LaunchResult;
using QindaQt::Apps::FileManager::ListingError;
using QindaQt::Apps::FileManager::ListingResult;
using QindaQt::Apps::FileManager::NavigationController;
using QindaQt::Apps::FileManager::Test::FakeDirectoryLister;
using QindaQt::Apps::FileManager::Test::FakeFileLauncher;

namespace {

[[nodiscard]] DirectoryEntry makeDirectory(const QString &name, const QString &path,
                                           bool hidden = false) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = path;
  entry.isDirectory = true;
  entry.isHidden = hidden;
  return entry;
}

[[nodiscard]] DirectoryEntry makeFile(const QString &name, const QString &path,
                                      bool hidden = false) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = path;
  entry.isDirectory = false;
  entry.isHidden = hidden;
  return entry;
}

[[nodiscard]] ListingResult okListing(const QString &path, QVector<DirectoryEntry> entries) {
  ListingResult result;
  result.path = path;
  result.entries = std::move(entries);
  return result;
}

[[nodiscard]] ListingResult errorListing(const QString &path, ListingError error,
                                         const QString &diagnostic) {
  ListingResult result;
  result.path = path;
  result.error = error;
  result.diagnostic = diagnostic;
  return result;
}

} // namespace

class TestNavigationController final : public QObject {
  Q_OBJECT

private slots:
  void initialNavigationPublishesEntriesAndStatus();
  void navigatingIntoADirectoryUpdatesHistoryAndReloads();
  void backForwardUpRerequestTheExpectedPaths();
  void activatingADirectoryEntryNavigatesIntoIt();
  void activatingAFileEntryLaunchesItAndReportsFailure();
  void clearLaunchErrorResetsTheProperty();
  void everyListingErrorMapsToADistinctStatusKey();
  void emptyDirectoryIsItsOwnStatus();
  void truncatedListingIsReflectedInStatusMessage();
  void indexOfNameFindsAndMissesCorrectly();
  void outOfRangeActivateIsIgnored();
  void hiddenEntriesAreFilteredByDefault();
  void hiddenNoticeComposesWithTheTruncationNotice();
  void sortColumnReordersTogglesDirectionAndIgnoresUnknownKeys();
  void directoriesFirstCanBeDisabled();
  void formattedFieldsArePublishedPerEntry();
  void indexOfNameFollowsTheVisibleListing();
  void viewModeAcceptsOnlyListAndGrid();
};

void TestNavigationController::initialNavigationPublishesEntriesAndStatus() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  rawLister->setResult(QStringLiteral("/home"),
                       okListing(QStringLiteral("/home"),
                                {makeDirectory(QStringLiteral("docs"),
                                               QStringLiteral("/home/docs"))}));

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  QSignalSpy entriesSpy(&controller, &NavigationController::entriesChanged);
  QSignalSpy navigationSpy(&controller, &NavigationController::navigationChanged);

  controller.navigateTo(QStringLiteral("/home"));

  QCOMPARE(controller.currentPath(), QStringLiteral("/home"));
  QCOMPARE(controller.statusKey(), QStringLiteral("ready"));
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(entriesSpy.count(), 1);
  QCOMPARE(navigationSpy.count(), 1);
  QVERIFY(!controller.canGoBack());
  QVERIFY(!controller.canGoForward());
}

void TestNavigationController::navigatingIntoADirectoryUpdatesHistoryAndReloads() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  rawLister->setResult(QStringLiteral("/home"), okListing(QStringLiteral("/home"), {}));
  rawLister->setResult(QStringLiteral("/home/docs"),
                       okListing(QStringLiteral("/home/docs"), {}));

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));
  controller.navigateTo(QStringLiteral("/home/docs"));

  QCOMPARE(controller.currentPath(), QStringLiteral("/home/docs"));
  QVERIFY(controller.canGoBack());
  QVERIFY(!controller.canGoForward());
  QCOMPARE(rawLister->requestedPaths(),
           (QStringList{QStringLiteral("/home"), QStringLiteral("/home/docs")}));
}

void TestNavigationController::backForwardUpRerequestTheExpectedPaths() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  for (const QString &path :
       {QStringLiteral("/a"), QStringLiteral("/a/b"), QStringLiteral("/a/b/c")}) {
    rawLister->setResult(path, okListing(path, {}));
  }

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/a"));
  controller.navigateTo(QStringLiteral("/a/b"));
  controller.navigateTo(QStringLiteral("/a/b/c"));

  controller.goBack();
  QCOMPARE(controller.currentPath(), QStringLiteral("/a/b"));
  controller.goUp();
  QCOMPARE(controller.currentPath(), QStringLiteral("/a"));
  // AGENT-GUARD: goUp() is an ordinary user navigation, so it follows
  // NavigationHistory::navigateTo's contract and clears the forward stack.
  // This expectation must stay aligned with forwardStackIsClearedByANewNavigation
  // in the history tests; a promise that Up preserves Forward would require a
  // deliberate model change, not a test-only exception.
  QVERIFY(!controller.canGoForward());

  QCOMPARE(rawLister->requestedPaths(),
           (QStringList{QStringLiteral("/a"), QStringLiteral("/a/b"), QStringLiteral("/a/b/c"),
                       QStringLiteral("/a/b"), QStringLiteral("/a")}));
}

void TestNavigationController::activatingADirectoryEntryNavigatesIntoIt() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  rawLister->setResult(QStringLiteral("/home"),
                       okListing(QStringLiteral("/home"),
                                {makeDirectory(QStringLiteral("docs"),
                                               QStringLiteral("/home/docs"))}));
  rawLister->setResult(QStringLiteral("/home/docs"),
                       okListing(QStringLiteral("/home/docs"), {}));

  auto launcher = std::make_unique<FakeFileLauncher>();
  auto *rawLauncher = launcher.get();
  NavigationController controller(std::move(lister), std::move(launcher));
  controller.navigateTo(QStringLiteral("/home"));
  controller.activate(0);

  QCOMPARE(controller.currentPath(), QStringLiteral("/home/docs"));
  QVERIFY(rawLauncher->requestedPaths().isEmpty());
}

void TestNavigationController::activatingAFileEntryLaunchesItAndReportsFailure() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  rawLister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeFile(QStringLiteral("notes.txt"), QStringLiteral("/home/notes.txt"))}));

  auto launcher = std::make_unique<FakeFileLauncher>();
  auto *rawLauncher = launcher.get();
  rawLauncher->setResult(LaunchResult{LaunchError::LaunchFailed, QStringLiteral("no handler")});

  NavigationController controller(std::move(lister), std::move(launcher));
  QSignalSpy launchErrorSpy(&controller, &NavigationController::launchErrorChanged);
  controller.navigateTo(QStringLiteral("/home"));
  controller.activate(0);

  QCOMPARE(rawLauncher->requestedPaths(), QStringList{QStringLiteral("/home/notes.txt")});
  QCOMPARE(controller.launchError(), QStringLiteral("no handler"));
  QCOMPARE(launchErrorSpy.count(), 1);
  // A failed launch never navigates.
  QCOMPARE(controller.currentPath(), QStringLiteral("/home"));
}

void TestNavigationController::clearLaunchErrorResetsTheProperty() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeFile(QStringLiteral("notes.txt"), QStringLiteral("/home/notes.txt"))}));
  auto launcher = std::make_unique<FakeFileLauncher>();
  launcher->setResult(LaunchResult{LaunchError::Unreadable, QStringLiteral("denied")});

  NavigationController controller(std::move(lister), std::move(launcher));
  controller.navigateTo(QStringLiteral("/home"));
  controller.activate(0);
  QVERIFY(!controller.launchError().isEmpty());

  QSignalSpy spy(&controller, &NavigationController::launchErrorChanged);
  controller.clearLaunchError();
  QVERIFY(controller.launchError().isEmpty());
  QCOMPARE(spy.count(), 1);

  // A second clear on an already-empty error is a no-op.
  controller.clearLaunchError();
  QCOMPARE(spy.count(), 1);
}

void TestNavigationController::everyListingErrorMapsToADistinctStatusKey() {
  const QVector<QPair<ListingError, QString>> cases = {
      {ListingError::NotFound, QStringLiteral("missing")},
      {ListingError::PermissionDenied, QStringLiteral("permission-denied")},
      {ListingError::NotADirectory, QStringLiteral("not-a-directory")},
      {ListingError::Unknown, QStringLiteral("error")},
  };
  for (const auto &[error, expectedKey] : cases) {
    auto lister = std::make_unique<FakeDirectoryLister>();
    lister->setResult(QStringLiteral("/x"), errorListing(QStringLiteral("/x"), error, QStringLiteral("boom")));
    NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
    controller.navigateTo(QStringLiteral("/x"));
    QCOMPARE(controller.statusKey(), expectedKey);
    QCOMPARE(controller.statusMessage(), QStringLiteral("boom"));
    QCOMPARE(controller.entryCount(), 0);
  }
}

void TestNavigationController::emptyDirectoryIsItsOwnStatus() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(QStringLiteral("/empty"), okListing(QStringLiteral("/empty"), {}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/empty"));
  QCOMPARE(controller.statusKey(), QStringLiteral("empty"));
}

void TestNavigationController::truncatedListingIsReflectedInStatusMessage() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult truncated =
      okListing(QStringLiteral("/big"), {makeFile(QStringLiteral("a"), QStringLiteral("/big/a"))});
  truncated.truncated = true;
  lister->setResult(QStringLiteral("/big"), truncated);

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/big"));
  QCOMPARE(controller.statusKey(), QStringLiteral("ready"));
  QVERIFY(controller.statusMessage().contains(QStringLiteral("1")));
}

void TestNavigationController::indexOfNameFindsAndMissesCorrectly() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeDirectory(QStringLiteral("docs"), QStringLiteral("/home/docs")),
                makeFile(QStringLiteral("notes.txt"), QStringLiteral("/home/notes.txt"))}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));

  QCOMPARE(controller.indexOfName(QStringLiteral("notes.txt")), 1);
  QCOMPARE(controller.indexOfName(QStringLiteral("missing")), -1);
}

void TestNavigationController::outOfRangeActivateIsIgnored() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(QStringLiteral("/home"), okListing(QStringLiteral("/home"), {}));
  auto launcher = std::make_unique<FakeFileLauncher>();
  auto *rawLauncher = launcher.get();

  NavigationController controller(std::move(lister), std::move(launcher));
  controller.navigateTo(QStringLiteral("/home"));
  controller.activate(-1);
  controller.activate(5);

  QVERIFY(rawLauncher->requestedPaths().isEmpty());
  QCOMPARE(controller.currentPath(), QStringLiteral("/home"));
}

void TestNavigationController::hiddenEntriesAreFilteredByDefault() {
  // AGENT-NOTE: S2 changed the default: hidden entries stay in the raw listing
  // but are filtered out of the published (visible) listing until the user
  // opts in. The filter is deliberate; do not "fix" it back to visible.
  auto lister = std::make_unique<FakeDirectoryLister>();
  auto *rawLister = lister.get();
  rawLister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeDirectory(QStringLiteral(".hdir"), QStringLiteral("/home/.hdir"),
                              true),
                makeFile(QStringLiteral(".hidden"), QStringLiteral("/home/.hidden"),
                         true),
                makeDirectory(QStringLiteral("vdir"), QStringLiteral("/home/vdir")),
                makeFile(QStringLiteral("visible.txt"),
                         QStringLiteral("/home/visible.txt"))}));
  rawLister->setResult(QStringLiteral("/home/vdir"),
                       okListing(QStringLiteral("/home/vdir"), {}));

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));
  QVERIFY(!controller.showHidden());
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.indexOfName(QStringLiteral(".hidden")), -1);
  QCOMPARE(controller.indexOfName(QStringLiteral(".hdir")), -1);
  QCOMPARE(controller.statusMessage(), QStringLiteral("2 hidden"));

  // activate() addresses the visible listing, so index 0 is the first
  // non-hidden directory even though a hidden directory sorts earlier.
  controller.activate(0);
  QCOMPARE(controller.currentPath(), QStringLiteral("/home/vdir"));
  controller.goBack();
  QCOMPARE(controller.currentPath(), QStringLiteral("/home"));

  QSignalSpy presentationSpy(&controller, &NavigationController::presentationChanged);
  QSignalSpy entriesSpy(&controller, &NavigationController::entriesChanged);
  controller.setShowHidden(true);
  QCOMPARE(presentationSpy.count(), 1);
  QCOMPARE(entriesSpy.count(), 1);
  QCOMPARE(controller.entryCount(), 4);
  // Hidden entries rejoin at their sorted position (directories first, then
  // names), not appended at the end.
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral(".hdir"));
  QCOMPARE(controller.entryAt(1)->name, QStringLiteral("vdir"));
  QCOMPARE(controller.entryAt(2)->name, QStringLiteral(".hidden"));
  QCOMPARE(controller.entryAt(3)->name, QStringLiteral("visible.txt"));
  QVERIFY(controller.statusMessage().isEmpty());

  controller.setShowHidden(true); // same value: no-op
  QCOMPARE(presentationSpy.count(), 1);
  QCOMPARE(entriesSpy.count(), 1);

  controller.setShowHidden(false);
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.statusMessage(), QStringLiteral("2 hidden"));
  QCOMPARE(presentationSpy.count(), 2);
}

void TestNavigationController::hiddenNoticeComposesWithTheTruncationNotice() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult truncated = okListing(
      QStringLiteral("/big"),
      {makeFile(QStringLiteral(".hidden"), QStringLiteral("/big/.hidden"), true),
       makeFile(QStringLiteral("a"), QStringLiteral("/big/a"))});
  truncated.truncated = true;
  lister->setResult(QStringLiteral("/big"), truncated);

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/big"));
  QCOMPARE(controller.statusKey(), QStringLiteral("ready"));
  QCOMPARE(controller.entryCount(), 1);
  // AGENT-CONTRACT: The truncation notice counts listed (not visible) entries;
  // the hidden notice appends with a fixed "; " separator. EntryList.qml's
  // truncationNotice label is the single surface for this composed text.
  QCOMPARE(controller.statusMessage(),
           QStringLiteral("Showing the first 2 entries; 1 hidden"));
}

void TestNavigationController::sortColumnReordersTogglesDirectionAndIgnoresUnknownKeys() {
  auto small = makeFile(QStringLiteral("a.bin"), QStringLiteral("/home/a.bin"));
  small.size = 30;
  auto medium = makeFile(QStringLiteral("b.bin"), QStringLiteral("/home/b.bin"));
  medium.size = 10;
  auto large = makeFile(QStringLiteral("c.bin"), QStringLiteral("/home/c.bin"));
  large.size = 20;
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(QStringLiteral("/home"),
                    okListing(QStringLiteral("/home"), {small, medium, large}));

  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));
  QCOMPARE(controller.sortColumn(), QStringLiteral("name"));
  QCOMPARE(controller.sortDirection(), QStringLiteral("ascending"));
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("a.bin"));

  QSignalSpy presentationSpy(&controller, &NavigationController::presentationChanged);
  QSignalSpy entriesSpy(&controller, &NavigationController::entriesChanged);
  controller.setSortColumn(QStringLiteral("size"));
  QCOMPARE(presentationSpy.count(), 1);
  QCOMPARE(entriesSpy.count(), 1);
  QCOMPARE(controller.sortColumn(), QStringLiteral("size"));
  QCOMPARE(controller.sortDirection(), QStringLiteral("ascending"));
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("b.bin"));
  QCOMPARE(controller.entryAt(1)->name, QStringLiteral("c.bin"));
  QCOMPARE(controller.entryAt(2)->name, QStringLiteral("a.bin"));

  // Re-selecting the active column flips the direction, header-click style.
  controller.setSortColumn(QStringLiteral("size"));
  QCOMPARE(controller.sortDirection(), QStringLiteral("descending"));
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("a.bin"));
  QCOMPARE(controller.entryAt(2)->name, QStringLiteral("b.bin"));
  QCOMPARE(presentationSpy.count(), 2);

  // Unknown keys are ignored without republishing or re-sorting.
  controller.setSortColumn(QStringLiteral("bogus"));
  QCOMPARE(controller.sortColumn(), QStringLiteral("size"));
  QCOMPARE(controller.sortDirection(), QStringLiteral("descending"));
  QCOMPARE(presentationSpy.count(), 2);
  QCOMPARE(entriesSpy.count(), 2);
}

void TestNavigationController::directoriesFirstCanBeDisabled() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeDirectory(QStringLiteral("zed"), QStringLiteral("/home/zed")),
                makeFile(QStringLiteral("alpha"), QStringLiteral("/home/alpha"))}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));
  QVERIFY(controller.directoriesFirst());
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("zed"));

  QSignalSpy presentationSpy(&controller, &NavigationController::presentationChanged);
  controller.setDirectoriesFirst(false);
  QVERIFY(!controller.directoriesFirst());
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("alpha"));
  QCOMPARE(presentationSpy.count(), 1);

  controller.setDirectoriesFirst(false); // same value: no-op
  QCOMPARE(presentationSpy.count(), 1);
  controller.setDirectoriesFirst(true);
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("zed"));
  QCOMPARE(presentationSpy.count(), 2);
}

void TestNavigationController::formattedFieldsArePublishedPerEntry() {
  const QDateTime modified(QDate(2026, 2, 3), QTime(10, 30), QTimeZone::UTC);
  auto dir = makeDirectory(QStringLiteral("docs"), QStringLiteral("/home/docs"));
  auto file = makeFile(QStringLiteral("notes.txt"), QStringLiteral("/home/notes.txt"));
  file.size = 100;
  file.lastModified = modified;
  auto noSuffix = makeFile(QStringLiteral("README"), QStringLiteral("/home/README"));
  auto link = makeFile(QStringLiteral("alias"), QStringLiteral("/home/alias"));
  link.isSymlink = true;
  auto staleDate = makeFile(QStringLiteral("old.dat"), QStringLiteral("/home/old.dat"));
  staleDate.lastModified = QDateTime();

  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(QStringLiteral("/home"),
                    okListing(QStringLiteral("/home"),
                             {dir, file, noSuffix, link, staleDate}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));

  const QVariantList entries = controller.entries();
  QCOMPARE(entries.size(), 5);
  QHash<QString, QVariantMap> byName;
  for (const QVariant &entry : entries) {
    const QVariantMap map = entry.toMap();
    byName.insert(map.value(QStringLiteral("name")).toString(), map);
  }
  QCOMPARE(byName.size(), 5);
  QCOMPARE(byName.value(QStringLiteral("docs"))
               .value(QStringLiteral("sizeText"))
               .toString(),
           QStringLiteral("—"));
  QCOMPARE(byName.value(QStringLiteral("docs"))
               .value(QStringLiteral("kindText"))
               .toString(),
           QStringLiteral("Folder"));
  QCOMPARE(byName.value(QStringLiteral("notes.txt"))
               .value(QStringLiteral("sizeText"))
               .toString(),
           QLocale().formattedDataSize(100));
  QCOMPARE(byName.value(QStringLiteral("notes.txt"))
               .value(QStringLiteral("kindText"))
               .toString(),
           QStringLiteral("TXT File"));
  QCOMPARE(byName.value(QStringLiteral("notes.txt"))
               .value(QStringLiteral("modifiedText"))
               .toString(),
           QLocale().toString(modified, QLocale::ShortFormat));
  QCOMPARE(byName.value(QStringLiteral("README"))
               .value(QStringLiteral("kindText"))
               .toString(),
           QStringLiteral("File"));
  QCOMPARE(byName.value(QStringLiteral("alias"))
               .value(QStringLiteral("kindText"))
               .toString(),
           QStringLiteral("Link"));
  QVERIFY(byName.value(QStringLiteral("old.dat"))
              .value(QStringLiteral("modifiedText"))
              .toString()
              .isEmpty());
}

void TestNavigationController::indexOfNameFollowsTheVisibleListing() {
  // AGENT-CONTRACT: QML restores the selection by name after every refresh
  // (EntryList.qml's lastSelectedName guard). indexOfName must therefore
  // address the visible listing only, so filtering or re-sorting shifts the
  // returned index to wherever the name actually landed.
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(
      QStringLiteral("/home"),
      okListing(QStringLiteral("/home"),
               {makeFile(QStringLiteral(".hidden"), QStringLiteral("/home/.hidden"),
                         true),
                makeFile(QStringLiteral("b.txt"), QStringLiteral("/home/b.txt")),
                makeFile(QStringLiteral("a.txt"), QStringLiteral("/home/a.txt"))}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  controller.navigateTo(QStringLiteral("/home"));

  QCOMPARE(controller.indexOfName(QStringLiteral("a.txt")), 0);
  QCOMPARE(controller.indexOfName(QStringLiteral("b.txt")), 1);
  controller.setShowHidden(true);
  QCOMPARE(controller.indexOfName(QStringLiteral(".hidden")), 0);
  QCOMPARE(controller.indexOfName(QStringLiteral("a.txt")), 1);
  QCOMPARE(controller.indexOfName(QStringLiteral("b.txt")), 2);
  controller.setSortColumn(QStringLiteral("name")); // toggle to descending
  QCOMPARE(controller.indexOfName(QStringLiteral("b.txt")), 0);
  QCOMPARE(controller.indexOfName(QStringLiteral(".hidden")), 2);
}

void TestNavigationController::viewModeAcceptsOnlyListAndGrid() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  lister->setResult(QStringLiteral("/home"), okListing(QStringLiteral("/home"), {}));
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>());
  QCOMPARE(controller.viewMode(), QStringLiteral("list"));

  QSignalSpy presentationSpy(&controller, &NavigationController::presentationChanged);
  controller.setViewMode(QStringLiteral("grid"));
  QCOMPARE(controller.viewMode(), QStringLiteral("grid"));
  QCOMPARE(presentationSpy.count(), 1);

  // Same value and unknown modes are ignored without republishing.
  controller.setViewMode(QStringLiteral("grid"));
  controller.setViewMode(QStringLiteral("columns"));
  controller.setViewMode(QString());
  QCOMPARE(controller.viewMode(), QStringLiteral("grid"));
  QCOMPARE(presentationSpy.count(), 1);

  controller.setViewMode(QStringLiteral("list"));
  QCOMPARE(controller.viewMode(), QStringLiteral("list"));
  QCOMPARE(presentationSpy.count(), 2);
}

QTEST_APPLESS_MAIN(TestNavigationController)
#include "tst_navigation_controller.moc"
