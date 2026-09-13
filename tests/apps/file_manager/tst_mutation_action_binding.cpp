// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"

#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "model/navigation_controller.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>
#include <optional>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Apps::FileManager::Test::FakeDirectoryLister;
using QindaQt::Apps::FileManager::Test::FakeFileLauncher;
using QindaQt::Apps::FileManager::Test::FakeNetworkDirectoryBackend;
using QindaQt::Apps::FileManager::Test::FakeRemoteCopier;
using QindaQt::Apps::FileManager::Test::FakeRemoteFolderCreator;
using QindaQt::Apps::FileManager::Test::FakeRemoteRenamer;

namespace {

// Blocks execute() on the caller-controlled gate so a test can observe the
// coordinator's action-enabled state while a mutation is genuinely busy,
// then release it deterministically. No filesystem access.
class GatedMutationBackend final : public MutationBackend {
public:
  [[nodiscard]] MutationResult
  execute(const MutationRequest &, const MutationCancellation &cancellation,
          const MutationProgressCallback &) override {
    while (!m_release.load() && !cancellation->load()) {
      QThread::msleep(1);
    }
    MutationResult result;
    result.outputPath = QStringLiteral("/tmp/gated");
    return result;
  }

  void release() { m_release.store(true); }

private:
  std::atomic_bool m_release{false};
};

[[nodiscard]] std::optional<bool> actionEnabled(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, const QString &id) {
  for (const auto &action : coordinator.actionRegistry().actions()) {
    if (action.id == id) {
      return action.enabled;
    }
  }
  return std::nullopt;
}

void pumpEvents(int ms = 20) {
  QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
}

[[nodiscard]] bool waitUntilIdle(MutationController &mutation) {
  QElapsedTimer timer;
  timer.start();
  while (mutation.busy() && timer.elapsed() < 5000) {
    pumpEvents();
    QThread::msleep(1);
  }
  pumpEvents();
  return !mutation.busy();
}

} // namespace

// Repairs review verdict `20260912T182254-0600-verdict-reject-da0d1d55.md`
// P2-A/P2-B/P2-C: proves the coordinator's actual action-enabled state (not
// just controller-level fields) for the current-folder mutation actions,
// Empty Trash, and view.filter/search under a remote (smb/sftp) location and
// under a busy mutation -- the two axes the rejected candidate conflated.
class TestMutationActionBinding final : public QObject {
  Q_OBJECT

private slots:
  void remoteBrowsingDisablesFolderMutationsButNotEmptyTrashOrHistory();
  void remoteBrowsingKeepsRenameEnabledWhenARenamerIsInjected();
  void remoteBrowsingKeepsNewFolderEnabledWhenACreatorIsInjected();
  void remoteBrowsingKeepsCopyEnabledWhenACopierIsInjected();
  void remoteBrowsingDisablesSearchAndFilter();
  void busyMutationDisablesEmptyTrashRegardlessOfRemoteState();
  // Review P1 repair (former red): the shared Cancel action enables while a
  // remote copy is in flight, not only for local mutation work.
  void remoteCopyDrivesTheSharedCancelAction();
};

