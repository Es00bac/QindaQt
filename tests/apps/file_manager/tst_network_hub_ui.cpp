// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/search_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "network/network_locations_controller.h"
#include "network/network_locations_store.h"
#include "network/transfer_queue_controller.h"
#include "preview/local_preview.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;
using namespace QindaQt::Apps::FileManager::Test;

namespace {

bool fail(QString *error, const QString &message) {
  *error = message;
  return false;
}

// AGENT-NOTE: existence and `visible` are not evidence that a page renders.
// This walks the real parent chain to the window's content item and demands
// a positive painted size, which is what catches a page that loaded into a
// zero-sized or unparented layout slot.
[[nodiscard]] bool rendersInsideWindow(QQuickItem *item, QQuickWindow *window) {
  if (item == nullptr || window == nullptr || item->window() != window) {
    return false;
  }
  if (!item->isVisible() || item->width() <= 0.0 || item->height() <= 0.0) {
    return false;
  }
  for (QQuickItem *ancestor = item->parentItem(); ancestor != nullptr;
       ancestor = ancestor->parentItem()) {
    if (ancestor == window->contentItem()) {
      return true;
    }
  }
  return false;
}

// AGENT-NOTE: a Repeater delegate is a *visual* child of the layout and not a
// QObject child of the window, so QObject::findChild never sees one. Sidebar
// rows must be looked up down the visual tree instead.
[[nodiscard]] QQuickItem *findVisualChild(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  const QList<QQuickItem *> children = root->childItems();
  for (QQuickItem *child : children) {
    if (QQuickItem *found = findVisualChild(child, objectName); found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

// The production shell (AppShell coordinator -> Main.qml) over injected
// fakes and a temporary state directory, so the Network hub and the
// Connect-to-server dialog are exercised through the real action route.
// init() reports failures as values because a helper must not host QTest
// macros -- an early return there would silently pass.
struct NetworkHubFixture final {
  bool init(const QString &temporaryPath, QString *error) {
    const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
    stateDirectory = temporaryPath + QStringLiteral("/state");

    auto backend = std::make_unique<FakeNetworkDirectoryBackend>();
    rawBackend = backend.get();
    navigation = std::make_unique<NavigationController>(
        std::make_unique<FakeDirectoryLister>(), std::make_unique<FakeFileLauncher>(),
        std::move(backend));
    // These rows never mutate anything; the backend is present only because
    // the window requires one, and it is rooted in the temporary directory.
    mutation = std::make_unique<MutationController>(
        std::make_unique<LocalMutationBackend>(temporaryPath + QStringLiteral("/Trash")));
    clipboard = std::make_unique<ClipboardController>(*mutation,
                                                      *QGuiApplication::clipboard());
    properties = std::make_unique<EntryPropertiesController>();
    search = std::make_unique<SearchController>();
    applications = std::make_unique<ApplicationsController>(QStringList{});
    places = std::make_unique<PlacesController>(
        std::make_unique<BookmarksStore>(stateDirectory));
    networkLocations = std::make_unique<NetworkLocationsController>(
        std::make_unique<NetworkLocationsStore>(stateDirectory));
    // These rows drive the hub and the dialog; nothing is ever transferred,
    // so the queue gets no worker.
    transfers = std::make_unique<TransferQueueController>(nullptr);

    const auto catalogResult = coordinator.replaceActions(fileManagerActionCatalog());
    if (!catalogResult.ok()) {
      return fail(error, QStringLiteral("the action catalog must install: %1")
                             .arg(catalogResult.message));
    }
    bindFileManagerBrowsingActions(coordinator, *navigation);
    navigation->navigateTo(temporaryPath);

    // The QML engine takes ownership of image providers, so these are raw
    // pointers the fixture deliberately does not delete.
    engine = std::make_unique<QQmlApplicationEngine>();
    engine->addImageProvider(QStringLiteral("previews"),
                             new PreviewProvider(std::make_unique<LocalPreviewDecoder>()));
    engine->addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
    engine->setInitialProperties({
        {"navigationController", QVariant::fromValue(static_cast<QObject *>(navigation.get()))},
        {"mutationController", QVariant::fromValue(static_cast<QObject *>(mutation.get()))},
        {"clipboardController", QVariant::fromValue(static_cast<QObject *>(clipboard.get()))},
        {"propertiesController", QVariant::fromValue(static_cast<QObject *>(properties.get()))},
        {"searchController", QVariant::fromValue(static_cast<QObject *>(search.get()))},
        {"placesController", QVariant::fromValue(static_cast<QObject *>(places.get()))},
        {"applicationsController", QVariant::fromValue(static_cast<QObject *>(applications.get()))},
        {"networkLocationsController",
         QVariant::fromValue(static_cast<QObject *>(networkLocations.get()))},
        {"transferQueueController",
         QVariant::fromValue(static_cast<QObject *>(transfers.get()))},
        {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}});
    engine->load(QUrl::fromLocalFile(sourceRoot +
                                     QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
    if (engine->rootObjects().isEmpty()) {
      return fail(error, QStringLiteral("Main.qml must load"));
    }
    window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
    if (window == nullptr) {
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
    return true;
  }

  QindaQt::AppShell::ApplicationCoordinator coordinator;
  QString stateDirectory;
  std::unique_ptr<NavigationController> navigation;
  std::unique_ptr<MutationController> mutation;
  std::unique_ptr<ClipboardController> clipboard;
  std::unique_ptr<EntryPropertiesController> properties;
  std::unique_ptr<SearchController> search;
  std::unique_ptr<ApplicationsController> applications;
  std::unique_ptr<PlacesController> places;
  std::unique_ptr<NetworkLocationsController> networkLocations;
  std::unique_ptr<TransferQueueController> transfers;
  std::unique_ptr<QQmlApplicationEngine> engine;
  QQuickWindow *window = nullptr;
  FakeNetworkDirectoryBackend *rawBackend = nullptr;
};

} // namespace

// ADR-0194: the Network place is a destination, not a hint. These rows drive
// the production coordinator -> Main.qml route, prove the hub really renders
// (parent chain to the window plus a positive painted size, not merely
// `visible`), save a location through the real dialog, and prove it reached
// the on-disk inventory, the sidebar, and navigation.
class TestNetworkHubUi final : public QObject {
  Q_OBJECT

private slots:
  void theNetworkPlaceOpensAHubThatRenders();
  void connectToServerSavesOpensAndListsInThePlacesSidebar();
  void aRefusedAddressExplainsItselfAndSavesNothing();
  void aCommittedTransferRefreshesOnlyTheFolderOnScreen();
};

void TestNetworkHubUi::theNetworkPlaceOpensAHubThatRenders() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  NetworkHubFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  auto *hub = fixture.window->findChild<QQuickItem *>(QStringLiteral("networkHub"));
  QVERIFY(hub);
  // Before the Network place is chosen the folder views own the surface.
  QTRY_VERIFY(!rendersInsideWindow(hub, fixture.window));

  // The same action the Network place button and the Go menu activate.
  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("go.network")));
  QTRY_VERIFY(rendersInsideWindow(hub, fixture.window));

  auto *emptyNotice =
      fixture.window->findChild<QQuickItem *>(QStringLiteral("networkHubEmptyNotice"));
  QVERIFY(emptyNotice);
  QVERIFY(rendersInsideWindow(emptyNotice, fixture.window));
  auto *connectButton =
      fixture.window->findChild<QQuickItem *>(QStringLiteral("connectToServerButton"));
  QVERIFY(connectButton);
  QVERIFY(rendersInsideWindow(connectButton, fixture.window));

  // Browsing anywhere leaves the hub, exactly as the Applications browser
  // does. It has to be a different folder: navigating to the one already open
  // changes nothing and would make this row pass for the wrong reason.
  fixture.navigation->navigateTo(temporary.filePath(QStringLiteral("elsewhere")));
  QTRY_VERIFY(!rendersInsideWindow(hub, fixture.window));
}

void TestNetworkHubUi::connectToServerSavesOpensAndListsInThePlacesSidebar() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  NetworkHubFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("network.connect")));
  auto *dialog =
      fixture.window->findChild<QObject *>(QStringLiteral("connectToServerDialog"));
  QVERIFY(dialog);
  QTRY_VERIFY(dialog->property("visible").toBool());

