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
using QindaQt::Apps::FileManager::Test::FakeRemoteMover;

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

// Builds a controller with backend + mover injected and publishes a single
// listed child, ready for move tests.
void publishRemoteTree(FakeNetworkDirectoryBackend *rawBackend, NavigationController &controller,
                       const QUrl &url, const QString &name, bool isDirectory) {
  controller.navigateTo(url.toString());
  const quint64 generation = rawBackend->requests().last().generation;
  rawBackend->emitReady(generation, url,
                        successResult(url, {makeEntry(name, NetworkLocation::childUrl(url, name).toString(),
                                                      isDirectory)}));
}

} // namespace

// ADR-0156 fake-injected remote Move To coverage for NavigationController:
// every scenario uses FakeNetworkDirectoryBackend plus FakeRemoteMover and
// synthetic smb/sftp URLs. No DNS, socket, SMB/SFTP server, or credential
// prompt is ever reachable from this file. Split from the copy rows because
// a confirmed move always removes the source from the visible folder, which
// is the discriminating difference from Copy To's destination-scoped refresh.
class TestRemoteMoveDispatch final : public QObject {
  Q_OBJECT

private slots:
  void remoteMoveDispatchesAValidatedDestination();
  void remoteMoveRejectsUnlistedSourcesAndBadDestinations();
  void remoteMoveRejectsSameTargetAndDirectorySelfMoves();
  void remoteMoveSuccessAlwaysRefreshesTheCurrentFolder();
  void remoteMoveFailureStaysVisibleWithoutOptimisticDisplay();
  void aCancelledRemoteMoveResultIsDiscarded();
  void directUserCancellationRetiresTheRemoteMove();
  // Review P1 repair (former red on e88d2e35): Cancel followed by an
  // immediate retry in the same unchanged listing must not let the
  // cancelled move's late result retire, fail, or refresh the replacement.
  void cancelThenRetryInSameListingIgnoresTheOldMovesLateFailure();
  void cancelThenRetryInSameListingIgnoresTheOldMovesLateSuccess();
  void destructionWithAPendingRemoteMoveDoesNotCrash();
};

// ADR-0156: Move To dispatches one validated move (listed child -> distinct
// child target of the canonical destination folder), fenced by the current
// listing generation.
void TestRemoteMoveDispatch::remoteMoveDispatchesAValidatedDestination() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  QCOMPARE(controller.remoteMoveAvailable(), true);
  QCOMPARE(controller.remoteMoveBusy(), false);

  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  QCOMPARE(rawMover->requests().size(), 1);
  QCOMPARE(rawMover->requests().constFirst().source.toString(),
           QStringLiteral("smb://server/share/notes.txt"));
  QCOMPARE(rawMover->requests().constFirst().destination.toString(),
           QStringLiteral("smb://server/backup/notes.txt"));
  QCOMPARE(rawMover->requests().constFirst().generation,
           rawBackend->requests().last().generation);
  QCOMPARE(controller.remoteMoveBusy(), true);
}

void TestRemoteMoveDispatch::remoteMoveRejectsUnlistedSourcesAndBadDestinations() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("sftp://server/home"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);

  // Not a listed child of the current folder.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("sftp://server/home/other.txt"),
                                      QStringLiteral("sftp://server/backup")));
  // Destination from the wrong authority.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("sftp://server/home/notes.txt"),
                                      QStringLiteral("smb://server/backup")));
  // Malformed destination.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("sftp://server/home/notes.txt"),
                                      QStringLiteral("not a url")));
  // Embedded credentials in the destination.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("sftp://server/home/notes.txt"),
                                      QStringLiteral("sftp://user:pass@server/backup")));
  QVERIFY(rawMover->requests().isEmpty());
  QCOMPARE(controller.remoteMoveBusy(), false);
}

void TestRemoteMoveDispatch::remoteMoveRejectsSameTargetAndDirectorySelfMoves() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  // Same target: destination folder == source folder.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                      QStringLiteral("smb://server/share")));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("Docs"), true);
  // Directory into itself.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("smb://server/share/Docs"),
                                      QStringLiteral("smb://server/share/Docs")));
  // Directory into its own descendant.
  QVERIFY(!controller.moveRemoteChild(QStringLiteral("smb://server/share/Docs"),
                                      QStringLiteral("smb://server/share/Docs/2026")));
  QVERIFY(rawMover->requests().isEmpty());
  QCOMPARE(controller.remoteMoveBusy(), false);
}

