// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_engine.h"
#include "system_monitor_actions.h"
#include "monitor_facade.h"
#include "process_table_model.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>
#include <QDBusConnection>
#include <QPointer>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

#include <memory>

using namespace QindaQt::SystemMonitor;

namespace {

// The panels the window can show. `--panel` opens one on its own, which is
// how the detachable-view contract (ADR-0108) is honoured now that the views
// are dock panels rather than pages of a stack: a detached panel is this same
// executable started with one panel id.
const QStringList &panelIds() {
  static const QStringList ids{
      QStringLiteral("dashboard"), QStringLiteral("cpu"),
      QStringLiteral("memory"),    QStringLiteral("disks"),
      QStringLiteral("network"),   QStringLiteral("processes"),
      QStringLiteral("hardware")};
  return ids;
}

} // namespace

int main(int argc, char **argv) {
  // AGENT-NOTE: multisampling is requested before the application exists
  // because Tk.Graph draws its traces as scene-graph geometry, which the
  // renderer does not antialias on its own. Without this every plot line is
  // visibly stepped. Qt falls back silently when the driver refuses.
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSamples(4);
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("qindaqt-system-monitor"));
  QGuiApplication::setApplicationDisplayName(
      QGuiApplication::translate("main", "System Monitor"));
  QGuiApplication::setOrganizationName(QStringLiteral("QindaQt"));
  QGuiApplication::setDesktopFileName(
      QStringLiteral("org.qindaqt.SystemMonitor"));
  QGuiApplication::setWindowIcon(
      QIcon::fromTheme(QStringLiteral("org.qindaqt.SystemMonitor")));

  QCommandLineParser parser;
  parser.setApplicationDescription(QGuiApplication::translate(
      "main", "Watch processor, memory, disk, network and hardware activity."));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption panelOption(
      {QStringLiteral("panel"), QStringLiteral("view")},
      QGuiApplication::translate("main", "Open one panel on its own: %1.")
          .arg(panelIds().join(QStringLiteral(", "))),
      QStringLiteral("id"), QStringLiteral("dashboard"));
  parser.addOption(panelOption);
  // AGENT-NOTE: the verification path for a window that has no display. It
  // mirrors `qtk-preview --grab`: render offscreen, wait for readings to
  // arrive, write a PNG, exit. Without it the only way to check this UI is a
  // live session, which no test and no agent can rely on having.
  const QCommandLineOption grabOption(
      QStringLiteral("grab"),
      QGuiApplication::translate(
          "main", "Render one frame to a PNG and exit (for tests and review)."),
      QStringLiteral("file.png"));
  parser.addOption(grabOption);
  const QCommandLineOption grabDelayOption(
      QStringLiteral("grab-after"),
      QGuiApplication::translate(
          "main", "Milliseconds of sampling before --grab writes its PNG."),
      QStringLiteral("ms"), QStringLiteral("2500"));
  parser.addOption(grabDelayOption);
  const QCommandLineOption sizeOption(
      QStringLiteral("size"),
      QGuiApplication::translate("main", "Window size as WxH."),
      QStringLiteral("WxH"));
  parser.addOption(sizeOption);
  parser.process(app);

  const QString panel = parser.value(panelOption).toLower();
  if (!panelIds().contains(panel)) {
    qCritical("Unknown panel \"%s\". Known panels: %s",
              qUtf8Printable(panel),
              qUtf8Printable(panelIds().join(QStringLiteral(", "))));
    return 2;
  }

  MonitorEngine engine;
  MonitorFacade facade;
  ProcessTableModel processes;
  processes.setSource(engine.processes());

  // AGENT-CONTRACT: one action catalog drives both the desktop's global menu
  // and the in-window bar. The desktop hosts it when a registrar is there;
  // `inWindowMenuVisible` on the root window is how the export tells QML to
  // stop drawing its own, so the menu is never in two places at once.
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  coordinator.setApplicationName(
      QGuiApplication::translate("main", "QindaQt System Monitor"));
  const QList<QindaQt::AppShell::ActionSpec> actions =
      systemMonitorActions(panel != QLatin1String("dashboard"));
  if (const auto error = coordinator.replaceActions(actions); !error.ok()) {
    qCritical("Could not publish the menu: %s", qUtf8Printable(error.message));
    return 1;
  }

  // AGENT-NOTE: checked state is published from here, not from QML:
  // setActionChecked is not Q_INVOKABLE, and this is the right side of the
  // boundary anyway -- the application's state decides what the menus show,
  // and both copies of the menu then read the same snapshot.
  const auto publishCheckedState = [&coordinator, &engine, &actions] {
    static_cast<void>(
        coordinator.setActionChecked(QStringLiteral("view.pause"), engine.paused()));
    for (const QindaQt::AppShell::ActionSpec &spec : actions) {
      const int interval = intervalForActionId(spec.id);
      if (interval > 0) {
        static_cast<void>(
            coordinator.setActionChecked(spec.id, interval == engine.interval()));
      }
    }
  };
  QObject::connect(&engine, &MonitorEngine::pausedChanged, &coordinator,
                   publishCheckedState);
  QObject::connect(&engine, &MonitorEngine::intervalChanged, &coordinator,
                   publishCheckedState);
  publishCheckedState();

  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Monitor", &engine);
  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Facade", &facade);
  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Processes",
                               &processes);

  QQmlApplicationEngine qml;
  qml.rootContext()->setContextProperty(QStringLiteral("initialPanel"), panel);
  qml.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);
  qml.loadFromModule("QindaQt.SystemMonitor", "Main");
  if (qml.rootObjects().isEmpty()) {
    return 1;
  }

  auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().constFirst());
  const QPointer<QQuickWindow> windowGuard(window);
  std::unique_ptr<QObject> menuExport;
  if (window != nullptr) {
    menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
        coordinator, *window, QDBusConnection::sessionBus(),
        [windowGuard](bool visible) {
          if (windowGuard) {
            windowGuard->setProperty("inWindowMenuVisible", visible);
          }
        });
  }
  if (window != nullptr && parser.isSet(sizeOption)) {
    const QStringList parts = parser.value(sizeOption).split(QLatin1Char('x'));
    if (parts.size() == 2) {
      window->resize(parts.at(0).toInt(), parts.at(1).toInt());
    }
  }

  if (parser.isSet(grabOption)) {
    if (window == nullptr) {
      qCritical("--grab needs a window root");
      return 1;
    }
    const QString path = parser.value(grabOption);
    // Sampling is asynchronous and rates need a second reading before they
    // exist at all, so a grab taken immediately would picture an empty
    // machine. The delay is how long the monitor is allowed to fill in.
    QTimer::singleShot(parser.value(grabDelayOption).toInt(), window, [window, path] {
      const QImage frame = window->grabWindow();
      if (frame.isNull() || !frame.save(path)) {
        qCritical("Could not write %s", qUtf8Printable(path));
        QCoreApplication::exit(1);
        return;
      }
      fprintf(stderr, "wrote %s (%dx%d)\n", qUtf8Printable(path), frame.width(),
              frame.height());
      QCoreApplication::quit();
    });
  }
  const int status = QGuiApplication::exec();
  // The export holds a D-Bus endpoint bound to the window; drop it before the
  // engine tears the window down.
  menuExport.reset();
  return status;
}
