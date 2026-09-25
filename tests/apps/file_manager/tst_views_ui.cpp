// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/clipboard_controller.h"
#include "model/column_listing.h"
#include "model/entry_facts.h"
#include "model/entry_properties.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/preferences_store.h"
#include "model/search_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include "window_fixtures.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool touch(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
}

// The production window of main.cpp -- every view, the per-folder view
// settings over a temporary preferences store, EntryFacts and ColumnListing --
// over a temporary folder tree: files/ (three files and a folder "inner"
// holding one file) and elsewhere/ (one file).
struct ViewsWindow final {
  explicit ViewsWindow(const QString &root)
      : navigation(std::make_unique<LocalDirectoryLister>(),
                   std::make_unique<Test::FakeFileLauncher>()),
        mutation(std::make_unique<LocalMutationBackend>(root + QStringLiteral("/Trash"))),
        clipboard(mutation, *QGuiApplication::clipboard()),
        places(std::make_unique<BookmarksStore>(root + QStringLiteral("/state"))),
        support(root), applications(QStringList{}),
        columnListing(std::make_unique<LocalDirectoryLister>()) {
    catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
    bindFileManagerBrowsingActions(coordinator, navigation);
    bindFileManagerTransferActions(coordinator, navigation, clipboard, mutation);
    navigation.navigateTo(root + QStringLiteral("/files"));
    auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
    auto *gallery = new PreviewProvider(
        std::make_unique<LocalPreviewDecoder>(LocalPreviewDecoder::galleryEdge));
    engine.addImageProvider(QStringLiteral("previews"), previews);
    engine.addImageProvider(QStringLiteral("gallery-previews"), gallery);
    engine.addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
    QObject::connect(&navigation, &NavigationController::entriesChanged, &engine,
                     [this, previews, gallery] {
                       previews->setGeneration(navigation.listingGeneration());
                       gallery->setGeneration(navigation.listingGeneration());
                     });
    previews->setGeneration(navigation.listingGeneration());
    gallery->setGeneration(navigation.listingGeneration());
    const auto object = [](QObject *value) { return QVariant::fromValue(value); };
    QVariantMap initial{{"navigationController", object(&navigation)},
                        {"mutationController", object(&mutation)},
                        {"clipboardController", object(&clipboard)},
                        {"propertiesController", object(&properties)},
                        {"searchController", object(&search)},
                        {"placesController", object(&places)},
                        {"applicationsController", object(&applications)},
                        {"entryFacts", object(&facts)},
                        {"columnListing", object(&columnListing)},
                        {"coordinator", object(&coordinator)}};
    support.insertInto(initial);
    engine.setInitialProperties(initial);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
                                    + QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
    window = engine.rootObjects().isEmpty()
        ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  }

  ~ViewsWindow() {
    // Tear QML down while the injected controllers are still alive.
    const auto roots = engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
  }

  [[nodiscard]] QQuickItem *item(const char *name) const {
    return window->findChild<QQuickItem *>(QString::fromLatin1(name));
  }
  [[nodiscard]] QObject *object(const char *name) const {
    return window->findChild<QObject *>(QString::fromLatin1(name));
  }
  [[nodiscard]] PreferencesController &preferences() { return support.preferences; }

  NavigationController navigation;
  MutationController mutation;
  ClipboardController clipboard;
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places;
  Test::WindowSupportControllers support;
  ApplicationsController applications;
  EntryFacts facts;
  ColumnListing columnListing;
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  bool catalogInstalled = false;
  QQmlApplicationEngine engine;
  QQuickWindow *window = nullptr;
};

QQuickItem *findVisual(QQuickItem *root, const QString &name) {
  if (root->objectName() == name) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *found = findVisual(child, name)) {
      return found;
    }
  }
  return nullptr;
}

// A QML `var` property holding a JavaScript array, as a QVariantList.
QVariantList listOf(QObject *object, const char *name) {
  QVariant value = object->property(name);
  if (value.canConvert<QJSValue>()) {
    value = value.value<QJSValue>().toVariant();
  }
  return value.toList();
}

QPoint centerOf(QQuickItem *item) {
  return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}

} // namespace

