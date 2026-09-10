// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <qindaqt/app_shell/application_coordinator.h>
using namespace QindaQt::Apps::FileManager;
// Test-only capture of production QML and real injected controllers. Fixtures
// remain private to this process; no session settings or user files are
// changed. ADR-0116: appearance comes from the platform theme; the argv[2]
// theme name is retained for harness compatibility and now only selects the
// declarative color scheme (qinda-light -> Light, anything else -> Dark) plus
// reduced transparency when a seventh argument is present.
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  if (argc < 6)
    return 2;
  const QString sourceRoot = QString::fromLocal8Bit(argv[1]);
  const QString appearance = QString::fromLocal8Bit(argv[2]);
  QGuiApplication::styleHints()->setColorScheme(
      appearance.contains(QStringLiteral("light")) ? Qt::ColorScheme::Light
                                                   : Qt::ColorScheme::Dark);
  QTemporaryDir temporary;
  if (!temporary.isValid())
    return 4;
  const QString folder = temporary.filePath("Little projects");
  QDir().mkpath(folder);
  for (const auto &name : {"Sketchbook", "Music", "Weekend plans"})
    QDir().mkpath(folder + "/" + QLatin1String(name));
  for (const auto &name : {"A thought.txt", "Palette.json", "Field notes.pdf",
                           "Summer mix.flac", "Keepsakes.zip"}) {
    QFile file(folder + "/" + QLatin1String(name));
    if (!file.open(QIODevice::WriteOnly))
      return 4;
    file.write("A little room for something new.\n");
  }
  const int extraItems = qBound(0, qEnvironmentVariableIntValue("QINDAQT_FILES_PROBE_ITEMS"), 400);
  for (int i = 0; i < extraItems; ++i) {
    QFile file(folder + QStringLiteral("/Document-%1.txt").arg(i, 3, 10, QLatin1Char('0')));
    if (!file.open(QIODevice::WriteOnly)) return 4;
    file.write("A local file for the scrolling fixture.\n");
  }
  QFile::copy(sourceRoot + "/data/artwork/empty-folder.png",
              folder + "/Little duck.png");
  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<DesktopFileLauncher>());
  MutationController mutation(
      std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  PlacesController places(
      std::make_unique<BookmarksStore>(temporary.filePath("state")));
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  coordinator.setApplicationName("QindaQt Files");
  coordinator.setWindowTitle("Little projects");
  if (!coordinator.replaceActions(fileManagerActionCatalog()).ok())
    return 5;
  bindFileManagerBrowsingActions(coordinator, navigation);
  QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  QQmlApplicationEngine engine;
  auto *provider = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", provider);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  QObject::connect(
      &navigation, &NavigationController::entriesChanged, &engine,
      [&] { provider->setGeneration(navigation.listingGeneration()); });
  navigation.navigateTo(folder);
  navigation.setViewMode(qEnvironmentVariable("QINDAQT_FILES_PROBE_VIEW", "grid"));
  navigation.zoomBy(qEnvironmentVariableIntValue("QINDAQT_FILES_PROBE_ZOOM_STEPS"));
  engine.setInitialProperties(
      {{"navigationController",
        QVariant::fromValue(static_cast<QObject *>(&navigation))},
       {"mutationController",
        QVariant::fromValue(static_cast<QObject *>(&mutation))},
       {"placesController",
        QVariant::fromValue(static_cast<QObject *>(&places))},
       {"coordinator",
        QVariant::fromValue(static_cast<QObject *>(&coordinator))}});
  engine.load(
      QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return 7;
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  if (!window)
    return 8;
  window->resize(QString::fromLocal8Bit(argv[4]).toInt(),
                 QString::fromLocal8Bit(argv[5]).toInt());
  const QString output = QString::fromLocal8Bit(argv[3]);
  QTimer::singleShot(1400, &app, [&] {
    const auto image = window->grabWindow();
    app.exit(!image.isNull() && image.save(output) ? 0 : 9);
  });
  return app.exec();
}