// The discriminating difference from Copy To (ADR-0155): the moved source is
// always a listed child of the folder being viewed, so a confirmed success
// always removes it from view -- the authoritative listing re-reads no matter
// where the destination was. No optimistic disappearance before confirmation.
void TestRemoteMoveDispatch::remoteMoveSuccessAlwaysRefreshesTheCurrentFolder() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);

  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const qsizetype requestsBefore = rawBackend->requests().size();
  QCOMPARE(controller.entryCount(), 1);
  rawMover->finishSuccess(rawMover->requests().constFirst().generation);
  QCOMPARE(rawBackend->requests().size(), requestsBefore + 1);
  QCOMPARE(controller.remoteMoveBusy(), false);
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
}

void TestRemoteMoveDispatch::remoteMoveFailureStaysVisibleWithoutOptimisticDisplay() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("sftp://server/home"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  QCOMPARE(controller.entryCount(), 1);

  QVERIFY(controller.moveRemoteChild(QStringLiteral("sftp://server/home/notes.txt"),
                                     QStringLiteral("sftp://server/backup")));
  rawMover->finishFailure(rawMover->requests().constFirst().generation,
                          QStringLiteral("synthetic move failure"));

  QCOMPARE(controller.launchError(), QStringLiteral("synthetic move failure"));
  // KIO may have deleted the source on a mid-move failure; either way this
  // slice never hides the entry optimistically and forces no refresh here.
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.remoteMoveBusy(), false);
}

// ADR-0156 lifetime: after the controller leaves the remote folder, the
// cancelled move's late result (even a failure) must not surface.
void TestRemoteMoveDispatch::aCancelledRemoteMoveResultIsDiscarded() {
  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QStringLiteral("/home/jarrod");
  lister->setResult(QStringLiteral("/home/jarrod"), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, nullptr, nullptr, nullptr,
                                  std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 moveGeneration = rawMover->requests().constFirst().generation;

  controller.navigateTo(QStringLiteral("/home/jarrod"));
  QVERIFY(rawMover->cancelled().contains(moveGeneration));
  QVERIFY(controller.launchError().isEmpty());

  // The quiet kill delivers a result for the cancelled generation; it is
  // fenced out and stays invisible.
  rawMover->finishFailure(moveGeneration, QStringLiteral("synthetic move failure"));
  QVERIFY(controller.launchError().isEmpty());
}

// ADR-0156: cancelRemoteMove() is the user-facing hook behind the shared
// operation.cancel action. It must retire the in-flight move in place --
// quiet mover cancel, no listing re-read, no visible failure -- exactly like
// the copy repair's directUserCancellationRetiresTheRemoteCopy.
void TestRemoteMoveDispatch::directUserCancellationRetiresTheRemoteMove() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  // Idle cancel is a harmless no-op.
  controller.cancelRemoteMove();
  QVERIFY(rawMover->cancelled().isEmpty());

  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 moveGeneration = rawMover->requests().constFirst().generation;
  const qsizetype requestsBefore = rawBackend->requests().size();

  controller.cancelRemoteMove();
  QVERIFY(rawMover->cancelled().contains(moveGeneration));
  QCOMPARE(controller.remoteMoveBusy(), false);
  // No navigation happened: the folder stays active and nothing re-reads.
  QCOMPARE(controller.remoteActive(), true);
  QCOMPARE(rawBackend->requests().size(), requestsBefore);
  QVERIFY(controller.launchError().isEmpty());

  // The quiet kill's late result is generation-fenced and stays invisible.
  rawMover->finishFailure(moveGeneration, QStringLiteral("synthetic move failure"));
  QVERIFY(controller.launchError().isEmpty());
  QCOMPARE(controller.remoteMoveBusy(), false);
}

