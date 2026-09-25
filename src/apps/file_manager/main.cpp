// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_application_actions.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_item_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "model/applications_controller.h"
#include "model/applications_place.h"
#include "model/applications_place_order.h"
#include "model/bookmarks_store.h"
#include "model/clipboard_controller.h"
#include "model/column_listing.h"
#include "model/entry_facts.h"
#include "model/entry_properties.h"
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/preferences_controller.h"
#include "model/preferences_store.h"
#include "model/search_controller.h"
#include "network/kio_network_directory_backend.h"
#include "network/kio_fuse_remote_file_opener.h"
#include "network/kio_remote_copier.h"
#include "network/kio_remote_folder_creator.h"
#include "network/kio_remote_mover.h"
#include "network/kio_remote_renamer.h"
#include "network/avahi_service_discovery.h"
#include "network/discovery_controller.h"
#include "network/kio_transfer_worker.h"
#include "network/network_mount_manager.h"
#include "network/systemd_user_units.h"
#include "network/network_locations_controller.h"
#include "network/network_locations_store.h"
#include "network/transfer_queue_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include "runtime/file_actions_composition.h"
#include "runtime/file_manager_application.h"
#include "runtime/mutation_ui_action_probe.h"
#include "runtime/finder_integration.h"
#include "runtime/file_manager_ui_contract_probe.h"
#include "mutation/karchive_codec.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"

#include <QClipboard>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDBusConnection>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>

#include <cstdio>
#include <memory>
#include <optional>

namespace {

[[nodiscard]] QString configureAppShell(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    QindaQt::Apps::FileManager::NavigationController &navigation,
    QindaQt::Apps::FileManager::MutationController &mutation,
    QindaQt::Apps::FileManager::ClipboardController &clipboard,
    const QString &trashFilesDirectory) {
  coordinator.setApplicationName(QStringLiteral("QindaQt File Manager"));
  coordinator.setWindowTitle(
      QStringLiteral("QindaQt File Manager — %1").arg(navigation.currentPath()));
  coordinator.setInitialFocusObjectName(navigation.statusKey() == QStringLiteral("ready")
      ? QStringLiteral("entryGridView") : QStringLiteral("newFolderButton"));
  const auto catalogResult = coordinator.replaceActions(
      QindaQt::Apps::FileManager::fileManagerActionCatalog());
  if (!catalogResult.ok()) {
    return catalogResult.message;
  }
  QindaQt::Apps::FileManager::bindFileManagerBrowsingActions(coordinator, navigation);
  QindaQt::Apps::FileManager::bindFileManagerTransferActions(coordinator, navigation,
                                                             clipboard, mutation);
  QindaQt::Apps::FileManager::bindFileManagerMutationActions(coordinator, navigation,
                                                             mutation);
  QindaQt::Apps::FileManager::bindFileManagerApplicationActions(coordinator, navigation,
                                                                clipboard);
  QindaQt::Apps::FileManager::bindFileManagerItemActions(coordinator, navigation, clipboard,
                                                         mutation, trashFilesDirectory);
  QObject::connect(
      &coordinator,
      &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested,
      &coordinator, [&coordinator, &mutation](quint64 requestId, const QString &) {
        const auto result = coordinator.resolveQuit(
            requestId, !mutation.busy(),
            mutation.busy() ? QStringLiteral("A file operation is still running")
                            : QString());
        Q_UNUSED(result);
      });
  return {};
}

// The option registrations live outside main() to keep the composition root
// within the function-length budget after the F1 font bootstrap line.
void registerCommandLineOptions(QCommandLineParser &parser) {
  // AGENT-NOTE: --theme/--theme-directory are accepted and ignored. ADR-0116
  // retired per-app QST themes, but external harnesses (the global-menu
  // private-bus row lives outside this lane) still pass them; removing the
  // options would turn those launches into exit-1 CLI errors.
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("Deprecated no-op (ADR-0116): the Qt platform theme styles the app"),
                    QStringLiteral("id")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Deprecated no-op (ADR-0116): no per-app theme catalog is read"),
                    QStringLiteral("path")});
  parser.addOption(
      {QStringLiteral("choose-application"),
       QStringLiteral("Run as a workspace application picker (ADR-0165)")});
  parser.addOption(
      {QStringLiteral("check-qml-root"),
       QStringLiteral("Construct the QML root for an installed-package probe and exit")});
  parser.addOption(
      {QStringLiteral("check-ui-contract"),
       QStringLiteral("Verify the mutation action and accessible object contract and exit")});
  parser.addOption(
      {QStringLiteral("check-ui-actions"),
       QStringLiteral("Drive production mutation QML against a disposable fixture and exit")});
  QindaQt::Apps::FileManager::registerFinderOptions(parser);
}