// ADR-0270: the four QindaTK views in the production window -- every view
// shows the folder with the window's selection, keyboard and context menu;
// the switcher and the View menu choose them; a folder keeps the view the
// user gave it, and Use as Defaults makes it every folder's.
class ViewsUiTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void everyViewShowsTheFolderWithTheSharedSelection();
  void theSwitcherAndTheContextMenuChooseViews();
  void detailsSortsGroupsAndChoosesColumns();
  void aFolderKeepsItsViewAndUseAsDefaultsSharesIt();
  void columnsWalkFolderLevelsWithArrows();

private:
  std::unique_ptr<QTemporaryDir> m_temporary;
  std::unique_ptr<ViewsWindow> m_window;
  bool open();
};

void ViewsUiTests::init() {
  m_window.reset();
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  const QString files = m_temporary->filePath(QStringLiteral("files"));
  QVERIFY(QDir().mkpath(files + QStringLiteral("/inner")));
  QVERIFY(QDir().mkpath(m_temporary->filePath(QStringLiteral("elsewhere"))));
  for (const char *name : {"alpha.txt", "beta.log", "gamma.txt"}) {
    QVERIFY(touch(files + QLatin1Char('/') + QString::fromLatin1(name)));
  }
  QVERIFY(touch(files + QStringLiteral("/inner/deep.txt")));
  QVERIFY(touch(m_temporary->filePath(QStringLiteral("elsewhere/other.txt"))));
}

bool ViewsUiTests::open() {
  m_window = std::make_unique<ViewsWindow>(m_temporary->path());
  if (!m_window->catalogInstalled || m_window->window == nullptr) {
    return false;
  }
  m_window->window->resize(QSize(1280, 800));
  m_window->window->requestActivate();
  return QTest::qWaitForWindowExposed(m_window->window);
}

void ViewsUiTests::everyViewShowsTheFolderWithTheSharedSelection() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 4);
  auto *selection = w.object("entrySelection");
  QVERIFY(selection);
  const int beta = w.navigation.indexOfName(QStringLiteral("beta.log"));
  QVERIFY(QMetaObject::invokeMethod(selection, "selectOnly", Q_ARG(QVariant, beta)));

  const struct {
    Qt::Key key;
    const char *mode;
    const char *focus;
    const char *menu;
  } views[] = {{Qt::Key_2, "grid", "entryGridView", "gridContextMenu"},
               {Qt::Key_1, "list", "entryListView", "listContextMenu"},
               {Qt::Key_3, "columns", "entryColumnsView", "columnsContextMenu"},
               {Qt::Key_4, "gallery", "entryGalleryView", "galleryContextMenu"}};
  for (const auto &view : views) {
    QTest::keyClick(w.window, view.key, Qt::ControlModifier);
    QTRY_COMPARE(w.navigation.viewMode(), QString::fromLatin1(view.mode));
    auto *focus = w.item(view.focus);
    QVERIFY2(focus, view.focus);
    QTRY_VERIFY2(focus->hasActiveFocus(), view.focus);
    // The same entry stays current and selected in every view.
    QCOMPARE(selection->property("currentIndex").toInt(), beta);
    // The keyboard's menu key targets the selection, as in every view.
    QTest::keyClick(w.window, Qt::Key_Menu);
    auto *menu = w.object(view.menu);
    QVERIFY2(menu, view.menu);
    QTRY_VERIFY2(menu->property("visible").toBool(), view.menu);
    QCOMPARE(menu->property("selectionCount").toInt(), 1);
    QMetaObject::invokeMethod(menu, "close");
    QTRY_VERIFY(!menu->property("visible").toBool());
  }
  // Down moves the shared current entry in the Gallery as in a list.
  QTest::keyClick(w.window, Qt::Key_Down);
  QTRY_COMPARE(selection->property("currentIndex").toInt(), beta + 1);
  auto *strip = w.item("galleryStrip");
  QVERIFY(strip);
  QCOMPARE(listOf(strip, "windowEntries").size(), 4);
}

