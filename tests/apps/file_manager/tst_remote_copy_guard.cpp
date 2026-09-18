// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/search_controller.h"
#include "window_fixtures.h"
#include "mutation/mutation_controller.h"
#include "preview/local_preview.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <qindaqt/app_shell/application_coordinator.h>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Apps::FileManager::Test::FakeDirectoryLister;
using QindaQt::Apps::FileManager::Test::FakeFileLauncher;
using QindaQt::Apps::FileManager::Test::FakeNetworkDirectoryBackend;
using QindaQt::Apps::FileManager::Test::FakeRemoteCopier;
using QindaQt::Apps::FileManager::Test::FakeRemoteMover;

namespace {

// Records whether the local-only mutation backend was asked to execute.
// The review P1 defect routed remote URL-shaped inputs into this backend's
// multi-item branch; this fake proves the production QML route never calls
// it for a remote selection. Never touches the filesystem.
class RecordingMutationBackend final : public MutationBackend {
public:
  [[nodiscard]] MutationResult
  execute(const MutationRequest &, const MutationCancellation &,
          const MutationProgressCallback &) override {
    ++m_executeCount;
    return MutationResult{};
  }

  [[nodiscard]] int executeCount() const { return m_executeCount; }

private:
  int m_executeCount = 0;
};

[[nodiscard]] bool fail(QString *error, const QString &message) {
  if (error) {
    *error = message;
  }
  return false;
}

[[nodiscard]] std::optional<bool> actionEnabled(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, const QString &id) {
  for (const auto &action : coordinator.actionRegistry().actions()) {
    if (action.id == id) {
      return action.enabled;
    }
  }
  return std::nullopt;
}

// Owns the fully wired production shell (AppShell coordinator -> Main.qml ->
// MutationDialogs.qml) over an injected fake remote tree, so tests exercise
// the real dispatch/accept/cancel paths instead of controller shortcuts.
// Mirrors the browsing-UI harness; everything is fake or temporary-dir
// based. init() reports failures as values because helpers must not host
// QTest macros (an early return would silently pass). Hosts both the Copy
// guard rows (ADR-0155 repair) and the Move To rows (ADR-0156): the
// production route is shared, only the injected collaborator differs.
struct RemoteMutationRouteFixture final {
  bool init(const QString &temporaryPath, QString *error) {
    const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
    NetworkListingResult listing;
    listing.url = QUrl(QStringLiteral("smb://server/share"));
    DirectoryEntry first;
    first.name = QStringLiteral("alpha.txt");
    first.absolutePath = QStringLiteral("smb://server/share/alpha.txt");
    first.isDirectory = false;
    DirectoryEntry second;
    second.name = QStringLiteral("beta.txt");
    second.absolutePath = QStringLiteral("smb://server/share/beta.txt");
    second.isDirectory = false;
    listing.entries = {first, second};

    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    rawBackend = backend.get();
    auto copier = std::make_unique<FakeRemoteCopier>();
    rawCopier = copier.get();
    auto mover = std::make_unique<FakeRemoteMover>();
    rawMover = mover.get();
    navigation = std::make_unique<NavigationController>(
        std::make_unique<FakeDirectoryLister>(), std::make_unique<FakeFileLauncher>(),
        std::move(backend), nullptr, nullptr, nullptr, std::move(copier),
        std::move(mover));
    auto recording = std::make_unique<RecordingMutationBackend>();
    rawRecording = recording.get();
    mutation = std::make_unique<MutationController>(std::move(recording));
    clipboard = std::make_unique<ClipboardController>(*mutation, *QGuiApplication::clipboard());
    properties = std::make_unique<EntryPropertiesController>();
    search = std::make_unique<SearchController>();
    applications = std::make_unique<ApplicationsController>(QStringList{});
    places = std::make_unique<PlacesController>(
        std::make_unique<BookmarksStore>(temporaryPath + QStringLiteral("/state")));
    // This fixture proves the ADR-0155/0156 one-child remote path and the
    // ADR-0195 queue routing; nothing else the window needs does anything.
    support = std::make_unique<Test::WindowSupportControllers>(temporaryPath);

    const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
    if (!catalogResult.ok()) {
      return fail(error, QStringLiteral("the action catalog must install: %1")
                             .arg(catalogResult.message));
    }
    bindFileManagerBrowsingActions(coordinator, *navigation);
    bindFileManagerMutationActions(coordinator, *navigation, *mutation);

    navigation->navigateTo(QStringLiteral("smb://server/share"));
    rawBackend->emitReady(rawBackend->requests().constLast().generation,
                          QUrl(QStringLiteral("smb://server/share")), listing);

    // The QML engine takes ownership of image providers, so this is a raw
    // pointer the fixture deliberately does not delete (same contract as the
    // browsing-UI harness).
    previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
    engine = std::make_unique<QQmlApplicationEngine>();
    engine->addImageProvider(QStringLiteral("previews"), previews);
    engine->addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
    previews->setGeneration(navigation->listingGeneration());
    QVariantMap initialProperties{
        {"navigationController", QVariant::fromValue(static_cast<QObject *>(navigation.get()))},
        {"mutationController", QVariant::fromValue(static_cast<QObject *>(mutation.get()))},
        {"clipboardController", QVariant::fromValue(static_cast<QObject *>(clipboard.get()))},
        {"propertiesController", QVariant::fromValue(static_cast<QObject *>(properties.get()))},
        {"searchController", QVariant::fromValue(static_cast<QObject *>(search.get()))},
        {"placesController", QVariant::fromValue(static_cast<QObject *>(places.get()))},
        {"applicationsController", QVariant::fromValue(static_cast<QObject *>(applications.get()))},
        {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}};
    support->insertInto(initialProperties);
    engine->setInitialProperties(initialProperties);
    engine->load(QUrl::fromLocalFile(sourceRoot + QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
    if (engine->rootObjects().isEmpty()) {
      return fail(error, QStringLiteral("Main.qml must load"));
    }
    window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
    if (!window) {
      return fail(error, QStringLiteral("Main.qml must produce a window"));
    }
    window->resize(QSize(1280, 800));
    window->requestActivate();
    QElapsedTimer exposeTimer;
    exposeTimer.start();
    while (!window->isExposed() && exposeTimer.elapsed() < 5000) {
      QCoreApplication::processEvents();
    }
    if (!window->isExposed()) {
      return fail(error, QStringLiteral("the window must expose offscreen"));
    }

    navigation->setViewMode(QStringLiteral("list"));
    listView = window->findChild<QQuickItem *>(QStringLiteral("entryListView"));
    if (!listView) {
      return fail(error, QStringLiteral("the list view must exist"));
    }
    QElapsedTimer entriesTimer;
    entriesTimer.start();
    while (navigation->entryCount() != 2 && entriesTimer.elapsed() < 5000) {
      QCoreApplication::processEvents();
    }
    if (navigation->entryCount() != 2) {
      return fail(error, QStringLiteral("the fake remote listing must publish two entries"));
    }
    return true;
  }

  QindaQt::AppShell::ApplicationCoordinator coordinator;
  std::unique_ptr<NavigationController> navigation;
  std::unique_ptr<MutationController> mutation;
  std::unique_ptr<ClipboardController> clipboard;
  std::unique_ptr<EntryPropertiesController> properties;
  std::unique_ptr<SearchController> search;
  std::unique_ptr<ApplicationsController> applications;
  std::unique_ptr<PlacesController> places;
  std::unique_ptr<Test::WindowSupportControllers> support;
  // Owned by the QML engine (see init()); not deleted here.
  PreviewProvider *previews = nullptr;
  std::unique_ptr<QQmlApplicationEngine> engine;
  QQuickWindow *window = nullptr;
  QQuickItem *listView = nullptr;
  FakeNetworkDirectoryBackend *rawBackend = nullptr;
  FakeRemoteCopier *rawCopier = nullptr;
  FakeRemoteMover *rawMover = nullptr;
  RecordingMutationBackend *rawRecording = nullptr;
};

} // namespace

// The production route for remote Copy To / Move To, through the real
// coordinator, Main.qml, and MutationDialogs.
//
// ADR-0195 changed *where* the ADR-0155/0156 one-child contract is enforced,
// not whether it is: a remote multi-selection now opens the destination
// dialog and is routed -- by C++, from the exact endpoint pair -- to the
// transfer queue, while exactly one remote child to the same authority still
// reaches the injected one-child copier/mover. The invariant these rows exist
// for is unchanged and still asserted at every step: the local-only mutation
// backend's multi-item branch never receives remote URL-shaped inputs.
// The shared Cancel action still retires an in-flight remote copy or move
// with a generation-fenced late result.
class TestRemoteCopyGuard final : public QObject {
  Q_OBJECT

private slots:
  void remoteMultiSelectionCopyRoutesToTheQueueNotTheLocalBackend();
  void sharedCancelRoutesToTheInFlightRemoteCopy();
  void remoteMultiSelectionMoveRoutesToTheQueueNotTheLocalBackend();
  void sharedCancelRoutesToTheInFlightRemoteMove();
};

void TestRemoteCopyGuard::remoteMultiSelectionCopyRoutesToTheQueueNotTheLocalBackend() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  RemoteMutationRouteFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));
  QCOMPARE(fixture.navigation->remoteActive(), true);
  QCOMPARE(fixture.navigation->remoteCopyAvailable(), true);

  QObject *destinationDialog =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationDialog"));
  QVERIFY(destinationDialog);

  // Two-entry selection through the production select-all action (the same
  // seam Main.qml's Ctrl+A uses), then prove via the production context
  // menu that this really is the multi-selection the guard targets.
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("edit.select-all")));
  QCoreApplication::processEvents();
  fixture.listView->forceActiveFocus();
  QTRY_VERIFY(fixture.listView->hasActiveFocus());
  QTest::keyClick(fixture.window, Qt::Key_Menu);
  QObject *contextMenu =
      fixture.window->findChild<QObject *>(QStringLiteral("listContextMenu"));
  QVERIFY(contextMenu);
  QTRY_COMPARE(contextMenu->property("selectionCount").toInt(), 2);
  QMetaObject::invokeMethod(contextMenu, "close");

  // The shared Copy action: coordinator -> Main.qml -> MutationDialogs.dispatch.
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.copy")));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *multiDestinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(multiDestinationField);
  QVERIFY(multiDestinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  // ADR-0195: two sources become two queue items. The one-child copier is
  // untouched, and the local mutation backend still saw nothing.
  QCOMPARE(fixture.support->transfers.itemValues().size(), 2);
  QCOMPARE(fixture.support->transfers.itemValues().constFirst().destinationFolder.toString(),
           QStringLiteral("smb://server/backup"));
  QCOMPARE(fixture.support->transfers.itemValues().constFirst().operation,
           TransferOperation::Copy);
  QCOMPARE(fixture.rawCopier->requests().size(), 0);
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
  QVERIFY(fixture.support->transfers.refusal().isEmpty());

  // Exactly one entry: the dialog opens and the accepted destination routes
  // to the injected copier, still never to the local backend.
  QVERIFY(QMetaObject::invokeMethod(fixture.listView, "selectEntry",
                                    Q_ARG(QVariant, QVariant(0))));
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.copy")));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *destinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(destinationField);
  QVERIFY(destinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.rawCopier->requests().size(), 1);
  QCOMPARE(fixture.rawCopier->requests().constFirst().source.toString(),
           QStringLiteral("smb://server/share/alpha.txt"));
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
}

