// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/calendar_action_catalog.h"
#include "model/calendar_collection.h"
#include "model/calendar_controller.h"
#include "model/occurrence_list_model.h"
#include "runtime/ui_action_probe.h"
#include "settings/calendar_preferences.h"

#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QVariant>

#include <cstdio>
#include <memory>

namespace {

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
//
// ADR-0116: Calendar renders with stock Qt Quick Controls; its palette and
// fonts come from the Qt platform theme (ADR-0115). There is deliberately no
// QST token publishing or per-app theme option here.
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

  // AGENT-GUARD: Desktop launchers must receive the stable exit 4 for a bad
  // %u even when the XDG environment is intentionally sanitized, so this
  // validation runs before any QML or session-service setup.
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

  QQmlApplicationEngine engine;

  const QString dataRoot =
      QDir(QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation))
          .filePath(QStringLiteral("qindaqt/calendar"));
  if (parser.isSet(QStringLiteral("check-ui-actions"))) {
    // AGENT-CONTRACT: --check-ui-actions is documented as driving "a
    // disposable data root". ctest reuses the row's XDG_DATA_HOME across
    // runs, so the probe resets the root itself to stay idempotent.
    QDir(dataRoot).removeRecursively();
  }
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
