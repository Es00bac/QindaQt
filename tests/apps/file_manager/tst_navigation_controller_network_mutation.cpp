// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/navigation_controller.h"
#include "network/network_location.h"

#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Apps::FileManager::Test::FakeDirectoryLister;
using QindaQt::Apps::FileManager::Test::FakeFileLauncher;
using QindaQt::Apps::FileManager::Test::FakeNetworkDirectoryBackend;
using QindaQt::Apps::FileManager::Test::FakeRemoteFolderCreator;
using QindaQt::Apps::FileManager::Test::FakeRemoteRenamer;

namespace {

[[nodiscard]] DirectoryEntry makeEntry(const QString &name, const QString &absolutePath,
                                       bool isDirectory) {
  DirectoryEntry entry;
  entry.name = name;
  entry.absolutePath = absolutePath;
  entry.isDirectory = isDirectory;
  return entry;
}

[[nodiscard]] NetworkListingResult successResult(const QUrl &url,
                                                 QVector<DirectoryEntry> entries,
                                                 bool truncated = false) {
  NetworkListingResult result;
  result.url = url;
  result.entries = std::move(entries);
  result.truncated = truncated;
  return result;
}

// Publishes a one-entry remote listing for url through rawBackend so
// rename tests have a listed child to act on.
void publishSingleEntry(FakeNetworkDirectoryBackend *rawBackend, NavigationController &controller,
                        const QUrl &url, const QString &name) {
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  rawBackend->emitReady(generation, url,
                        successResult(url, {makeEntry(name, NetworkLocation::childUrl(url, name).toString(), false)}));
}

// Builds a controller state of backend + folder creator injected and navigates
// to an empty remote folder, ready for create tests.
void enterEmptyRemoteFolder(FakeNetworkDirectoryBackend *rawBackend,
                            NavigationController &controller, const QUrl &url) {
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  rawBackend->emitReady(generation, url, successResult(url, {}));
}

} // namespace

// Fake-injected remote-mutation coverage for NavigationController: every
// scenario uses FakeNetworkDirectoryBackend plus FakeRemoteRenamer or
// FakeRemoteFolderCreator and synthetic smb/sftp URLs. No DNS, socket,
// SMB/SFTP server, or credential prompt is ever reachable from this file.
class TestNavigationControllerRemoteMutation final : public QObject {
  Q_OBJECT

private slots:
  void remoteRenameDispatchesAValidatedSiblingUrl();
  void remoteRenameRejectsInvalidNames();
  void remoteRenameRejectsUnlistedAndCrossFolderSources();
  void remoteRenameRejectsOverlappingOperations();
  void remoteRenameSuccessRefreshesTheListing();
  void remoteRenameFailureStaysVisibleWithoutOptimisticRename();
  void aCancelledRemoteRenameResultIsDiscarded();
  void destructionWithAPendingRemoteRenameDoesNotCrash();
  void remoteCreateDispatchesAValidatedChildUrl();
  void remoteCreateRejectsInvalidAndTraversalNames();
  void remoteCreateRejectsOverlappingOperations();
  void remoteCreateSuccessRefreshesTheListing();
  void remoteCreateFailureStaysVisibleWithoutOptimisticEntry();
  void aCancelledRemoteCreateResultIsDiscarded();
  void destructionWithAPendingRemoteCreateDoesNotCrash();
};

// ADR-0153: renaming a listed child dispatches one same-folder KIO rename
// through the injected renamer, fenced by the current listing generation.
void TestNavigationControllerRemoteMutation::remoteRenameDispatchesAValidatedSiblingUrl() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));
  QCOMPARE(controller.remoteRenameAvailable(), true);
  QCOMPARE(controller.remoteRenameBusy(), false);

  QVERIFY(controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                       QStringLiteral("report.txt")));
  QCOMPARE(rawRenamer->requests().size(), 1);
  QCOMPARE(rawRenamer->requests().constFirst().source.toString(),
           QStringLiteral("smb://server/share/notes.txt"));
  QCOMPARE(rawRenamer->requests().constFirst().destination.toString(),
           QStringLiteral("smb://server/share/report.txt"));
  QCOMPARE(rawRenamer->requests().constFirst().generation,
           rawBackend->requests().last().generation);
  QCOMPARE(controller.remoteRenameBusy(), true);
}

void TestNavigationControllerRemoteMutation::remoteRenameRejectsInvalidNames() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));

  for (const QString &badName :
       {QString(), QStringLiteral("a/b"), QStringLiteral("a\\b"), QStringLiteral("."),
        QStringLiteral(".."), QStringLiteral("a\u0000b")}) {
    QVERIFY2(!controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                           badName),
             qPrintable(QStringLiteral("name '%1' must be refused").arg(badName)));
  }
  QVERIFY(rawRenamer->requests().isEmpty());
  QCOMPARE(controller.remoteRenameBusy(), false);
}

