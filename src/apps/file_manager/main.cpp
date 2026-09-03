// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "runtime/qml_component_ready.h"
#include "runtime/mutation_ui_action_probe.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
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
  if (!facade->publish(theme, {}, error)) {
    return nullptr;
  }
  return facade;
}

[[nodiscard]] QString configureAppShell(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    QindaQt::Apps::FileManager::NavigationController &navigation,
    QindaQt::Apps::FileManager::MutationController &mutation) {
  coordinator.setApplicationName(QStringLiteral("QindaQt File Manager"));
  coordinator.setWindowTitle(
      QStringLiteral("QindaQt File Manager — %1").arg(navigation.currentPath()));
  coordinator.setInitialFocusObjectName(QStringLiteral("newFolderButton"));
  const auto catalogResult = coordinator.replaceActions(
      QindaQt::Apps::FileManager::fileManagerActionCatalog());
  if (!catalogResult.ok()) {
    return catalogResult.message;
  }
  QObject::connect(&navigation,
                   &QindaQt::Apps::FileManager::NavigationController::navigationChanged,
                   &coordinator, [&coordinator, &navigation]() {
    coordinator.setWindowTitle(
        QStringLiteral("QindaQt File Manager — %1").arg(navigation.currentPath()));
  });
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
    uiActionFixture = std::make_unique<QTemporaryDir>(
        QDir(startPath).filePath(QStringLiteral("qindaqt-file-manager-ui-XXXXXX")));
    if (!uiActionFixture->isValid()) {
      std::fprintf(stderr,
                   "qindaqt-file-manager: could not create the UI action fixture\n");
      return 5;
    }
    startPath = uiActionFixture->path();
    QFile seed(QDir(startPath).filePath(QStringLiteral("qml-source.txt")));
    if (!seed.open(QIODevice::WriteOnly) ||
        seed.write("fixture-data") != QByteArray("fixture-data").size()) {
      std::fprintf(stderr,
                   "qindaqt-file-manager: could not seed the UI action fixture\n");
      return 5;
    }
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
  if (!registerAndPublishTokens(engine, theme.theme, &tokenError)) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(tokenError));
    return 3;
  }

  auto controller = std::make_unique<QindaQt::Apps::FileManager::NavigationController>(
      std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>(),
      std::make_unique<QindaQt::Apps::FileManager::DesktopFileLauncher>());
  controller->navigateTo(startPath);

  const QString trashRoot = QDir(QStandardPaths::writableLocation(
                                     QStandardPaths::GenericDataLocation))
                                .filePath(QStringLiteral("Trash"));
  auto mutationController =
      std::make_unique<QindaQt::Apps::FileManager::MutationController>(
          std::make_unique<QindaQt::Apps::FileManager::LocalMutationBackend>(trashRoot));
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
       {QStringLiteral("coordinator"),
        QVariant::fromValue(static_cast<QObject *>(appCoordinator.get()))}});
  engine.loadFromModule(QStringLiteral("QindaQt.FileManagerApp"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }
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
    const QStringList requiredObjects = {
        QStringLiteral("newFolderButton"), QStringLiteral("entryListView"),
        QStringLiteral("mutationProgressCard"), QStringLiteral("mutationFailureCard"),
        QStringLiteral("mutationResultCard"), QStringLiteral("newFolderDialog"),
        QStringLiteral("renameDialog"), QStringLiteral("destinationDialog"),
        QStringLiteral("trashConfirmationDialog"),
        QStringLiteral("emptyTrashConfirmationDialog")};
    QObject *root = engine.rootObjects().constFirst();
    for (const QString &objectName : requiredObjects) {
      if (!root->findChild<QObject *>(objectName)) {
        std::fprintf(stderr, "qindaqt-file-manager: missing UI object %s\n",
                     qPrintable(objectName));
        destroyRoots();
        return 5;
      }
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
