// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_icon_provider.h"
#include "library_controller.h"
#include "qindalutris_actions.h"

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
#include <QTimer>

#include <memory>

using namespace QindaQt::QindaLutris;

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("qindalutris"));
  QGuiApplication::setApplicationDisplayName(
      QGuiApplication::translate("main", "QindaLutris"));
  QGuiApplication::setOrganizationName(QStringLiteral("QindaQt"));
  QGuiApplication::setDesktopFileName(
      QStringLiteral("org.qindaqt.QindaLutris"));
  QGuiApplication::setWindowIcon(
      QIcon::fromTheme(QStringLiteral("org.qindaqt.QindaLutris")));

  QCommandLineParser parser;
  parser.setApplicationDescription(QGuiApplication::translate(
      "main", "Every game on the machine, in one native QindaQt window."));
  parser.addHelpOption();
  parser.addVersionOption();
  // AGENT-NOTE: the verification path for a window that has no display,
  // mirroring the System Monitor's --grab (ADR-0231): render offscreen, wait
  // for the library scan to land, write a PNG, exit.
  const QCommandLineOption grabOption(
      QStringLiteral("grab"),
      QGuiApplication::translate(
          "main", "Render one frame to a PNG and exit (for tests and review)."),
      QStringLiteral("file.png"));
  parser.addOption(grabOption);
  const QCommandLineOption grabDelayOption(
      QStringLiteral("grab-after"),
      QGuiApplication::translate(
          "main", "Milliseconds of settling before --grab writes its PNG."),
      QStringLiteral("ms"), QStringLiteral("1500"));
  parser.addOption(grabDelayOption);
  const QCommandLineOption sizeOption(
      QStringLiteral("size"),
      QGuiApplication::translate("main", "Window size as WxH."),
      QStringLiteral("WxH"));
  parser.addOption(sizeOption);
  parser.process(app);

  LibraryController library;
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  coordinator.setApplicationName(
      QGuiApplication::translate("main", "QindaLutris"));
  if (const auto error = coordinator.replaceActions(qindaLutrisActions());
      !error.ok()) {
    qCritical("Could not publish the menu: %s", qUtf8Printable(error.message));
    return 1;
  }

  qmlRegisterSingletonInstance("QindaQt.QindaLutris", 1, 0, "Library", &library);

  QQmlApplicationEngine qml;
  // AGENT-GUARD: addImageProvider takes ownership; the provider must outlive
  // the engine exactly once, so it is heap-allocated and never stack-owned.
  qml.addImageProvider(QStringLiteral("gameicon"), new GameIconProvider(&library));
  qml.rootContext()->setContextProperty(QStringLiteral("coordinator"),
                                        &coordinator);
  qml.loadFromModule("QindaQt.QindaLutris", "Main");
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

  library.refresh();

  if (parser.isSet(grabOption)) {
    if (window == nullptr) {
      qCritical("--grab needs a window root");
      return 1;
    }
    const QString path = parser.value(grabOption);
    QTimer::singleShot(parser.value(grabDelayOption).toInt(), window,
                       [window, path] {
      const QImage frame = window->grabWindow();
      if (frame.isNull() || !frame.save(path)) {
        qCritical("Could not write %s", qUtf8Printable(path));
        QCoreApplication::exit(1);
        return;
      }
      fprintf(stderr, "wrote %s (%dx%d)\n", qUtf8Printable(path),
              frame.width(), frame.height());
      QCoreApplication::quit();
    });
  }
  const int status = QGuiApplication::exec();
  menuExport.reset();
  return status;
}
