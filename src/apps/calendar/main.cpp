// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/calendar_action_catalog.h"
#include "model/calendar_collection.h"
#include "model/calendar_controller.h"
#include "model/occurrence_list_model.h"
#include "runtime/qml_component_ready.h"
#include "runtime/ui_action_probe.h"
#include "settings/calendar_preferences.h"

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
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QStandardPaths>
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
// first evaluation. See src/apps/file_manager/main.cpp for the original
// sequence and token_facade.h for the publish-before-construct contract.
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
                       QUrl(QStringLiteral("inline:qindaqt-calendar-token-registration.qml")));
  if (!QindaQt::Apps::Calendar::awaitQmlComponentReady(registration, error)) {
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

class CalendarAppearanceBinding final {
public:
  CalendarAppearanceBinding(QQmlApplicationEngine &engine,
                            QindaQt::DesignTokens::TokenFacade &facade,
                            const QString &themeDirectory,
                            const QString &explicitTheme)
      : transport(QDBusConnection::sessionBus()),
        client(transport, {QStringLiteral("appearance.theme"),
                           QStringLiteral("appearance.colorScheme")}),
        controller(client,
                   QindaQt::AppAppearance::standardThemeDirectories(themeDirectory),
                   QStringLiteral("qinda-dark"), explicitTheme) {
    QObject::connect(
        &controller,
        &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged,
        &engine, [&facade, this] {
          QString error;
          if (!controller.publishTokens(facade, &error))
            qWarning().noquote() << "QindaQt Calendar kept its theme:" << error;
        });
    QString error;
    if (!client.start(&error))
      qWarning().noquote() << "QindaQt Calendar appearance settings unavailable:" << error;
  }
private:
  QindaQt::Services::SettingsClient::QtSettingsTransport transport;
  QindaQt::Services::SettingsClient::SettingsClient client;
  QindaQt::AppAppearance::ApplicationAppearanceController controller;
};

[[nodiscard]] QString configureAppShell(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    QindaQt::Apps::Calendar::CalendarController &controller) {
  coordinator.setApplicationName(QStringLiteral("QindaQt Calendar"));
  coordinator.setWindowTitle(
      QStringLiteral("QindaQt Calendar — %1").arg(controller.periodTitle()));
  coordinator.setInitialFocusObjectName(QStringLiteral("newEventButton"));
  const auto catalogResult = coordinator.replaceActions(
      QindaQt::Apps::Calendar::calendarActionCatalog());
  if (!catalogResult.ok()) {
    return catalogResult.message;
  }
  const auto syncViewChecks = [&coordinator, &controller]() {
    for (const auto &[id, mode] :
         {std::pair{QStringLiteral("view.month"), QStringLiteral("month")},
          {QStringLiteral("view.week"), QStringLiteral("week")},
          {QStringLiteral("view.day"), QStringLiteral("day")}}) {
      const auto result =
          coordinator.setActionChecked(id, controller.viewMode() == mode);
      Q_UNUSED(result);
    }
  };
  syncViewChecks();
  QObject::connect(&controller,
                   &QindaQt::Apps::Calendar::CalendarController::viewModeChanged,
                   &coordinator, syncViewChecks);
  QObject::connect(
      &controller,
      &QindaQt::Apps::Calendar::CalendarController::currentDateChanged,
      &coordinator, [&coordinator, &controller]() {
        coordinator.setWindowTitle(QStringLiteral("QindaQt Calendar — %1")
                                     .arg(controller.periodTitle()));
      });
  // Calendar holds no dirty document state — every mutation is persisted
  // atomically at the moment it is made — so quitting is always approved.
  QObject::connect(
      &coordinator,
      &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested,
      &coordinator, [&coordinator](quint64 requestId, const QString &) {
        const auto result = coordinator.resolveQuit(requestId, true, QString());
        Q_UNUSED(result);
      });
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
       QStringLiteral("Verify the action and accessible object contract and exit")});
  parser.addOption(
      {QStringLiteral("check-ui-actions"),
       QStringLiteral("Drive production calendar QML against a disposable data root and exit"),
       QStringLiteral("fixtures-dir")});
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
  application.setApplicationName(QStringLiteral("qindaqt-calendar"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Calendar"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.Calendar"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt local calendar"));
  parser.addHelpOption();
  parser.addVersionOption();
  registerCommandLineOptions(parser);
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("iCalendar file to import"),
                               QStringLiteral("[file]"));
  parser.process(application);
  if (parser.positionalArguments().size() > 1) {
    std::fprintf(stderr, "qindaqt-calendar: open one calendar file at a time\n");
    return 2;
  }

  // AGENT-GUARD: Validate the file contract before theme discovery. Desktop
  // launchers must receive the stable exit 4 for a bad %u even when themes
  // are absent or the XDG environment is intentionally sanitized.
  QString importPath;
  if (!parser.positionalArguments().isEmpty()) {
    const QFileInfo requested(parser.positionalArguments().first());
    if (requested.isFile()) {
      importPath = requested.absoluteFilePath();
    } else {
      std::fprintf(stderr, "qindaqt-calendar: %s is not a file\n",
                   qPrintable(parser.positionalArguments().first()));
      return 4;
    }
  }

  const auto theme = loadTheme(
      parser.value(QStringLiteral("theme")),
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-calendar: %s\n", qPrintable(theme.error));
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
    std::fprintf(stderr, "qindaqt-calendar: %s\n", qPrintable(tokenError));
    return 3;
  }
  CalendarAppearanceBinding appearanceBinding(
      engine, *tokenFacade, parser.value(QStringLiteral("theme-directory")),
      parser.isSet(QStringLiteral("theme"))
          ? parser.value(QStringLiteral("theme")) : QString());

  const QString dataRoot =
      QDir(QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation))
          .filePath(QStringLiteral("qindaqt/calendar"));
  auto preferences =
      std::make_unique<QindaQt::Apps::Calendar::CalendarPreferences>();
  auto occurrenceModel =
      std::make_unique<QindaQt::Apps::Calendar::OccurrenceListModel>();
  auto controller = std::make_unique<QindaQt::Apps::Calendar::CalendarController>(
      dataRoot, preferences.get(), occurrenceModel.get());
  auto appCoordinator = std::make_unique<QindaQt::AppShell::ApplicationCoordinator>();
  const QString appShellError = configureAppShell(*appCoordinator, *controller);
  if (!appShellError.isEmpty()) {
    std::fprintf(stderr, "qindaqt-calendar: %s\n", qPrintable(appShellError));
    return 3;
  }

  engine.setInitialProperties(
      {{QStringLiteral("coordinator"),
        QVariant::fromValue(static_cast<QObject *>(appCoordinator.get()))},
       {QStringLiteral("calendarController"),
        QVariant::fromValue(static_cast<QObject *>(controller.get()))},
       {QStringLiteral("occurrenceModel"),
        QVariant::fromValue(static_cast<QObject *>(occurrenceModel.get()))}});
  engine.loadFromModule(QStringLiteral("QindaQt.CalendarApp"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }
  [[maybe_unused]] auto menuExport = QindaQt::Apps::Calendar::composeCalendarMenuExport(
      *appCoordinator, engine.rootObjects().constFirst());
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
    const QString missing = QindaQt::Apps::Calendar::missingUiContractObject(
        engine.rootObjects().constFirst());
    if (!missing.isEmpty()) {
      std::fprintf(stderr, "qindaqt-calendar: missing UI object %s\n",
                   qPrintable(missing));
      destroyRoots();
      return 5;
    }
    std::printf("calendar-ui-contract-ok\n");
    destroyRoots();
    return 0;
  }
  if (parser.isSet(QStringLiteral("check-ui-actions"))) {
    QString actionError;
    if (!QindaQt::Apps::Calendar::verifyCalendarUiActions(
            engine.rootObjects().constFirst(), appCoordinator.get(),
            controller.get(), dataRoot,
            parser.value(QStringLiteral("check-ui-actions")), &actionError)) {
      std::fprintf(stderr, "qindaqt-calendar: %s\n", qPrintable(actionError));
      destroyRoots();
      return 5;
    }
    std::printf("calendar-ui-actions-ok\n");
    destroyRoots();
    return 0;
  }
  if (!importPath.isEmpty()) {
    Q_UNUSED(controller->importIcs(importPath, QString()));
  }
  const int exitCode = application.exec();
  destroyRoots();
  return exitCode;
}