  const auto setField = [&fixture](const char *objectName, const QString &text) {
    QObject *field = fixture.window->findChild<QObject *>(QLatin1String(objectName));
    return field != nullptr && field->setProperty("text", text);
  };
  QVERIFY(setField("connectHostField", QStringLiteral("qinda")));
  QVERIFY(setField("connectPathField", QStringLiteral("/mnt/storage")));
  QVERIFY(setField("connectNameField", QStringLiteral("Storage (desktop)")));
  QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();

  // Saved in memory...
  QCOMPARE(fixture.networkLocations->locationValues().size(), 1);
  QCOMPARE(fixture.networkLocations->locationValues().constFirst().url.toString(),
           QStringLiteral("sftp://qinda/mnt/storage"));
  // ...and on disk, where the next launch and an ssh probe will find it.
  const NetworkLocationsStore store(fixture.stateDirectory);
  const auto reloaded = store.load();
  QVERIFY2(reloaded.ok(), qPrintable(reloaded.diagnostic));
  QCOMPARE(reloaded.locations.size(), 1);
  QCOMPARE(reloaded.locations.constFirst().name, QStringLiteral("Storage (desktop)"));

  // Saving opens it: the address reached the injected network backend.
  QTRY_COMPARE(fixture.rawBackend->requests().size(), 1);
  QCOMPARE(fixture.rawBackend->requests().constFirst().url.toString(),
           QStringLiteral("sftp://qinda/mnt/storage"));