// Creates and seeds the disposable --check-ui-actions fixture. The probe's S2
// stage needs one differently sized visible file (qml-batch.txt) for the
// size-sort assertions and one hidden file (.qml-hidden.txt) for the
// hidden-filter round trip. Returns nullptr on failure.
[[nodiscard]] std::unique_ptr<QTemporaryDir>
seedUiActionFixture(const QString &parentPath, QString *fixturePath) {
  auto fixture = std::make_unique<QTemporaryDir>(
      QDir(parentPath).filePath(QStringLiteral("qindaqt-file-manager-ui-XXXXXX")));
  if (!fixture->isValid()) {
    return nullptr;
  }
  *fixturePath = fixture->path();
  const struct {
    const char *name;
    QByteArray payload;
  } seeds[] = {
      {"qml-source.txt", QByteArray("fixture-data")},
      {"qml-batch.txt", QByteArray("fixture-batch-payload")},
      {".qml-hidden.txt", QByteArray("hidden-fixture")},
  };
  for (const auto &seedInfo : seeds) {
    QFile seed(QDir(*fixturePath).filePath(QString::fromLatin1(seedInfo.name)));
    if (!seed.open(QIODevice::WriteOnly) ||
        seed.write(seedInfo.payload) != seedInfo.payload.size()) {
      return nullptr;
    }
  }
  return fixture;
}

// ADR-0164: the composition root resolves the XDG data roots; the
// applications controller itself never reads the environment.
[[nodiscard]] QStringList uniqueApplicationDataRoots() {
  QStringList roots;
  for (const auto &location :
       QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
    if (!roots.contains(location)) {
      roots.append(location);
    }
  }
  return roots;
}

// The window's one mutation controller over the home Trash. ADR-0269:
// Compress and Extract run in the same busy slot through the KArchive codec.
[[nodiscard]] std::unique_ptr<QindaQt::Apps::FileManager::MutationController>
composeMutationController(const QString &trashRoot) {
  return std::make_unique<QindaQt::Apps::FileManager::MutationController>(
      std::make_unique<QindaQt::Apps::FileManager::LocalMutationBackend>(
          trashRoot, std::make_shared<QindaQt::Apps::FileManager::LocalDeviceResolver>(),
          std::make_shared<QindaQt::Apps::FileManager::KArchiveCodec>()));
}

// The one app-local state root: bookmarks, saved network locations and
// preferences are separate schemas in the same $XDG_STATE_HOME directory
// (ADR-0090/0194/0198).
[[nodiscard]] QString fileManagerStateDirectory() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation))
      .filePath(QStringLiteral("qindaqt-file-manager"));
}

// ADR-0199: the user's own systemd unit directory. Nothing is written there
// unless a saved location asks to be mounted at login.
[[nodiscard]] QString systemdUserUnitDirectory() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
      .filePath(QStringLiteral("systemd/user"));
}

// A search result set only becomes the visible listing while the window still
// shows the folder the search started in; otherwise the stale results would
// masquerade as the new location's contents.
void publishSearchResultsInto(QindaQt::Apps::FileManager::SearchController &search,
                              QindaQt::Apps::FileManager::NavigationController &navigation) {
  QObject::connect(
      &search, &QindaQt::Apps::FileManager::SearchController::searchReady,
      &navigation,
      [&search, &navigation](
          quint64, const QVector<QindaQt::Apps::FileManager::DirectoryEntry> &entries,
          const QString &statusText) {
        if (search.rootPath() != navigation.currentPath()) {
          return;
        }
        navigation.showGuestListing(entries, statusText);
      });
}