void TestMutationActionBinding::remoteBrowsingDisablesFolderMutationsButNotEmptyTrashOrHistory() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QDir::tempPath();
  lister->setResult(QDir::tempPath(), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  NavigationController navigation(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend));
  MutationController mutation(std::make_unique<GatedMutationBackend>());

  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  QCOMPARE(navigation.remoteActive(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.new-folder")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(navigation.remoteActive(), true);

  // The rejected candidate's own AGENT-GUARD and ADR-0137 both state these
  // four stay enabled while remote; only current-folder mutations disable.
  for (const char *actionId :
       {"file.new-folder", "file.rename", "file.copy", "file.move", "file.trash"}) {
    QCOMPARE(actionEnabled(coordinator, QLatin1String(actionId)), std::optional<bool>(false));
  }
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("edit.undo")), std::optional<bool>(false));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.restore-last")), std::optional<bool>(false));

  navigation.navigateTo(QDir::tempPath());
  QCOMPARE(navigation.remoteActive(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.new-folder")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));
}

// ADR-0153 fail-before/green row: with a RemoteRenamer injected, Rename is
// the one current-folder mutation that stays available while browsing a
// remote folder; without one it keeps disabling exactly as before.
void TestMutationActionBinding::remoteBrowsingKeepsRenameEnabledWhenARenamerIsInjected() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QDir::tempPath();
  lister->setResult(QDir::tempPath(), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto renamer = std::make_unique<FakeRemoteRenamer>();
  NavigationController navigation(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, std::move(renamer));
  MutationController mutation(std::make_unique<GatedMutationBackend>());

  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(navigation.remoteActive(), true);
  QCOMPARE(navigation.remoteRenameAvailable(), true);
  // Every other current-folder mutation stays disabled while remote.
  for (const char *actionId :
       {"file.new-folder", "file.copy", "file.move", "file.trash"}) {
    QCOMPARE(actionEnabled(coordinator, QLatin1String(actionId)), std::optional<bool>(false));
  }
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.rename")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));

  navigation.navigateTo(QDir::tempPath());
  QCOMPARE(navigation.remoteActive(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.rename")), std::optional<bool>(true));
}

// ADR-0154 fail-before/green row: with a RemoteFolderCreator injected, New
// Folder is available while browsing a remote folder; without one it keeps
// disabling with the other current-folder mutations.
void TestMutationActionBinding::remoteBrowsingKeepsNewFolderEnabledWhenACreatorIsInjected() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QDir::tempPath();
  lister->setResult(QDir::tempPath(), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto creator = std::make_unique<FakeRemoteFolderCreator>();
  NavigationController navigation(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, nullptr, std::move(creator));
  MutationController mutation(std::make_unique<GatedMutationBackend>());

  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(navigation.remoteActive(), true);
  QCOMPARE(navigation.remoteCreateAvailable(), true);
  // Every other current-folder mutation except Rename stays disabled while
  // remote (Rename needs its own renamer, absent here).
  for (const char *actionId : {"file.rename", "file.copy", "file.move", "file.trash"}) {
    QCOMPARE(actionEnabled(coordinator, QLatin1String(actionId)), std::optional<bool>(false));
  }
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.new-folder")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));

  navigation.navigateTo(QDir::tempPath());
  QCOMPARE(navigation.remoteActive(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.new-folder")), std::optional<bool>(true));
}

// ADR-0155 fail-before/green row: with a RemoteCopier injected, Copy To is
// available while browsing a remote folder; without one it keeps disabling
// with the other current-folder mutations.
void TestMutationActionBinding::remoteBrowsingKeepsCopyEnabledWhenACopierIsInjected() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QDir::tempPath();
  lister->setResult(QDir::tempPath(), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto copier = std::make_unique<FakeRemoteCopier>();
  NavigationController navigation(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, nullptr, nullptr,
                                  std::move(copier));
  MutationController mutation(std::make_unique<GatedMutationBackend>());

  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(navigation.remoteActive(), true);
  QCOMPARE(navigation.remoteCopyAvailable(), true);
  // Every other current-folder mutation stays disabled while remote
  // (rename/new-folder need their own seams, absent here).
  for (const char *actionId :
       {"file.new-folder", "file.rename", "file.move", "file.trash"}) {
    QCOMPARE(actionEnabled(coordinator, QLatin1String(actionId)), std::optional<bool>(false));
  }
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.copy")), std::optional<bool>(true));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));

  navigation.navigateTo(QDir::tempPath());
  QCOMPARE(navigation.remoteActive(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.copy")), std::optional<bool>(true));
}

void TestMutationActionBinding::remoteBrowsingDisablesSearchAndFilter() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto lister = std::make_unique<FakeDirectoryLister>();
  ListingResult ready;
  ready.path = QDir::tempPath();
  lister->setResult(QDir::tempPath(), ready);
  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  NavigationController navigation(std::move(lister), std::make_unique<FakeFileLauncher>(),
                                  std::move(backend));
  bindFileManagerBrowsingActions(coordinator, navigation);

  // Ctrl+F, the View menu, and the exported native menu all read this one
  // flag; a visibly enabled entry point led to SearchController::startSearch
  // silently refusing on a non-local root (review P2-B).
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("view.filter")), std::optional<bool>(true));

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("view.filter")), std::optional<bool>(false));

  navigation.navigateTo(QDir::tempPath());
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("view.filter")), std::optional<bool>(true));
}