  // And it is a rendered row in the Places sidebar.
  QCOMPARE(fixture.networkLocations->placesLocations().size(), 1);
  auto *sidebar = fixture.window->findChild<QQuickItem *>(QStringLiteral("placesSidebar"));
  QVERIFY(sidebar);
  QQuickItem *place = nullptr;
  QTRY_VERIFY((place = findVisualChild(
                   sidebar, QStringLiteral("networkPlaceButton_0"))) != nullptr);
  QTRY_VERIFY(rendersInsideWindow(place, fixture.window));
  QCOMPARE(place->property("text").toString(), QStringLiteral("Storage (desktop)"));
}

void TestNetworkHubUi::aRefusedAddressExplainsItselfAndSavesNothing() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  NetworkHubFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  QVERIFY(fixture.coordinator.activateAction(QStringLiteral("network.connect")));
  auto *dialog =
      fixture.window->findChild<QObject *>(QStringLiteral("connectToServerDialog"));
  QVERIFY(dialog);
  QTRY_VERIFY(dialog->property("visible").toBool());

  // AGENT-GUARD: the dialog has no credential fields, so the server field is
  // the only way userinfo could be smuggled in. It must be refused, and the
  // dialog must stay open with the reason on screen.
  QObject *host = fixture.window->findChild<QObject *>(QStringLiteral("connectHostField"));
  QVERIFY(host);
  QVERIFY(host->setProperty("text", QStringLiteral("cabewse:secret@qinda")));
  QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
  QCoreApplication::processEvents();

  QVERIFY(fixture.networkLocations->locationValues().isEmpty());
  QVERIFY(!fixture.networkLocations->requestError().isEmpty());
  QVERIFY(fixture.rawBackend->requests().isEmpty());
  QTRY_VERIFY(dialog->property("visible").toBool());
  auto *banner = fixture.window->findChild<QQuickItem *>(
      QStringLiteral("connectRequestErrorBanner"));
  QVERIFY(banner);
  QTRY_VERIFY(banner->isVisible());
  // Nothing was written, so the next launch finds a clean first run.
  const NetworkLocationsStore store(fixture.stateDirectory);
  QCOMPARE(store.load().error, NetworkLocationsError::Absent);
}

void TestNetworkHubUi::aCommittedTransferRefreshesOnlyTheFolderOnScreen() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  NetworkHubFixture fixture;
  QString error;
  QVERIFY2(fixture.init(temporary.path(), &error), qPrintable(error));

  // Browse a remote folder through the injected backend, so a refresh is
  // observable as a second listing request for the same URL.
  const QUrl folder(QStringLiteral("sftp://qinda/mnt/storage"));
  fixture.navigation->navigateTo(folder.toString());
  QTRY_COMPARE(fixture.rawBackend->requests().size(), 1);
  NetworkListingResult listing;
  listing.url = folder;
  fixture.rawBackend->emitReady(fixture.rawBackend->requests().constLast().generation,
                                folder, listing);
  QCOMPARE(fixture.navigation->currentPath(), folder.toString());

  // A transfer that landed somewhere else changes nothing on screen.
  QVERIFY(QMetaObject::invokeMethod(
      fixture.transfers.get(), "transferCommitted", Qt::DirectConnection,
      Q_ARG(QUrl, QUrl(QStringLiteral("sftp://qinda/mnt/other")))));
  QCoreApplication::processEvents();
  QCOMPARE(fixture.rawBackend->requests().size(), 1);

  // A transfer that landed here re-reads the authoritative listing.
  QVERIFY(QMetaObject::invokeMethod(fixture.transfers.get(), "transferCommitted",
                                    Qt::DirectConnection, Q_ARG(QUrl, folder)));
  QTRY_COMPARE(fixture.rawBackend->requests().size(), 2);
  QCOMPARE(fixture.rawBackend->requests().constLast().url, folder);
}

QTEST_MAIN(TestNetworkHubUi)
#include "tst_network_hub_ui.moc"