void TestNavigationControllerRemoteMutation::remoteRenameRejectsUnlistedAndCrossFolderSources() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));

  // Not in the current listing (stale or foreign identity).
  QVERIFY(!controller.renameRemoteEntry(QStringLiteral("smb://server/share/other.txt"),
                                        QStringLiteral("renamed.txt")));
  // Listed child of a different folder.
  QVERIFY(!controller.renameRemoteEntry(QStringLiteral("smb://server/other/notes.txt"),
                                        QStringLiteral("renamed.txt")));
  // Embedded credentials are refused outright.
  QVERIFY(!controller.renameRemoteEntry(QStringLiteral("smb://user:pass@server/share/notes.txt"),
                                        QStringLiteral("renamed.txt")));
  QVERIFY(rawRenamer->requests().isEmpty());
  QCOMPARE(controller.remoteRenameBusy(), false);
}

void TestNavigationControllerRemoteMutation::remoteRenameRejectsOverlappingOperations() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));

  QVERIFY(controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                       QStringLiteral("report.txt")));
  QVERIFY(!controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                        QStringLiteral("second.txt")));
  QCOMPARE(rawRenamer->requests().size(), 1);
  QCOMPARE(controller.remoteRenameBusy(), true);
}

void TestNavigationControllerRemoteMutation::remoteRenameSuccessRefreshesTheListing() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));
  const qsizetype requestsBefore = rawBackend->requests().size();

  QVERIFY(controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                       QStringLiteral("report.txt")));
  rawRenamer->finishSuccess(rawRenamer->requests().constFirst().generation);

  // The authoritative listing is re-requested; nothing changed optimistically
  // in between.
  QCOMPARE(rawBackend->requests().size(), requestsBefore + 1);
  QCOMPARE(controller.remoteRenameBusy(), false);
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
}

void TestNavigationControllerRemoteMutation::remoteRenameFailureStaysVisibleWithoutOptimisticRename() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("sftp://server/home"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("notes.txt"));

  QVERIFY(controller.renameRemoteEntry(QStringLiteral("sftp://server/home/notes.txt"),
                                       QStringLiteral("report.txt")));
  rawRenamer->finishFailure(rawRenamer->requests().constFirst().generation,
                            QStringLiteral("synthetic rename failure"));

  QCOMPARE(controller.launchError(), QStringLiteral("synthetic rename failure"));
  // No optimistic rename: the listed entry still shows the original name and
  // no refresh was forced by the failure.
  QCOMPARE(controller.entryAt(0)->name, QStringLiteral("notes.txt"));
  QCOMPARE(controller.remoteRenameBusy(), false);
}

// ADR-0153 lifetime: after the controller leaves the remote folder, the
// cancelled rename's late result (even a failure) must not surface.
void TestNavigationControllerRemoteMutation::aCancelledRemoteRenameResultIsDiscarded() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QStringLiteral("/home/jarrod");
  lister->setResult(QStringLiteral("/home/jarrod"), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  auto *rawRenamer = renamer.get();
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, std::move(renamer));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));
  QVERIFY(controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                       QStringLiteral("report.txt")));
  const quint64 renameGeneration = rawRenamer->requests().constFirst().generation;

  controller.navigateTo(QStringLiteral("/home/jarrod"));
  QVERIFY(rawRenamer->cancelled().contains(renameGeneration));
  QVERIFY(controller.launchError().isEmpty());

  // The quiet kill delivers a result for the cancelled generation; it is
  // fenced out and stays invisible.
  rawRenamer->finishFailure(renameGeneration, QStringLiteral("synthetic rename failure"));
  QVERIFY(controller.launchError().isEmpty());
}

void TestNavigationControllerRemoteMutation::destructionWithAPendingRemoteRenameDoesNotCrash() {
  {
    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    auto *rawBackend = backend.get();
    auto renamer = std::make_unique<FakeRemoteRenamer>();
    NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                    std::make_unique<FakeFileLauncher>(), std::move(backend),
                                    nullptr, std::move(renamer));
    const QUrl url(QStringLiteral("smb://server/share"));
    publishSingleEntry(rawBackend, controller, url, QStringLiteral("notes.txt"));
    QVERIFY(controller.renameRemoteEntry(QStringLiteral("smb://server/share/notes.txt"),
                                         QStringLiteral("report.txt")));
  }
  QVERIFY(true);
}

