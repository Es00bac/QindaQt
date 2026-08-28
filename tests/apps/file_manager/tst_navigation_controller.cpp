// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"

#include <QSignalSpy>
#include <QTest>

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

[[nodiscard]] DirectoryEntry makeDirectory(const QString &name, const QString &path) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = path;
  entry.isDirectory = true;
  return entry;
}

[[nodiscard]] DirectoryEntry makeFile(const QString &name, const QString &path) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = path;
  entry.isDirectory = false;
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

QTEST_APPLESS_MAIN(TestNavigationController)
#include "tst_navigation_controller.moc"