void TestRemoteCopyGuard::sharedCancelRoutesToTheInFlightRemoteCopy() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  RemoteMutationRouteFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  // Start one validated remote copy and leave it in flight.
  QVERIFY(QMetaObject::invokeMethod(fixture.listView, "selectEntry",
                                    Q_ARG(QVariant, QVariant(0))));
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.copy")));
  QObject *destinationDialog =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationDialog"));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *destinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(destinationField);
  QVERIFY(destinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.rawCopier->requests().size(), 1);
  QCOMPARE(fixture.mutation->busy(), false);
  QCOMPARE(fixture.rawRecording->executeCount(), 0);

  // The shared Cancel action is truthfully enabled for the remote owner even
  // though the local mutation backend is idle (former red: it stayed
  // disabled), and dispatching it retires the copy through the copier.
  QCOMPARE(actionEnabled(fixture.coordinator, QStringLiteral("operation.cancel")),
           std::optional<bool>(true));
  const quint64 copyGeneration = fixture.rawCopier->requests().constFirst().generation;
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("operation.cancel")));
  QCoreApplication::processEvents();
  QVERIFY(fixture.rawCopier->cancelled().contains(copyGeneration));
  QCOMPARE(fixture.navigation->remoteCopyBusy(), false);

  // The quiet kill's late result is generation-fenced: no visible failure.
  fixture.rawCopier->finishFailure(copyGeneration,
                                   QStringLiteral("synthetic copy failure"));
  QCoreApplication::processEvents();
  QVERIFY(fixture.navigation->launchError().isEmpty());
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
}