void TestMutationActionBinding::busyMutationDisablesEmptyTrashRegardlessOfRemoteState() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  NavigationController navigation(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(),
                                  std::move(backend));
  auto gated = std::make_unique<GatedMutationBackend>();
  auto *rawGated = gated.get();
  MutationController mutation(std::move(gated));
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QVERIFY(mutation.createFolder(tempDir.path(), QStringLiteral("gated-folder")));
  QVERIFY(mutation.busy());
  // Busy gates Empty Trash independent of the remote state checked above.
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(false));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("operation.cancel")), std::optional<bool>(true));

  rawGated->release();
  QVERIFY(waitUntilIdle(mutation));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.empty-trash")), std::optional<bool>(true));
}

// Review P1 repair (former red): with a RemoteCopier injected and one copy
// dispatched, the shared operation.cancel action must be enabled even though
// the local mutation backend is idle, and must disable again when the copy
// finishes. This is the coordinator-level half of the Cancel fix; the QML
// routing to the remote owner is covered by the production-route guard test.
void TestMutationActionBinding::remoteCopyDrivesTheSharedCancelAction() {
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
  QVERIFY2(catalogResult.ok(), qPrintable(catalogResult.message));

  auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
  auto *rawBackend = backend.get();
  auto copier = std::make_unique<FakeRemoteCopier>();
  auto *rawCopier = copier.get();
  NavigationController navigation(std::make_unique<FakeDirectoryLister>(),
                                  std::make_unique<FakeFileLauncher>(),
                                  std::move(backend), nullptr, nullptr, nullptr,
                                  std::move(copier));
  MutationController mutation(std::make_unique<GatedMutationBackend>());

  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerMutationActions(coordinator, navigation, mutation);

  navigation.navigateTo(QStringLiteral("smb://server/share"));
  NetworkListingResult listing;
  listing.url = QUrl(QStringLiteral("smb://server/share"));
  DirectoryEntry entry;
  entry.name = QStringLiteral("notes.txt");
  entry.absolutePath = QStringLiteral("smb://server/share/notes.txt");
  entry.isDirectory = false;
  listing.entries = {entry};
  rawBackend->emitReady(rawBackend->requests().constLast().generation,
                        QUrl(QStringLiteral("smb://server/share")), listing);
  pumpEvents();
  QCOMPARE(navigation.entryCount(), 1);

  // Idle locally and remotely: Cancel stays disabled.
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("operation.cancel")),
           std::optional<bool>(false));

  QVERIFY(navigation.copyRemoteChild(QStringLiteral("smb://server/share/notes.txt"),
                                     QStringLiteral("smb://server/backup")));
  // The local backend never sees the remote copy, yet Cancel is reachable.
  QCOMPARE(mutation.busy(), false);
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("operation.cancel")),
           std::optional<bool>(true));
  // Copy stays disabled while its own operation is in flight.
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.copy")), std::optional<bool>(false));

  rawCopier->finishSuccess(rawCopier->requests().constFirst().generation);
  pumpEvents();
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("operation.cancel")),
           std::optional<bool>(false));
  QCOMPARE(actionEnabled(coordinator, QStringLiteral("file.copy")), std::optional<bool>(true));
}

QTEST_MAIN(TestMutationActionBinding)
#include "tst_mutation_action_binding.moc"
