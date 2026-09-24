// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "model/applications_controller.h"
#include "model/applications_place.h"
#include "model/applications_place_order.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "window_fixtures.h"
#include "model/search_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
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
  // ADR-0262: QINDAQT_FILES_PROBE_PLACE=applications opens the Applications
  // place over QINDAQT_FILES_PROBE_APP_ROOTS (colon-separated XDG data roots,
  // default this machine's). The probe's launch seams refuse everything, so a
  // capture can never start an application.
  const bool applicationsPlace =
      qEnvironmentVariable("QINDAQT_FILES_PROBE_PLACE") == QStringLiteral("applications");
  QStringList appRoots = qEnvironmentVariable("QINDAQT_FILES_PROBE_APP_ROOTS")
                             .split(QLatin1Char(':'), Qt::SkipEmptyParts);
  if (appRoots.isEmpty())
    appRoots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  ApplicationsController applications(
      appRoots, ApplicationLaunchSeams{
                    [](const QString &) { return ChooserReply{false, QStringLiteral("probe")}; },
                    [](const QString &, const QStringList &) { return false; }});
  NavigationController navigation(
      std::make_unique<ApplicationsDirectoryLister>(
          std::make_unique<LocalDirectoryLister>(),
          [&applications] { applications.refresh(); return applications.listing(); }),
      std::make_unique<ApplicationsFileLauncher>(
          std::make_unique<DesktopFileLauncher>(),
          [&applications](const QString &id) { return applications.open(id); }));
  ApplicationsPlaceOrder applicationsOrder(navigation);
  MutationController mutation(
      std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places(
      std::make_unique<BookmarksStore>(temporary.filePath("state")));
  // The probe renders; it never transfers, discovers, or mounts.
  Test::WindowSupportControllers support(temporary.path());
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  coordinator.setApplicationName("QindaQt Files");
  coordinator.setWindowTitle("Little projects");
  if (!coordinator.replaceActions(fileManagerActionCatalog()).ok())
    return 5;
  bindFileManagerBrowsingActions(coordinator, navigation);
  // Application icons live in the system hicolor tree the QindaQt theme
  // inherits; only the Applications capture needs it on the search path.
  QIcon::setThemeSearchPaths(applicationsPlace
      ? QStringList{sourceRoot + "/data/icons", QStringLiteral("/usr/share/icons")}
      : QStringList{sourceRoot + "/data/icons"});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  QQmlApplicationEngine engine;
  auto *provider = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", provider);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  QObject::connect(
      &navigation, &NavigationController::entriesChanged, &engine,
      [&] { provider->setGeneration(navigation.listingGeneration()); });
  navigation.navigateTo(applicationsPlace ? ApplicationsController::location() : folder);
  // Idempotent: setSortColumn() flips the direction of the active column.
  const auto applyPresentation = [&navigation] {
    const QString sort = qEnvironmentVariable("QINDAQT_FILES_PROBE_SORT");
    if (!sort.isEmpty() && navigation.sortColumn() != sort)
      navigation.setSortColumn(sort);
    navigation.setViewMode(qEnvironmentVariable("QINDAQT_FILES_PROBE_VIEW", "grid"));
    navigation.resetZoom();
    navigation.zoomBy(qEnvironmentVariableIntValue("QINDAQT_FILES_PROBE_ZOOM_STEPS"));
  };
  applyPresentation();
  QVariantMap initialProperties{
      {{"navigationController",
        QVariant::fromValue(static_cast<QObject *>(&navigation))},
       {"mutationController",
        QVariant::fromValue(static_cast<QObject *>(&mutation))},
       {"clipboardController",
        QVariant::fromValue(static_cast<QObject *>(&clipboard))},
       {"propertiesController",
        QVariant::fromValue(static_cast<QObject *>(&properties))},
       {"searchController",
        QVariant::fromValue(static_cast<QObject *>(&search))},
       {"placesController",
        QVariant::fromValue(static_cast<QObject *>(&places))},
       {"applicationsController",
        QVariant::fromValue(static_cast<QObject *>(&applications))},
       {"coordinator",
        QVariant::fromValue(static_cast<QObject *>(&coordinator))}}};
  support.insertInto(initialProperties);
  engine.setInitialProperties(initialProperties);
  engine.load(
      QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return 7;
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  if (!window)
    return 8;
  window->resize(QString::fromLocal8Bit(argv[4]).toInt(),
                 QString::fromLocal8Bit(argv[5]).toInt());
  // PresentationDefaults.qml applied the fixture's default preferences when
  // the window completed; the requested view, zoom and sort win over them.
  applyPresentation();
  // Optional capture state: select one row, then run one catalog action
  // (for example file.properties for Get Info) exactly as a user would.
  if (qEnvironmentVariableIsSet("QINDAQT_FILES_PROBE_SELECT")) {
    if (auto *selection = window->findChild<QObject *>(QStringLiteral("entrySelection")))
      QMetaObject::invokeMethod(
          selection, "selectOnly",
          Q_ARG(QVariant, qEnvironmentVariableIntValue("QINDAQT_FILES_PROBE_SELECT")));
  }
  if (qEnvironmentVariableIsSet("QINDAQT_FILES_PROBE_ACTION"))
    QTimer::singleShot(600, &app, [&coordinator] {
      coordinator.activateAction(qEnvironmentVariable("QINDAQT_FILES_PROBE_ACTION"));
    });
  const QString output = QString::fromLocal8Bit(argv[3]);
  QTimer::singleShot(1400, &app, [&] {
    const auto image = window->grabWindow();
    app.exit(!image.isNull() && image.save(output) ? 0 : 9);
  });
  return app.exec();
}
