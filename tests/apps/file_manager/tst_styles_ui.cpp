// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0271: File manager styles and tabs in the production window -- the
// three styles' chrome, tabs with their own folder and history, Commander's
// two panes (Tab, the function keys, Copy To at the other pane) and the
// Explorer folder tree.
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
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
#include "model/search_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/theme_icon_provider.h"
#include "runtime/folder_navigations.h"
#include "window_fixtures.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QJSValue>
#include <QPointer>
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

[[nodiscard]] std::unique_ptr<NavigationController> localNavigation() {
  return std::make_unique<NavigationController>(std::make_unique<LocalDirectoryLister>(),
                                                std::make_unique<Test::FakeFileLauncher>());
}

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

bool inside(QQuickItem *item, QQuickItem *ancestor) {
  for (; item != nullptr; item = item->parentItem()) {
    if (item == ancestor) {
      return true;
    }
  }
  return false;
}

// The production window with the composition root's FolderNavigations: every
// tab after the first gets a controller of its own, and the window's action
// states follow the one in front.
struct StylesWindow final {
  explicit StylesWindow(const QString &root)
      : navigation(std::make_unique<LocalDirectoryLister>(),
                   std::make_unique<Test::FakeFileLauncher>()),
        mutation(std::make_unique<LocalMutationBackend>(root + QStringLiteral("/Trash"))),
        clipboard(mutation, *QGuiApplication::clipboard()),
        places(std::make_unique<BookmarksStore>(root + QStringLiteral("/state"))),
        support(root), applications(QStringList{}),
        columnListing(std::make_unique<LocalDirectoryLister>()) {
    catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
    navigation.navigateTo(root + QStringLiteral("/files"));
    navigations = std::make_unique<FolderNavigations>(
        navigation, [] { return localNavigation(); },
        [this](NavigationController &active, QObject &context) {
          bindFileManagerBrowsingActions(coordinator, active, &context);
          bindFileManagerTransferActions(coordinator, active, clipboard, mutation, &context);
          bindFileManagerMutationActions(coordinator, active, mutation, &context);
        });
    engine.addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
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
                        {"navigationFactory", object(navigations.get())},
                        {"coordinator", object(&coordinator)}};
    support.insertInto(initial);
    engine.setInitialProperties(initial);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
                                    + QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
    window = engine.rootObjects().isEmpty()
        ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  }

  ~StylesWindow() {
    const auto roots = engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
  }

  [[nodiscard]] QQuickItem *item(const char *name) const {
    return window->findChild<QQuickItem *>(QString::fromLatin1(name));
  }
  // Items made by a Repeater or a Loader are found through the visual tree.
  [[nodiscard]] QQuickItem *visual(const char *name) const {
    return findVisual(window->contentItem(), QString::fromLatin1(name));
  }
  [[nodiscard]] QObject *object(const char *name) const {
    return window->findChild<QObject *>(QString::fromLatin1(name));
  }
  [[nodiscard]] QObject *panesProperty(const char *name) const {
    return object("folderPanes")->property(name).value<QObject *>();
  }
  [[nodiscard]] NavigationController *active() const {
    return qobject_cast<NavigationController *>(panesProperty("activeNavigation"));
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
  std::unique_ptr<FolderNavigations> navigations;
  QQmlApplicationEngine engine;
  QQuickWindow *window = nullptr;
};

// A QML `var` property holding a JavaScript array, as a QVariantList.
QVariantList listOf(QObject *object, const char *name) {
  QVariant value = object->property(name);
  if (value.canConvert<QJSValue>()) {
    value = value.value<QJSValue>().toVariant();
  }
  return value.toList();
}

int rowOf(QObject *tree, const QString &path) {
  const QVariantList rows = listOf(tree, "rows");
  for (int i = 0; i < rows.size(); ++i) {
    if (rows.at(i).toMap().value(QStringLiteral("path")).toString() == path) {
      return i;
    }
  }
  return -1;
}