// The five owners the network and preference surfaces need, composed
// together so the composition root stays within the function-length budget
// and so their wiring -- units follow the saved locations, discovery follows
// the preference -- lives in one reviewable place.
struct NetworkComposition final {
  std::unique_ptr<QindaQt::Apps::FileManager::NetworkLocationsController> locations;
  std::unique_ptr<QindaQt::Apps::FileManager::TransferQueueController> transfers;
  std::unique_ptr<QindaQt::Apps::FileManager::PreferencesController> preferences;
  std::unique_ptr<QindaQt::Apps::FileManager::DiscoveryController> discovery;
  std::unique_ptr<QindaQt::Apps::FileManager::NetworkMountManager> mounts;
};

[[nodiscard]] NetworkComposition composeNetworkSurfaces(const QString &stateDirectory) {
  NetworkComposition composed;
  composed.locations =
      std::make_unique<QindaQt::Apps::FileManager::NetworkLocationsController>(
          std::make_unique<QindaQt::Apps::FileManager::NetworkLocationsStore>(
              stateDirectory));
  composed.transfers =
      std::make_unique<QindaQt::Apps::FileManager::TransferQueueController>(
          std::make_unique<QindaQt::Apps::FileManager::KioTransferWorker>());
  composed.preferences =
      std::make_unique<QindaQt::Apps::FileManager::PreferencesController>(
          std::make_unique<QindaQt::Apps::FileManager::PreferencesStore>(
              stateDirectory));
  // ADR-0200: Avahi lives on the system bus. Browsing starts only when the
  // preference says so, so the controller is created stopped.
  composed.discovery =
      std::make_unique<QindaQt::Apps::FileManager::DiscoveryController>(
          std::make_unique<QindaQt::Apps::FileManager::AvahiServiceDiscovery>(
              QDBusConnection::systemBus()));
  composed.mounts =
      std::make_unique<QindaQt::Apps::FileManager::NetworkMountManager>(
          systemdUserUnitDirectory(), QDir::homePath(),
          std::make_unique<QindaQt::Apps::FileManager::SystemctlUserUnits>());
  // ADR-0199: the units on disk follow the saved locations, here and on every
  // change. Nothing is written unless a location asked for a mount.
  const auto synchronizeMounts = [manager = composed.mounts.get(),
                                  locations = composed.locations.get()] {
    manager->synchronize(locations->locationValues());
  };
  QObject::connect(
      composed.locations.get(),
      &QindaQt::Apps::FileManager::NetworkLocationsController::locationsChanged,
      composed.mounts.get(), synchronizeMounts);
  synchronizeMounts();
  composed.discovery->setEnabled(composed.preferences->discoverNearbyServers());
  return composed;
}

// The listing's image providers, which the engine owns: thumbnails (ADR-0111),
// the Gallery view's one large preview (ADR-0270) and theme icons. Both
// preview caches follow the listing generation, so a new listing cancels
// every obsolete decode.
void installImageProviders(QQmlApplicationEngine &engine,
                           QindaQt::Apps::FileManager::NavigationController &navigation) {
  using QindaQt::Apps::FileManager::LocalPreviewDecoder;
  using QindaQt::Apps::FileManager::PreviewProvider;
  auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  auto *gallery = new PreviewProvider(
      std::make_unique<LocalPreviewDecoder>(LocalPreviewDecoder::galleryEdge));
  engine.addImageProvider(QStringLiteral("previews"), previews);
  engine.addImageProvider(QStringLiteral("gallery-previews"), gallery);
  engine.addImageProvider(QStringLiteral("theme-icons"),
                          new QindaQt::Apps::FileManager::ThemeIconProvider());
  QObject::connect(&navigation,
                   &QindaQt::Apps::FileManager::NavigationController::entriesChanged, &engine,
                   [previews, gallery, &navigation] {
                     previews->setGeneration(navigation.listingGeneration());
                     gallery->setGeneration(navigation.listingGeneration());
                   });
}