// Review P1 repair (former red): the rejected candidate fenced results
// with the listing generation, which a Cancel -> immediate retry in the
// same unchanged listing reuses -- the cancelled move's late failure then
// retired the replacement and exposed the stale diagnostic. The repair
// gives every accepted move a monotonic, never-reused operation identity
// (still distinct across the two dispatches below), so the old failure
// must stay invisible: the replacement stays busy, no error surfaces, no
// refresh fires, and only the replacement's own completion is authoritative.
void TestRemoteMoveDispatch::cancelThenRetryInSameListingIgnoresTheOldMovesLateFailure() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  const qsizetype listingRequests = rawBackend->requests().size();

  // Move A against the current listing generation, then Cancel it.
  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 operationA = rawMover->requests().constFirst().generation;
  controller.cancelRemoteMove();
  QVERIFY(rawMover->cancelled().contains(operationA));

  // Move B in the SAME unchanged listing: the listing generation is
  // identical, so only a non-reused operation identity can keep A's late
  // result from aliasing B.
  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 operationB = rawMover->requests().constLast().generation;
  QVERIFY2(operationB != operationA,
           "each accepted move must carry a non-reused operation identity");

  // A's late failure must not retire, fail, or refresh B.
  rawMover->finishFailure(operationA, QStringLiteral("stale move failure"));
  QCOMPARE(controller.remoteMoveBusy(), true);
  QVERIFY(controller.launchError().isEmpty());
  QCOMPARE(rawBackend->requests().size(), listingRequests);

  // B's own completion stays authoritative: its failure surfaces and only
  // its failure forces no refresh (a move failure changes nothing visible).
  rawMover->finishFailure(operationB, QStringLiteral("replacement move failure"));
  QCOMPARE(controller.remoteMoveBusy(), false);
  QCOMPARE(controller.launchError(), QStringLiteral("replacement move failure"));
  QCOMPARE(rawBackend->requests().size(), listingRequests);
}

// The success ordering of the same alias: A's late success must not
// trigger the authoritative refresh belonging to B's completion -- B stays
// busy, no refresh fires, and B's own success later refreshes exactly once.
void TestRemoteMoveDispatch::cancelThenRetryInSameListingIgnoresTheOldMovesLateSuccess() {
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto mover = std::make_unique<FakeRemoteMover>();
  auto *rawMover = mover.get();
  NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(), std::move(backend),
                                  nullptr, nullptr, nullptr, nullptr, std::move(mover));

  const QUrl url(QStringLiteral("smb://server/share"));
  publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
  const qsizetype listingRequests = rawBackend->requests().size();

  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 operationA = rawMover->requests().constFirst().generation;
  controller.cancelRemoteMove();
  QVERIFY(rawMover->cancelled().contains(operationA));

  QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  const quint64 operationB = rawMover->requests().constLast().generation;
  QVERIFY2(operationB != operationA,
           "each accepted move must carry a non-reused operation identity");

  // A's late success must not refresh for B.
  rawMover->finishSuccess(operationA);
  QCOMPARE(controller.remoteMoveBusy(), true);
  QCOMPARE(rawBackend->requests().size(), listingRequests);

  // B's own confirmed success is the single authoritative refresh.
  rawMover->finishSuccess(operationB);
  QCOMPARE(controller.remoteMoveBusy(), false);
  QCOMPARE(rawBackend->requests().size(), listingRequests + 1);
  QCOMPARE(controller.statusKey(), QStringLiteral("loading"));
}

void TestRemoteMoveDispatch::destructionWithAPendingRemoteMoveDoesNotCrash() {
  {
    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    auto *rawBackend = backend.get();
    auto mover = std::make_unique<FakeRemoteMover>();
    NavigationController controller(std::make_unique<FakeDirectoryLister>(),
                                    std::make_unique<FakeFileLauncher>(), std::move(backend),
                                    nullptr, nullptr, nullptr, nullptr, std::move(mover));
    const QUrl url(QStringLiteral("smb://server/share"));
    publishRemoteTree(rawBackend, controller, url, QStringLiteral("notes.txt"), false);
    QVERIFY(controller.moveRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                       QStringLiteral("smb://server/backup")));
  }
  QVERIFY(true);
}

QTEST_GUILESS_MAIN(TestRemoteMoveDispatch)
#include "tst_remote_move_dispatch.moc"