bool visible(QQuickItem *item) { return item != nullptr && item->isVisible(); }

} // namespace

class StylesUiTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void eachStyleShowsItsChromeAndStartingView();
  void tabsKeepTheirOwnFolderAndHistory();
  void commanderSwitchesPanesAndCopiesToTheOther();
  void theFolderTreeOpensFolders();

private:
  std::unique_ptr<QTemporaryDir> m_temporary;
  std::unique_ptr<StylesWindow> m_window;
  QString m_files;
  QString m_elsewhere;
  bool open();
};

void StylesUiTests::init() {
  m_window.reset();
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  m_files = m_temporary->filePath(QStringLiteral("files"));
  m_elsewhere = m_temporary->filePath(QStringLiteral("elsewhere"));
  QVERIFY(QDir().mkpath(m_files + QStringLiteral("/inner")));
  QVERIFY(QDir().mkpath(m_elsewhere));
  for (const char *name : {"alpha.txt", "beta.log"}) {
    QVERIFY(touch(m_files + QLatin1Char('/') + QString::fromLatin1(name)));
  }
  QVERIFY(touch(m_elsewhere + QStringLiteral("/other.txt")));
}

bool StylesUiTests::open() {
  m_window = std::make_unique<StylesWindow>(m_temporary->path());
  if (!m_window->catalogInstalled || m_window->window == nullptr) {
    return false;
  }
  m_window->window->resize(QSize(1280, 800));
  m_window->window->requestActivate();
  return QTest::qWaitForWindowExposed(m_window->window);
}

void StylesUiTests::eachStyleShowsItsChromeAndStartingView() {
  QVERIFY(open());
  auto &w = *m_window;
  // Finder, today's window: one pane, no tree, no extra bars.
  QCOMPARE(w.preferences().fileManagerStyle(), QStringLiteral("finder"));
  QVERIFY(!visible(w.item("commandBar")));
  QVERIFY(!visible(w.item("functionKeyBar")));
  QVERIFY(!visible(w.item("rightPane")));
  QVERIFY(w.visual("folderTree") == nullptr);
  QCOMPARE(w.navigation.viewMode(), QStringLiteral("grid"));

  // Explorer: the tree, the address bar and the command bar, in Details.
  w.preferences().setFileManagerStyle(QStringLiteral("explorer"));
  QTRY_VERIFY(visible(w.item("commandBar")));
  QTRY_VERIFY(visible(w.visual("folderTree")));
  QVERIFY(visible(w.item("breadcrumbAddressSpace")));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("list"));
  QVERIFY(!visible(w.item("rightPane")));

  // Commander: two panes and the function keys; the Explorer chrome goes.
  w.preferences().setFileManagerStyle(QStringLiteral("commander"));
  QTRY_VERIFY(visible(w.item("rightPane")));
  QVERIFY(visible(w.item("functionKeyBar")));
  QVERIFY(visible(w.visual("functionKey_F5")));
  QVERIFY(!visible(w.item("commandBar")));
  QTRY_VERIFY(w.visual("folderTree") == nullptr);
  QCOMPARE(listOf(w.item("rightPane"), "tabs").size(), 1);

  // Back to Finder: one pane, Icons.
  w.preferences().setFileManagerStyle(QStringLiteral("finder"));
  QTRY_VERIFY(!visible(w.item("rightPane")));
  QVERIFY(!visible(w.item("functionKeyBar")));
  QTRY_COMPARE(w.navigation.viewMode(), QStringLiteral("grid"));
  QCOMPARE(w.active(), &w.navigation);
}