// Runs whichever --check-* probe mode was requested. Returns the process
// exit code when one ran, or nullopt when the window should simply be shown.
// Kept out of main() so the composition root stays within the function-length
// budget; the probes themselves are unchanged.
[[nodiscard]] std::optional<int> runProbeMode(
    const QCommandLineParser &parser, QObject *root,
    QindaQt::AppShell::ApplicationCoordinator *coordinator,
    QindaQt::Apps::FileManager::NavigationController *navigation,
    QindaQt::Apps::FileManager::MutationController *mutation,
    QindaQt::Apps::FileManager::ClipboardController *clipboard,
    const QString &startPath) {
  if (parser.isSet(QStringLiteral("check-qml-root"))) {
    std::printf("qml-root-loaded\n");
    return 0;
  }
  if (parser.isSet(QStringLiteral("check-ui-contract"))) {
    const QString missing = QindaQt::Apps::FileManager::missingUiContractObject(root);
    if (!missing.isEmpty()) {
      std::fprintf(stderr, "qindaqt-file-manager: missing UI object %s\n",
                   qPrintable(missing));
      return 5;
    }
    std::printf("mutation-ui-contract-ok\n");
    return 0;
  }
  if (parser.isSet(QStringLiteral("check-ui-actions"))) {
    QString actionError;
    if (!QindaQt::Apps::FileManager::verifyMutationUiActions(
            root, coordinator, navigation, mutation, clipboard, startPath,
            &actionError)) {
      std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(actionError));
      return 5;
    }
    std::printf("mutation-ui-actions-ok\n");
    return 0;
  }
  return std::nullopt;
}

} // namespace

