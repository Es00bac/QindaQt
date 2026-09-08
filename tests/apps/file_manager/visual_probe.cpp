// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>
using namespace QindaQt::Apps::FileManager;
// Test-only capture of production QML and real injected controllers. Fixtures
// remain private to this process; no session settings or user files are
// changed.
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  if (argc < 6)
    return 2;
  const QString sourceRoot = QString::fromLocal8Bit(argv[1]);
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      sourceRoot + "/data/themes/" + QString::fromLocal8Bit(argv[2]) + ".json");
  if (!theme.ok)
    return 3;
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
  QQmlApplicationEngine engine;
  QQmlComponent registration(&engine);
  registration.setData("import QtQuick\nimport QindaQt.Tokens 1.0\nQtObject { "
                       "property int revision: Tokens.qstRevision }",
                       QUrl("inline:tokens.qml"));
  while (registration.isLoading())
    QCoreApplication::processEvents();
  std::unique_ptr<QObject> tokenObject(registration.create());
  if (!tokenObject)
    return 6;
  auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      "QindaQt.Tokens", "Tokens");
  QString error;
  QindaQt::DesignTokens::AccessibilityInputs access;
  access.highContrast = theme.theme.variant == "high-contrast";
  if (argc > 6)
    access.reducedTransparency = true;
  if (!facade || !facade->publish(theme.theme, access, &error))
    return 6;
  QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
  QIcon::setThemeName(theme.theme.iconTheme);
  auto *provider = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", provider);
  QObject::connect(
      &navigation, &NavigationController::entriesChanged, &engine,
      [&] { provider->setGeneration(navigation.listingGeneration()); });
  navigation.navigateTo(folder);
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