void ViewsUiTests::theSwitcherAndTheContextMenuChooseViews() {
  QVERIFY(open());
  auto &w = *m_window;
  auto *switcher = w.item("viewSwitcher");
  QVERIFY(switcher);
  QTRY_VERIFY(switcher->isVisible());
  // Segments follow Finder's order: Icons, Details, Columns, Gallery.
  QCOMPARE(switcher->property("currentIndex").toInt(), 0);
  auto *columnsSegment = findVisual(switcher, QStringLiteral("segment_2"));
  QVERIFY(columnsSegment);
  QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier, centerOf(columnsSegment));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("columns"));
  // The keyboard moves the switcher too: its binding came back after the click.
  QTest::keyClick(w.window, Qt::Key_4, Qt::ControlModifier);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("gallery"));
  QTRY_COMPARE(switcher->property("currentIndex").toInt(), 3);

  // The background menu's View ▸ lists the four views.
  QTest::keyClick(w.window, Qt::Key_2, Qt::ControlModifier);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("grid"));
  auto *menu = w.object("gridContextMenu");
  QVERIFY(menu);
  menu->setProperty("selectionCount", 0);
  QMetaObject::invokeMethod(menu, "popup");
  QTRY_VERIFY(menu->property("visible").toBool());
  for (const char *name : {"contextIconViewAction", "contextDetailsViewAction",
                           "contextColumnsViewAction", "contextGalleryViewAction",
                           "contextGroupKindAction"}) {
    QVERIFY2(menu->findChild<QObject *>(QString::fromLatin1(name)), name);
  }
  auto *columnsChoice = menu->findChild<QObject *>(QStringLiteral("contextColumnsViewAction"));
  QMetaObject::invokeMethod(columnsChoice, "triggered");
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("columns"));
  QMetaObject::invokeMethod(menu, "close");
}

void ViewsUiTests::detailsSortsGroupsAndChoosesColumns() {
  QVERIFY(open());
  auto &w = *m_window;
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.details-mode")));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("list"));
  auto *table = w.object("detailsTable");
  auto *details = w.object("detailsView");
  QVERIFY(table && details);

  // A header asks; the controller sorts (Tk.DataTable never sorts itself).
  QVERIFY(QMetaObject::invokeMethod(table, "sortRequested", Q_ARG(QString, QStringLiteral("kind")),
                                    Q_ARG(int, int(Qt::DescendingOrder))));
  QTRY_COMPARE(w.navigation.sortColumn(), QStringLiteral("kind"));
  QCOMPARE(w.navigation.sortDirection(), QStringLiteral("descending"));

  // Group By from the View menu: headings in Details, one per kind.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.group-kind")));
  QTRY_COMPARE(w.navigation.groupBy(), QStringLiteral("kind"));
  QTRY_COMPARE(details->property("headingCount").toInt(), 3);

  // View ▸ Show Columns opens the chooser; a column shown there is part of
  // this folder's view from then on.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.show-columns")));
  auto *chooser = w.object("columnChooser");
  QVERIFY(chooser);
  QTRY_VERIFY(chooser->property("visible").toBool());
  auto *columns = details->findChild<QObject *>(QStringLiteral("detailsColumnSet"));
  QVERIFY(columns);
  QVERIFY(QMetaObject::invokeMethod(columns, "showColumn", Q_ARG(QVariant, QStringLiteral("extension")),
                                    Q_ARG(QVariant, true)));
  QTRY_COMPARE(columns->property("shownKeys").toString(),
               QStringLiteral("name,size,kind,modified,extension"));
  const QString files = w.navigation.currentPath();
  QTRY_VERIFY(w.preferences().folderView(files).value(QStringLiteral("remembered")).toBool());
  const QVariantList stored =
      w.preferences().folderView(files).value(QStringLiteral("columns")).toList();
  QCOMPARE(stored.constLast().toMap().value(QStringLiteral("key")).toString(),
           QStringLiteral("extension"));
  QCOMPARE(w.preferences().folderView(files).value(QStringLiteral("groupBy")).toString(),
           QStringLiteral("kind"));
}

