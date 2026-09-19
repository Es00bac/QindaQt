// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_engine.h"
#include "monitor_facade.h"
#include "process_table_model.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

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

  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Monitor", &engine);
  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Facade", &facade);
  qmlRegisterSingletonInstance("QindaQt.SystemMonitor", 1, 0, "Processes",
                               &processes);

  QQmlApplicationEngine qml;
  qml.rootContext()->setContextProperty(QStringLiteral("initialPanel"), panel);
  qml.loadFromModule("QindaQt.SystemMonitor", "Main");
  if (qml.rootObjects().isEmpty()) {
    return 1;
  }

  auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().constFirst());
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
  return QGuiApplication::exec();
}