// ADR-0154: New Folder in an active remote folder dispatches one validated
// child-directory creation through the injected creator, fenced by the
// current listing generation.
void TestNavigationControllerRemoteMutation::remoteCreateDispatchesAValidatedChildUrl() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("smb://server/share"));
  enterEmptyRemoteFolder(rawBackend, controller, url);
  QCOMPARE(controller.remoteCreateAvailable(), true);
  QCOMPARE(controller.remoteCreateBusy(), false);

  QVERIFY(controller.createRemoteFolder(QStringLiteral("New Folder")));
  QCOMPARE(rawCreator->requests().size(), 1);
  QCOMPARE(rawCreator->requests().constFirst().url.toString(),
           QStringLiteral("smb://server/share/New Folder"));
  QCOMPARE(rawCreator->requests().constFirst().generation,
           rawBackend->requests().last().generation);
  QCOMPARE(controller.remoteCreateBusy(), true);
}

void TestNavigationControllerRemoteMutation::remoteCreateRejectsInvalidAndTraversalNames() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("sftp://server/home"));
  enterEmptyRemoteFolder(rawBackend, controller, url);

  for (const QString &badName :
       {QString(), QStringLiteral("a/b"), QStringLiteral("a\\b"), QStringLiteral("."),
        QStringLiteral(".."), QStringLiteral("a\u0000b")}) {
    QVERIFY2(!controller.createRemoteFolder(badName),
             qPrintable(QStringLiteral("name '%1' must be refused").arg(badName)));
  }
  QVERIFY(rawCreator->requests().isEmpty());
  QCOMPARE(controller.remoteCreateBusy(), false);
}

void TestNavigationControllerRemoteMutation::remoteCreateRejectsOverlappingOperations() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("smb://server/share"));
  enterEmptyRemoteFolder(rawBackend, controller, url);

  QVERIFY(controller.createRemoteFolder(QStringLiteral("One")));
  QVERIFY(!controller.createRemoteFolder(QStringLiteral("Two")));
  QCOMPARE(rawCreator->requests().size(), 1);
  QCOMPARE(controller.remoteCreateBusy(), true);
}

void TestNavigationControllerRemoteMutation::remoteCreateSuccessRefreshesTheListing() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("smb://server/share"));
  enterEmptyRemoteFolder(rawBackend, controller, url);
  const qsizetype requestsBefore = rawBackend->requests().size();

  QVERIFY(controller.createRemoteFolder(QStringLiteral("New Folder")));
  rawCreator->finishSuccess(rawCreator->requests().constFirst().generation);

  // The authoritative listing is re-requested after confirmed success; no
  // optimistic entry appeared in between.
  QCOMPARE(rawBackend->requests().size(), requestsBefore + 1);
  QCOMPARE(controller.remoteCreateBusy(), false);
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
}

void TestNavigationControllerRemoteMutation::remoteCreateFailureStaysVisibleWithoutOptimisticEntry() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("sftp://server/home"));
  enterEmptyRemoteFolder(rawBackend, controller, url);
  QCOMPARE(controller.entryCount(), 0);

  QVERIFY(controller.createRemoteFolder(QStringLiteral("New Folder")));
  rawCreator->finishFailure(rawCreator->requests().constFirst().generation,
                            QStringLiteral("synthetic create failure"));

  QCOMPARE(controller.launchError(), QStringLiteral("synthetic create failure"));
  // No optimistic entry and no refresh on failure.
  QCOMPARE(controller.entryCount(), 0);
  QCOMPARE(controller.remoteCreateBusy(), false);
}

void TestNavigationControllerRemoteMutation::aCancelledRemoteCreateResultIsDiscarded() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QStringLiteral("/home/jarrod");
  lister->setResult(QStringLiteral("/home/jarrod"), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  auto *rawCreator = creator.get();
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, nullptr, std::move(creator));

  const QUrl url(QStringLiteral("smb://server/share"));
  enterEmptyRemoteFolder(rawBackend, controller, url);
  QVERIFY(controller.createRemoteFolder(QStringLiteral("New Folder")));
  const quint64 createGeneration = rawCreator->requests().constFirst().generation;

  controller.navigateTo(QStringLiteral("/home/jarrod"));
  QVERIFY(rawCreator->cancelled().contains(createGeneration));
  QVERIFY(controller.launchError().isEmpty());

  // The quiet kill delivers a result for the cancelled generation; it is
  // fenced out and stays invisible.
  rawCreator->finishFailure(createGeneration, QStringLiteral("synthetic create failure"));
  QVERIFY(controller.launchError().isEmpty());
}

void TestNavigationControllerRemoteMutation::destructionWithAPendingRemoteCreateDoesNotCrash() {
  {
    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    auto *rawBackend = backend.get();
    auto creator = std::make_unique<FakeRemoteFolderCreator>();
    NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                    std::make_unique<FakeFileLauncher>(), std::move(backend),
                                    nullptr, nullptr, std::move(creator));
    const QUrl url(QStringLiteral("smb://server/share"));
    enterEmptyRemoteFolder(rawBackend, controller, url);
    QVERIFY(controller.createRemoteFolder(QStringLiteral("New Folder")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestNavigationControllerRemoteMutation)
#include "tst_navigation_controller_network_mutation.moc"
