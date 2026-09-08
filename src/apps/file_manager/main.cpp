// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "model/bookmarks_store.h"
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "runtime/qml_component_ready.h"
#include "preview/preview_provider.h"
#include "runtime/mutation_ui_action_probe.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>

#include <cstdio>
#include <memory>

namespace {

[[nodiscard]] QStringList themeSearchDirectories(const QString &explicitDirectory) {
  QStringList directories;
  if (!explicitDirectory.isEmpty()) {
    directories.append(QFileInfo(explicitDirectory).absoluteFilePath());
  }
  directories.append(QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
      QStandardPaths::LocateDirectory));
  directories.append(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
  directories.removeDuplicates();
  return directories;
}

[[nodiscard]] QindaQt::Themes::LoadResult
loadTheme(const QString &themeId, const QStringList &directories) {
  static const QRegularExpression safeId(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
  if (!safeId.match(themeId).hasMatch()) {
    return {.ok = false, .theme = {}, .error = QStringLiteral("Invalid theme identifier")};
  }
  for (const QString &directory : directories) {
    const QString path = QDir(directory).filePath(themeId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return QindaQt::Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false, .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(themeId)};
}

// AGENT-CONTRACT: A QML engine must import QindaQt.Tokens 1.0 through one
// component before engine.singletonInstance() resolves the generated plugin's
// registration, and TokenFacade::publish() must complete before the real root
// QML is created so every Controls binding reads a complete generation on its
// first evaluation. See tests/controls/control_test_support.cpp for the same
// sequence and src/design_tokens/include/qindaqt/design_tokens/token_facade.h
// for the GUI-thread/publish-before-construct contract.
[[nodiscard]] QindaQt::DesignTokens::TokenFacade *
registerAndPublishTokens(QQmlApplicationEngine &engine,
                         const QindaQt::Themes::ThemeSpec &theme,
                         QString *error) {
  QQmlComponent registration(&engine);
  registration.setData(R"qml(
      import QtQuick
      import QindaQt.Tokens 1.0
      QtObject { property int revision: Tokens.qstRevision }
  )qml",
                       QUrl(QStringLiteral("inline:qindaqt-file-manager-token-registration.qml")));
  if (!QindaQt::Apps::FileManager::awaitQmlComponentReady(registration, error)) {
    return nullptr;
  }
  std::unique_ptr<QObject> registrationObject(registration.create());
  if (!registrationObject) {
    if (error) {
      *error = registration.errorString();
    }
    return nullptr;
  }
  auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      "QindaQt.Tokens", "Tokens");
  if (!facade) {
    if (error) {
      *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
    }
    return nullptr;
  }
  QIcon::setThemeName(theme.iconTheme);
  QindaQt::DesignTokens::AccessibilityInputs inputs;
  inputs.highContrast = theme.variant == QStringLiteral("high-contrast");
  if (!facade->publish(theme, inputs, error)) {
    return nullptr;
  }
  return facade;
}

class FileManagerAppearanceBinding final {
public:
  FileManagerAppearanceBinding(QQmlApplicationEngine &engine,
                               QindaQt::DesignTokens::TokenFacade &facade,
                               const QString &themeDirectory,
                               const QString &explicitTheme)
      : transport(QDBusConnection::sessionBus()),
        client(transport, {QStringLiteral("appearance.theme"),
                           QStringLiteral("appearance.colorScheme"),
                           QStringLiteral("fonts.family"), QStringLiteral("fonts.monospaceFamily"),
                           QStringLiteral("fonts.pointSize"), QStringLiteral("accessibility.textScale"),
                           QStringLiteral("accessibility.highContrast"), QStringLiteral("accessibility.reducedMotion"),
                           QStringLiteral("accessibility.reducedTransparency")}),
        controller(client,
                   QindaQt::AppAppearance::standardThemeDirectories(themeDirectory),
                   QStringLiteral("qinda-dark"), explicitTheme) {
    QObject::connect(
        &controller,
        &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged,
        &engine, [&facade, this] {
          QIcon::setThemeName(controller.theme().iconTheme);
          QString error;
          if (!controller.publishTokens(facade, &error))
            qWarning().noquote() << "QindaQt File Manager kept its theme:" << error;
        });
    QString error;
    if (!client.start(&error))
      qWarning().noquote() << "QindaQt File Manager appearance settings unavailable:" << error;
  }
private:
  QindaQt::Services::SettingsClient::QtSettingsTransport transport;
  QindaQt::Services::SettingsClient::SettingsClient client;
  QindaQt::AppAppearance::ApplicationAppearanceController controller;
};

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
  parser.addOption({QStringLiteral("theme"), QStringLiteral("QindaQt theme identifier"),
                    QStringLiteral("id"), QStringLiteral("qinda-dark")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Additional local theme directory"), QStringLiteral("path")});
  parser.addOption({QStringLiteral("check-theme"),
                    QStringLiteral("Validate the selected theme through QST-1 and exit")});
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

  // AGENT-GUARD: Validate the folder contract before theme discovery. Desktop
  // launchers must receive the stable exit 4 for a bad %u even when themes are
  // absent or the XDG environment is intentionally sanitized by packaging.
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

  const auto theme = loadTheme(
      parser.value(QStringLiteral("theme")),
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(theme.error));
    return 3;
  }
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n", qPrintable(theme.theme.id),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  QQmlApplicationEngine engine;
  // AGENT-GUARD: Resolve the private package prefix from the installed
  // executable. An absolute build-tree import here makes package probes pass
  // on a developer machine while shipped clients fail to load Tokens/Controls.
  engine.addImportPath(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
  QString tokenError;
  auto *tokenFacade = registerAndPublishTokens(engine, theme.theme, &tokenError);
  if (!tokenFacade) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(tokenError));
    return 3;
  }
  FileManagerAppearanceBinding appearanceBinding(
      engine, *tokenFacade, parser.value(QStringLiteral("theme-directory")),
      parser.isSet(QStringLiteral("theme"))
          ? parser.value(QStringLiteral("theme")) : QString());

  auto controller = std::make_unique<QindaQt::Apps::FileManager::NavigationController>(
      std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>(),
      std::make_unique<QindaQt::Apps::FileManager::DesktopFileLauncher>());
  auto *previews = new QindaQt::Apps::FileManager::PreviewProvider(
      std::make_unique<QindaQt::Apps::FileManager::LocalPreviewDecoder>());
  engine.addImageProvider(QStringLiteral("previews"), previews);
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