// AGENT-CONTRACT: F1 font bootstrap — the single guarded composition-root
// call runs before application construction (pre-construction
// QGuiApplication::setFont persists as the application default font). A
// missing, unavailable, or unresolvable preference source leaves platform
// defaults untouched (fail-closed). See
// docs/wiki/architecture/font-preferences.md.
//
// ADR-0116: File Manager renders with stock Qt Quick Controls; its palette,
// fonts, and icon theme come from the Qt platform theme (ADR-0115). There is
// deliberately no QST token publishing or per-app theme handling here.
//
// ADR-0157: remote files open through the session KIOFuse service so
// the desktop handler edits a local write-back path; it falls back to
// the plain ADR-0152 KIO open wherever KIOFuse is unavailable.
int main(int argc, char **argv) {
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::applyFromSessionSettings();
  // AGENT-NOTE: composed through the shared factory, not constructed here
  // directly: KIO's standard widgets delegate requires a QWidget-capable
  // application (see runtime/file_manager_application.h), and focused tests
  // must run under the same application class to exercise its prompts.
  auto application = QindaQt::Apps::FileManager::createApplication(argc, argv);
  application->setApplicationName(QStringLiteral("qindaqt-file-manager"));
  application->setApplicationDisplayName(QStringLiteral("QindaQt File Manager"));
  application->setOrganizationName(QStringLiteral("QindaQt"));
  application->setDesktopFileName(QStringLiteral("org.qindaqt.FileManager"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt local file manager"));
  parser.addHelpOption();
  parser.addVersionOption();
  registerCommandLineOptions(parser);
  parser.addPositionalArgument(QStringLiteral("folder"),
                               QStringLiteral("Local folder to open"), QStringLiteral("[folder]"));
  parser.process(*application);
  if (parser.positionalArguments().size() > 1) {
    std::fprintf(stderr, "qindaqt-file-manager: open one folder at a time\n");
    return 2;
  }

  // AGENT-GUARD: Desktop launchers must receive the stable exit 4 for a bad
  // %u even when the XDG environment is intentionally sanitized by packaging,
  // so this validation runs before any QML or session-service setup.
  QString startPath = QDir::homePath();
  if (!parser.positionalArguments().isEmpty()) {
    const QFileInfo requested(parser.positionalArguments().first());
    if (requested.isDir()) {
      startPath = requested.absoluteFilePath();
    } else {
      std::fprintf(stderr, "qindaqt-file-manager: %s is not a folder\n",
                   qPrintable(parser.positionalArguments().first()));
      return 4;
    }
  }

  std::unique_ptr<QTemporaryDir> uiActionFixture;
  if (parser.isSet(QStringLiteral("check-ui-actions"))) {
    QString fixturePath;
    uiActionFixture = seedUiActionFixture(startPath, &fixturePath);
    if (!uiActionFixture) {
      std::fprintf(stderr,
                   "qindaqt-file-manager: could not create or seed the UI action fixture\n");
      return 5;
    }
    startPath = fixturePath;
  }

  QQmlApplicationEngine engine;
  // ADR-0262: the Applications place is browsed through the navigation's own
  // lister and launcher seams, so this controller is built first and outlives
  // them. Its catalog is scanned whenever the place is listed (and on F5).
  auto applicationsController =
      std::make_unique<QindaQt::Apps::FileManager::ApplicationsController>(
          uniqueApplicationDataRoots());
  applicationsController->setChooserMode(parser.isSet(QStringLiteral("choose-application")));
  auto *applications = applicationsController.get();
  auto controller = std::make_unique<QindaQt::Apps::FileManager::NavigationController>(
      std::make_unique<QindaQt::Apps::FileManager::ApplicationsDirectoryLister>(
          std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>(),
          [applications] { applications->refresh(); return applications->listing(); }),
      std::make_unique<QindaQt::Apps::FileManager::ApplicationsFileLauncher>(
          std::make_unique<QindaQt::Apps::FileManager::DesktopFileLauncher>(),
          [applications](const QString &id) { return applications->open(id); }),
      std::make_unique<QindaQt::Apps::FileManager::KioNetworkDirectoryBackend>(),
      std::make_unique<QindaQt::Apps::FileManager::KioFuseRemoteFileOpener>(),
      std::make_unique<QindaQt::Apps::FileManager::KioRemoteRenamer>(),
      std::make_unique<QindaQt::Apps::FileManager::KioRemoteFolderCreator>(),
      std::make_unique<QindaQt::Apps::FileManager::KioRemoteCopier>(),
      std::make_unique<QindaQt::Apps::FileManager::KioRemoteMover>());
  installImageProviders(engine, *controller);
  // ADR-0262: Applications keeps its own sort (A to Z) apart from folders'.
  QindaQt::Apps::FileManager::ApplicationsPlaceOrder applicationsOrder(*controller);
  // ADR-0165/ADR-0262: a workspace picker opens straight into Applications.
  controller->navigateTo(applications->chooserMode() ? applications->location() : startPath);

  const QString trashRoot = QDir(QStandardPaths::writableLocation(
                                     QStandardPaths::GenericDataLocation))
                                .filePath(QStringLiteral("Trash"));
  auto mutationController = composeMutationController(trashRoot);
  auto clipboardController =
      std::make_unique<QindaQt::Apps::FileManager::ClipboardController>(
          *mutationController, *QGuiApplication::clipboard());
  auto propertiesController =
      std::make_unique<QindaQt::Apps::FileManager::EntryPropertiesController>();
  auto searchController =
      std::make_unique<QindaQt::Apps::FileManager::SearchController>();
  // ADR-0270: lazily read Details facts, and the Columns view's other columns.
  auto entryFacts = std::make_unique<QindaQt::Apps::FileManager::EntryFacts>();
  auto columnListing = std::make_unique<QindaQt::Apps::FileManager::ColumnListing>(
      std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>());
  const QString stateDirectory = fileManagerStateDirectory();
  auto placesController =
      std::make_unique<QindaQt::Apps::FileManager::PlacesController>(
          std::make_unique<QindaQt::Apps::FileManager::BookmarksStore>(stateDirectory));
  NetworkComposition network = composeNetworkSurfaces(stateDirectory);
  auto appCoordinator = std::make_unique<QindaQt::AppShell::ApplicationCoordinator>();
  const QString appShellError = configureAppShell(
      *appCoordinator, *controller, *mutationController, *clipboardController,
      QDir(trashRoot).filePath(QStringLiteral("files")));
  if (!appShellError.isEmpty()) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n",
                 qPrintable(appShellError));
    return 3;
  }

  publishSearchResultsInto(*searchController, *controller);
  const QindaQt::Apps::FileManager::FileActionsComposition fileActions =
      QindaQt::Apps::FileManager::composeFileActions(uniqueApplicationDataRoots(), *applications);

  QVariantMap initialProperties(
      {{QStringLiteral("navigationController"),
        QVariant::fromValue(static_cast<QObject *>(controller.get()))},
       {QStringLiteral("mutationController"),
        QVariant::fromValue(static_cast<QObject *>(mutationController.get()))},
       {QStringLiteral("clipboardController"),
        QVariant::fromValue(static_cast<QObject *>(clipboardController.get()))},
       {QStringLiteral("propertiesController"),
        QVariant::fromValue(static_cast<QObject *>(propertiesController.get()))},
       {QStringLiteral("searchController"),
        QVariant::fromValue(static_cast<QObject *>(searchController.get()))},
       {QStringLiteral("placesController"),
        QVariant::fromValue(static_cast<QObject *>(placesController.get()))},
       {QStringLiteral("applicationsController"),
        QVariant::fromValue(static_cast<QObject *>(applicationsController.get()))},
       {QStringLiteral("networkLocationsController"),
        QVariant::fromValue(static_cast<QObject *>(network.locations.get()))},
       {QStringLiteral("transferQueueController"),
        QVariant::fromValue(static_cast<QObject *>(network.transfers.get()))},
       {QStringLiteral("preferencesController"),
        QVariant::fromValue(static_cast<QObject *>(network.preferences.get()))},
       {QStringLiteral("discoveryController"),
        QVariant::fromValue(static_cast<QObject *>(network.discovery.get()))},
       {QStringLiteral("mountManager"),
        QVariant::fromValue(static_cast<QObject *>(network.mounts.get()))},
       {QStringLiteral("chooserMode"), parser.isSet(QStringLiteral("choose-application"))},
       {QStringLiteral("visible"), !parser.isSet(QStringLiteral("service"))},
       {QStringLiteral("coordinator"),
        QVariant::fromValue(static_cast<QObject *>(appCoordinator.get()))}});
  fileActions.insertInto(initialProperties);
  initialProperties.insert(QStringLiteral("entryFacts"),
                           QVariant::fromValue(static_cast<QObject *>(entryFacts.get())));
  initialProperties.insert(QStringLiteral("columnListing"),
                           QVariant::fromValue(static_cast<QObject *>(columnListing.get())));
  engine.setInitialProperties(initialProperties);
  engine.loadFromModule(QStringLiteral("QindaQt.FileManagerApp"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }
  [[maybe_unused]] auto menuExport = QindaQt::Apps::FileManager::composeFileManagerMenuExport(*appCoordinator, engine.rootObjects().constFirst());
  // Keep the injected C++ controllers alive while QML tears down. The engine
  // was constructed before them, so relying on automatic stack destruction
  // would invalidate required bindings during package probes.
  const auto destroyRoots = [&engine]() {
    const QList<QObject *> roots = engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
  };
  const std::optional<int> probeExit = runProbeMode(
      parser, engine.rootObjects().constFirst(), appCoordinator.get(),
      controller.get(), mutationController.get(), clipboardController.get(),
      startPath);
  if (probeExit.has_value()) {
    destroyRoots();
    return *probeExit;
  }
  [[maybe_unused]] const auto finder = QindaQt::Apps::FileManager::composeFinderIntegration(
      parser, engine.rootObjects().constFirst(), *controller, *appCoordinator, startPath,
      applications->chooserMode());
  const int exitCode = application->exec();
  destroyRoots();
  return exitCode;
}