// ADR-0156 former red: the pre-slice QML only fenced remote Copy, so a
// remote multi-selection Move reached the local-only backend's multi-item
// branch with remote URL-shaped inputs. ADR-0195 routes it to the transfer
// queue instead of refusing to open the dialog; one selected child still
// routes the accepted move to the injected mover, and neither path ever
// reaches the local backend.
void TestRemoteCopyGuard::remoteMultiSelectionMoveRoutesToTheQueueNotTheLocalBackend() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  RemoteMutationRouteFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));
  QCOMPARE(fixture.navigation->remoteActive(), true);
  QCOMPARE(fixture.navigation->remoteMoveAvailable(), true);

  QObject *destinationDialog =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationDialog"));
  QVERIFY(destinationDialog);

  // Two-entry selection through the production select-all action.
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("edit.select-all")));
  QCoreApplication::processEvents();

  // The shared Move action: coordinator -> Main.qml -> MutationDialogs.dispatch.
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.move")));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *multiDestinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(multiDestinationField);
  QVERIFY(multiDestinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  // ADR-0195: two sources become two queued moves. The destructive one-child
  // mover is untouched, and the local mutation backend still saw nothing.
  QCOMPARE(fixture.support->transfers.itemValues().size(), 2);
  QCOMPARE(fixture.support->transfers.itemValues().constFirst().operation,
           TransferOperation::Move);
  QCOMPARE(fixture.rawMover->requests().size(), 0);
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
  QVERIFY(fixture.support->transfers.refusal().isEmpty());

  // Exactly one entry: the dialog opens and the accepted destination routes
  // to the injected mover, still never to the local backend.
  QVERIFY(QMetaObject::invokeMethod(fixture.listView, "selectEntry",
                                    Q_ARG(QVariant, QVariant(0))));
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.move")));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *destinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(destinationField);
  QVERIFY(destinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.rawMover->requests().size(), 1);
  QCOMPARE(fixture.rawMover->requests().constFirst().source.toString(),
           QStringLiteral("smb://server/share/alpha.txt"));
  QCOMPARE(fixture.rawMover->requests().constFirst().destination.toString(),
           QStringLiteral("smb://server/backup/alpha.txt"));
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
}