void StylesUiTests::tabsKeepTheirOwnFolderAndHistory() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 3);
  QQuickItem *leftPane = w.item("leftPane");
  QVERIFY(leftPane);
  QVERIFY(!visible(w.item("folderTabBar")));

  // Ctrl+T: a second tab at the same folder, in front, with a history of its own.
  QTest::keyClick(w.window, Qt::Key_T, Qt::ControlModifier);
  QTRY_COMPARE(listOf(leftPane, "tabs").size(), 2);
  QTRY_VERIFY(visible(w.item("folderTabBar")));
  QPointer<NavigationController> second = w.active();
  QVERIFY(second);
  QVERIFY(second != &w.navigation);
  QCOMPARE(second->currentPath(), m_files);
  QVERIFY(!second->canGoBack());
  second->navigateTo(m_elsewhere);
  QVERIFY(w.coordinator.windowTitle().endsWith(m_elsewhere));
  QCOMPARE(w.navigation.currentPath(), m_files);

  // Ctrl+Tab and Ctrl+Shift+Tab move between the tabs; the window's actions
  // follow the tab in front.
  QTest::keyClick(w.window, Qt::Key_Tab, Qt::ControlModifier);
  QTRY_COMPARE(w.active(), &w.navigation);
  QVERIFY(w.coordinator.windowTitle().endsWith(m_files));
  QVERIFY(!w.navigation.canGoBack());
  QTest::keyClick(w.window, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
  QTRY_COMPARE(w.active(), second.data());
  QVERIFY(second->canGoBack());

  // Ctrl+W closes the tab in front; its controller goes after its views.
  QTest::keyClick(w.window, Qt::Key_W, Qt::ControlModifier);
  QTRY_COMPARE(listOf(leftPane, "tabs").size(), 1);
  QCOMPARE(w.active(), &w.navigation);
  QTRY_VERIFY(second.isNull());
  QVERIFY(!visible(w.item("folderTabBar")));
  QVERIFY(w.coordinator.windowTitle().endsWith(m_files));
}

void StylesUiTests::commanderSwitchesPanesAndCopiesToTheOther() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 3);
  w.preferences().setFileManagerStyle(QStringLiteral("commander"));
  QQuickItem *rightPane = w.item("rightPane");
  QTRY_VERIFY(visible(rightPane));
  auto *right = qobject_cast<NavigationController *>(
      rightPane->property("navigation").value<QObject *>());
  QVERIFY(right);
  QCOMPARE(right->currentPath(), m_files);
  right->navigateTo(m_elsewhere);
  QObject *panes = w.object("folderPanes");
  QCOMPARE(panes->property("activePaneIndex").toInt(), 0);
  QCOMPARE(panes->property("otherPanePath").toString(), m_elsewhere);

  // Tab moves between the panes, and the actions follow.
  QQuickItem *leftPane = w.item("leftPane");
  QMetaObject::invokeMethod(panes, "focusActiveView");
  QTRY_VERIFY(inside(w.window->activeFocusItem(), leftPane));
  QTest::keyClick(w.window, Qt::Key_Tab);
  QTRY_COMPARE(panes->property("activePaneIndex").toInt(), 1);
  QCOMPARE(w.active(), right);
  QVERIFY(rightPane->property("active").toBool());
  QVERIFY(w.coordinator.windowTitle().endsWith(m_elsewhere));
  QTest::keyClick(w.window, Qt::Key_Tab);
  QTRY_COMPARE(panes->property("activePaneIndex").toInt(), 0);
  QCOMPARE(w.active(), &w.navigation);
  QTRY_VERIFY(inside(w.window->activeFocusItem(), leftPane));

  // F5 copies to the other pane: Copy To opens at its folder. F5 is Refresh
  // elsewhere; in a Commander pane the function key wins.
  QObject *selection = panes->property("activeSelection").value<QObject *>();
  QVERIFY(selection);
  const int alpha = w.navigation.indexOfName(QStringLiteral("alpha.txt"));
  QVERIFY(QMetaObject::invokeMethod(selection, "selectOnly", Q_ARG(QVariant, alpha)));
  QTest::keyClick(w.window, Qt::Key_F5);
  QObject *copyDialog = w.object("destinationDialog");
  QVERIFY(copyDialog);
  QTRY_VERIFY(copyDialog->property("visible").toBool());
  QCOMPARE(w.object("destinationPathField")->property("text").toString(),
           m_elsewhere + QStringLiteral("/alpha.txt"));
  QMetaObject::invokeMethod(copyDialog, "reject");
  QTRY_VERIFY(!copyDialog->property("visible").toBool());

  // F7 is New Folder, from the key and from its button.
  QMetaObject::invokeMethod(panes, "focusActiveView");
  QTRY_VERIFY(inside(w.window->activeFocusItem(), leftPane));
  QTest::keyClick(w.window, Qt::Key_F7);
  QObject *newFolder = w.object("newFolderDialog");
  QVERIFY(newFolder);
  QTRY_VERIFY(newFolder->property("visible").toBool());
  QMetaObject::invokeMethod(newFolder, "reject");
  QTRY_VERIFY(!newFolder->property("visible").toBool());
  QQuickItem *f7 = w.visual("functionKey_F7");
  QVERIFY(visible(f7));
  QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier,
                    f7->mapToScene(QPointF(f7->width() / 2, f7->height() / 2)).toPoint());
  QTRY_VERIFY(newFolder->property("visible").toBool());
  QMetaObject::invokeMethod(newFolder, "reject");
}

