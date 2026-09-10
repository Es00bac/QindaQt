// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "model/bookmarks_store.h"
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include "runtime/mutation_ui_action_probe.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>

#include <cstdio>
#include <memory>

namespace {

[[nodiscard]] QString configureAppShell(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    QindaQt::Apps::FileManager::NavigationController &navigation,
    QindaQt::Apps::FileManager::MutationController &mutation) {
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
  QObject::connect(
      &mutation, &QindaQt::Apps::FileManager::MutationController::stateChanged,
      &coordinator, [&coordinator, &mutation]() {
        const bool idle = !mutation.busy();
        for (const QString &actionId :
             {QStringLiteral("file.new-folder"), QStringLiteral("file.rename"),
              QStringLiteral("file.copy"), QStringLiteral("file.move"),
              QStringLiteral("file.trash"), QStringLiteral("file.empty-trash")}) {
          const auto result = coordinator.setActionEnabled(actionId, idle);
          Q_UNUSED(result);
        }
        const auto undoResult = coordinator.setActionEnabled(
            QStringLiteral("edit.undo"), mutation.canUndo());
        const auto restoreResult = coordinator.setActionEnabled(
            QStringLiteral("file.restore-last"), mutation.canRestore());
        const auto cancelResult = coordinator.setActionEnabled(
            QStringLiteral("operation.cancel"), !idle);
        Q_UNUSED(undoResult);
        Q_UNUSED(restoreResult);
        Q_UNUSED(cancelResult);
      });
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
  const auto undoDisabled =
      coordinator.setActionEnabled(QStringLiteral("edit.undo"), false);
  const auto restoreDisabled =
      coordinator.setActionEnabled(QStringLiteral("file.restore-last"), false);
  const auto cancelDisabled =
      coordinator.setActionEnabled(QStringLiteral("operation.cancel"), false);
  Q_UNUSED(undoDisabled);
  Q_UNUSED(restoreDisabled);
  Q_UNUSED(cancelDisabled);
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
      {QStringLiteral("check-qml-root"),
       QStringLiteral("Construct the QML root for an installed-package probe and exit")});
  parser.addOption(
      {QStringLiteral("check-ui-contract"),
       QStringLiteral("Verify the mutation action and accessible object contract and exit")});
  parser.addOption(
      {QStringLiteral("check-ui-actions"),
       QStringLiteral("Drive production mutation QML against a disposable fixture and exit")});
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

// Returns the first missing required UI object name, or an empty string when
// the complete --check-ui-contract surface is present.
[[nodiscard]] QString missingUiContractObject(QObject *root) {
  const QStringList requiredObjects = {
      QStringLiteral("newFolderButton"), QStringLiteral("entryListView"),
      QStringLiteral("entryGridView"), QStringLiteral("locationField"),
      QStringLiteral("locationToggleButton"), QStringLiteral("toggleHiddenButton"),
      QStringLiteral("toggleViewModeButton"), QStringLiteral("placesSidebar"),
      QStringLiteral("addBookmarkButton"), QStringLiteral("bookmarkList"),
      QStringLiteral("bookmarkStoreBanner"), QStringLiteral("sortHeader_name"),
      QStringLiteral("sortHeader_size"), QStringLiteral("sortHeader_kind"),
      QStringLiteral("sortHeader_modified"),
      QStringLiteral("mutationProgressCard"), QStringLiteral("mutationFailureCard"),
      QStringLiteral("mutationResultCard"), QStringLiteral("newFolderDialog"),
      QStringLiteral("renameDialog"), QStringLiteral("destinationDialog"),
      QStringLiteral("trashConfirmationDialog"),
      QStringLiteral("emptyTrashConfirmationDialog")};
  for (const QString &objectName : requiredObjects) {
    if (!root->findChild<QObject *>(objectName)) {
      return objectName;
    }
  }
  return {};
}

} // namespace