void ViewsUiTests::aFolderKeepsItsViewAndUseAsDefaultsSharesIt() {
  QVERIFY(open());
  auto &w = *m_window;
  const QString files = m_temporary->filePath(QStringLiteral("files"));
  const QString elsewhere = m_temporary->filePath(QStringLiteral("elsewhere"));
  QTest::keyClick(w.window, Qt::Key_4, Qt::ControlModifier);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("gallery"));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.zoom-in")));
  QTRY_COMPARE(w.navigation.iconSize(), 96);

  // A folder without a view of its own keeps the window's view (Finder's
  // browsing); given one there, it keeps that one.
  w.navigation.navigateTo(elsewhere);
  QTRY_COMPARE(w.navigation.currentPath(), elsewhere);
  QCOMPARE(w.navigation.viewMode(), QStringLiteral("gallery"));
  QVERIFY(!w.preferences().folderView(elsewhere).value(QStringLiteral("remembered")).toBool());
  QTest::keyClick(w.window, Qt::Key_1, Qt::ControlModifier);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("list"));

  // Each folder reopens in its own view.
  w.navigation.goBack();
  QTRY_COMPARE(w.navigation.currentPath(), files);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("gallery"));
  QCOMPARE(w.navigation.iconSize(), 96);
  w.navigation.goForward();
  QTRY_COMPARE(w.navigation.currentPath(), elsewhere);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("list"));

  // View ▸ Use as Defaults: this folder's view becomes the defaults, and the
  // folder follows them from now on instead of keeping a copy.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.use-as-defaults")));
  QTRY_COMPARE(w.preferences().defaultViewMode(), QStringLiteral("list"));
  QCOMPARE(w.preferences().iconSize(), 96);
  QVERIFY(!w.preferences().folderView(elsewhere).value(QStringLiteral("remembered")).toBool());
  QVERIFY(w.preferences().folderView(files).value(QStringLiteral("remembered")).toBool());

  // Changed defaults reach the open window when its folder has no view of
  // its own (as Preferences always did).
  w.preferences().setDefaultViewMode(QStringLiteral("columns"));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("columns"));

  // And the next launch reads it all back from preferences-v2.
  const PreferencesStore store(m_temporary->filePath(QStringLiteral("state")));
  const auto reloaded = store.load();
  QVERIFY2(reloaded.ok(), qPrintable(reloaded.diagnostic));
  QCOMPARE(reloaded.preferences.defaultViewMode, QStringLiteral("columns"));
  QCOMPARE(reloaded.preferences.iconSize, 96);
  QCOMPARE(reloaded.preferences.folderViewFor(files).viewMode, QStringLiteral("gallery"));
}

void ViewsUiTests::columnsWalkFolderLevelsWithArrows() {
  QVERIFY(open());
  auto &w = *m_window;
  QTest::keyClick(w.window, Qt::Key_3, Qt::ControlModifier);
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("columns"));
  auto *view = w.object("columnsView");
  QVERIFY(view);
  // The levels above the folder are there, nearest last.
  const QVariantList ancestors = listOf(view, "ancestors");
  QVERIFY(!ancestors.isEmpty());
  QCOMPARE(ancestors.constLast().toMap().value(QStringLiteral("childPath")).toString(),
           w.navigation.currentPath());

  // Folders sort first: "inner" is the current entry; Right opens it.
  auto *list = w.item("entryColumnsView");
  QTRY_VERIFY(list->hasActiveFocus());
  QCOMPARE(w.navigation.indexOfName(QStringLiteral("inner")), 0);
  const QString files = w.navigation.currentPath();
  // Even a folder with a view of its own stays in Columns while walked into.
  QVERIFY(w.preferences().rememberFolderView(
      files + QStringLiteral("/inner"),
      {{QStringLiteral("viewMode"), QStringLiteral("gallery")},
       {QStringLiteral("sortColumn"), QStringLiteral("name")},
       {QStringLiteral("sortDirection"), QStringLiteral("ascending")},
       {QStringLiteral("groupBy"), QStringLiteral("none")},
       {QStringLiteral("iconSize"), 64},
       {QStringLiteral("columns"), w.preferences().detailsColumns()}}));
  QTest::keyClick(w.window, Qt::Key_Right);
  QTRY_COMPARE(w.navigation.currentPath(), files + QStringLiteral("/inner"));
  QCOMPARE(w.navigation.viewMode(), QStringLiteral("columns"));
  // Left comes back with the folder it came from selected.
  QTest::keyClick(w.window, Qt::Key_Left);
  QTRY_COMPARE(w.navigation.currentPath(), files);
  auto *selection = w.object("entrySelection");
  QTRY_COMPARE(selection->property("currentIndex").toInt(), 0);
  // Arriving any other way, the folder opens in its own view.
  w.navigation.navigateTo(files + QStringLiteral("/inner"));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("gallery"));
}

QTEST_MAIN(ViewsUiTests)
#include "tst_views_ui.moc"