void StylesUiTests::theFolderTreeOpensFolders() {
  QVERIFY(open());
  auto &w = *m_window;
  // Hidden folders above the temporary directory stay reachable in the tree.
  w.navigation.setShowHidden(true);
  w.preferences().setFileManagerStyle(QStringLiteral("explorer"));
  QTRY_VERIFY(w.visual("folderTree") != nullptr);
  QObject *tree = w.visual("folderTree");
  QMetaObject::invokeMethod(tree, "reveal");
  // The tree is open down to the browsed folder, which is its current row.
  const int files = rowOf(tree, m_files);
  QVERIFY(files >= 0);
  QCOMPARE(tree->property("currentRow").toInt(), files);
  QVERIFY(!listOf(tree, "rows").at(files).toMap().value(QStringLiteral("open")).toBool());

  // Opening a row shows its subfolders; choosing one browses it.
  // The tree lists /tmp, where other processes add and remove folders, and a
  // finished folder load rebuilds it; so the row is re-found each attempt.
  QTRY_VERIFY([&] {
    QMetaObject::invokeMethod(tree, "setOpen", Q_ARG(QVariant, rowOf(tree, m_files)),
                              Q_ARG(QVariant, true));
    return rowOf(tree, m_files + QStringLiteral("/inner")) > rowOf(tree, m_files);
  }());
  const int inner = rowOf(tree, m_files + QStringLiteral("/inner"));
  QVERIFY(rowOf(tree, m_files + QStringLiteral("/alpha.txt")) < 0);
  QMetaObject::invokeMethod(tree, "openRow", Q_ARG(QVariant, inner));
  QTRY_COMPARE(w.navigation.currentPath(), m_files + QStringLiteral("/inner"));
  QCOMPARE(tree->property("currentRow").toInt(), rowOf(tree, m_files + QStringLiteral("/inner")));

  // Keys: Left goes up to the parent row, Return browses it.
  QMetaObject::invokeMethod(tree, "forceActiveFocus");
  QTRY_VERIFY(qobject_cast<QQuickItem *>(tree)->hasActiveFocus());
  QTest::keyClick(w.window, Qt::Key_Left);
  QTRY_COMPARE(tree->property("currentRow").toInt(), rowOf(tree, m_files));
  QTest::keyClick(w.window, Qt::Key_Return);
  QTRY_COMPARE(w.navigation.currentPath(), m_files);
}

QTEST_MAIN(StylesUiTests)
#include "tst_styles_ui.moc"