// AGENT-CONTRACT: F1 font bootstrap — the single guarded composition-root
// call runs before QGuiApplication construction (pre-construction
// QGuiApplication::setFont persists as the application default font). A
// missing, unavailable, or unresolvable preference source leaves platform
// defaults untouched (fail-closed). See
// docs/wiki/architecture/font-preferences.md.
//
// ADR-0116: File Manager renders with stock Qt Quick Controls; its palette,
// fonts, and icon theme come from the Qt platform theme (ADR-0115). There is
// deliberately no QST token publishing or per-app theme handling here.
int main(int argc, char **argv) {
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::applyFromSessionSettings();
  QGuiApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-file-manager"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt File Manager"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.FileManager"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt local file manager"));
  parser.addHelpOption();
  parser.addVersionOption();
  registerCommandLineOptions(parser);
  parser.addPositionalArgument(QStringLiteral("folder"),
                               QStringLiteral("Local folder to open"), QStringLiteral("[folder]"));
  parser.process(application);
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
  auto controller = std::make_unique<QindaQt::Apps::FileManager::NavigationController>(
      std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>(),
      std::make_unique<QindaQt::Apps::FileManager::DesktopFileLauncher>());
  auto *previews = new QindaQt::Apps::FileManager::PreviewProvider(
      std::make_unique<QindaQt::Apps::FileManager::LocalPreviewDecoder>());
  engine.addImageProvider(QStringLiteral("previews"), previews);
  engine.addImageProvider(QStringLiteral("theme-icons"),
                          new QindaQt::Apps::FileManager::ThemeIconProvider());
  QObject::connect(controller.get(), &QindaQt::Apps::FileManager::NavigationController::entriesChanged,
                   &engine, [previews, &controller] { previews->setGeneration(controller->listingGeneration()); });
  controller->navigateTo(startPath);

  const QString trashRoot = QDir(QStandardPaths::writableLocation(
                                     QStandardPaths::GenericDataLocation))
                                .filePath(QStringLiteral("Trash"));
  auto mutationController =
      std::make_unique<QindaQt::Apps::FileManager::MutationController>(
          std::make_unique<QindaQt::Apps::FileManager::LocalMutationBackend>(trashRoot));
  auto placesController =
      std::make_unique<QindaQt::Apps::FileManager::PlacesController>(
          std::make_unique<QindaQt::Apps::FileManager::BookmarksStore>(
              QDir(QStandardPaths::writableLocation(
                       QStandardPaths::GenericStateLocation))
                  .filePath(QStringLiteral("qindaqt-file-manager"))));
  auto appCoordinator = std::make_unique<QindaQt::AppShell::ApplicationCoordinator>();
  const QString appShellError = configureAppShell(
      *appCoordinator, *controller, *mutationController);
  if (!appShellError.isEmpty()) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n",
                 qPrintable(appShellError));
    return 3;
  }

  engine.setInitialProperties(
      {{QStringLiteral("navigationController"),
        QVariant::fromValue(static_cast<QObject *>(controller.get()))},
       {QStringLiteral("mutationController"),
        QVariant::fromValue(static_cast<QObject *>(mutationController.get()))},
       {QStringLiteral("placesController"),
        QVariant::fromValue(static_cast<QObject *>(placesController.get()))},
       {QStringLiteral("coordinator"),
        QVariant::fromValue(static_cast<QObject *>(appCoordinator.get()))}});
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
  if (parser.isSet(QStringLiteral("check-qml-root"))) {
    std::printf("qml-root-loaded\n");
    destroyRoots();
    return 0;
  }
  if (parser.isSet(QStringLiteral("check-ui-contract"))) {
    const QString missing = missingUiContractObject(engine.rootObjects().constFirst());
    if (!missing.isEmpty()) {
      std::fprintf(stderr, "qindaqt-file-manager: missing UI object %s\n",
                   qPrintable(missing));
      destroyRoots();
      return 5;
    }
    std::printf("mutation-ui-contract-ok\n");
    destroyRoots();
    return 0;
  }
  if (parser.isSet(QStringLiteral("check-ui-actions"))) {
    QString actionError;
    if (!QindaQt::Apps::FileManager::verifyMutationUiActions(
            engine.rootObjects().constFirst(), appCoordinator.get(),
            controller.get(), mutationController.get(), startPath,
            &actionError)) {
      std::fprintf(stderr, "qindaqt-file-manager: %s\n",
                   qPrintable(actionError));
      destroyRoots();
      return 5;
    }
    std::printf("mutation-ui-actions-ok\n");
    destroyRoots();
    return 0;
  }
  const int exitCode = application.exec();
  destroyRoots();
  return exitCode;
}