// ADR-0156: the shared Cancel action retires an in-flight remote move
// through the injected mover (quiet kill, generation-fenced late result)
// exactly like the copy repair, and is truthfully enabled while the local
// mutation backend is idle.
void TestRemoteCopyGuard::sharedCancelRoutesToTheInFlightRemoteMove() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  RemoteMutationRouteFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  // Start one validated remote move and leave it in flight.
  QVERIFY(QMetaObject::invokeMethod(fixture.listView, "selectEntry",
                                    Q_ARG(QVariant, QVariant(0))));
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("file.move")));
  QObject *destinationDialog =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationDialog"));
  QTRY_VERIFY(destinationDialog->property("visible").toBool());
  QObject *destinationField =
      fixture.window->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QVERIFY(destinationField);
  QVERIFY(destinationField->setProperty("text", QStringLiteral("smb://server/backup")));
  QVERIFY(QMetaObject::invokeMethod(destinationDialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.rawMover->requests().size(), 1);
  QCOMPARE(fixture.mutation->busy(), false);
  QCOMPARE(fixture.rawRecording->executeCount(), 0);

  QCOMPARE(actionEnabled(fixture.coordinator, QStringLiteral("operation.cancel")),
           std::optional<bool>(true));
  const quint64 moveGeneration = fixture.rawMover->requests().constFirst().generation;
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("operation.cancel")));
  QCoreApplication::processEvents();
  QVERIFY(fixture.rawMover->cancelled().contains(moveGeneration));
  QCOMPARE(fixture.navigation->remoteMoveBusy(), false);

  // The quiet kill's late result is generation-fenced: no visible failure.
  fixture.rawMover->finishFailure(moveGeneration,
                                  QStringLiteral("synthetic move failure"));
  QCoreApplication::processEvents();
  QVERIFY(fixture.navigation->launchError().isEmpty());
  QCOMPARE(fixture.rawRecording->executeCount(), 0);
}

QTEST_MAIN(TestRemoteCopyGuard)
#include "tst_remote_copy_guard.moc"
