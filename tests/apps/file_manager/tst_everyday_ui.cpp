// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_item_actions.h"
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
#include "model/recents_place.h"
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
#include <QUrl>
#include <QtTest>

using namespace QindaQt::Apps::FileManager;

// ADR-0272: the File Manager's everyday features in the production window --
// Quick Look from Space in every view, type-to-select in every view, and the
// Recents place over a fixture recently-used store.

namespace {

[[nodiscard]] bool touch(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
}

// main.cpp's window over temporary storage: the Recents lister over a fixture
// store, the catalog with the browsing, transfer, mutation and item binders,
// and the preview pipelines Quick Look draws from.
struct EverydayWindow final {
  EverydayWindow(const QString &root, const QString &store)
      : navigation(std::make_unique<RecentsDirectoryLister>(
                       std::make_unique<LocalDirectoryLister>(), store),
                   std::make_unique<Test::FakeFileLauncher>()),
        mutation(std::make_unique<LocalMutationBackend>(root + QStringLiteral("/Trash"))),
        clipboard(mutation, *QGuiApplication::clipboard()),
        places(std::make_unique<BookmarksStore>(root + QStringLiteral("/state"))),
        support(root), applications(QStringList{}),
        columnListing(std::make_unique<LocalDirectoryLister>()) {
    catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
    bindFileManagerBrowsingActions(coordinator, navigation);
    bindFileManagerTransferActions(coordinator, navigation, clipboard, mutation);
    bindFileManagerMutationActions(coordinator, navigation, mutation);
    bindFileManagerItemActions(coordinator, navigation, clipboard, mutation,
                               root + QStringLiteral("/Trash/files"));
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

  ~EverydayWindow() {
    // Tear QML down while the injected controllers are still alive.
    const auto roots = engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
  }

  [[nodiscard]] QObject *object(const char *name) const {
    return window->findChild<QObject *>(QString::fromLatin1(name));
  }
  [[nodiscard]] QQuickItem *item(const char *name) const {
    return window->findChild<QQuickItem *>(QString::fromLatin1(name));
  }
  [[nodiscard]] bool actionEnabled(const QString &id) const {
    for (const auto &menu : coordinator.menus()) {
      for (const auto &action : menu.toMap().value(QStringLiteral("actions")).toList()) {
        const auto map = action.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
          return map.value(QStringLiteral("enabled")).toBool();
        }
      }
    }
    return false;
  }
  [[nodiscard]] int currentIndex() const {
    return object("entrySelection")->property("currentIndex").toInt();
  }
  [[nodiscard]] QString currentName() const {
    const int index = currentIndex();
    return index >= 0 && index < navigation.entryCount() ? navigation.entryAt(index)->name
                                                          : QString();
  }
  // The entry Quick Look shows (a QML `var` holding a JavaScript object).
  [[nodiscard]] QString previewedName() const {
    QVariant value = object("quickLook")->property("current");
    if (value.canConvert<QJSValue>()) {
      value = value.value<QJSValue>().toVariant();
    }
    return value.toMap().value(QStringLiteral("name")).toString();
  }
  [[nodiscard]] bool quickLookShowing() const {
    return object("quickLook")->property("showing").toBool();
  }

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

// Icons, Details, Columns and Gallery, each with its keyboard focus item.
const struct {
  Qt::Key key;
  const char *mode;
  const char *focus;
} kViews[] = {{Qt::Key_2, "grid", "entryGridView"},
              {Qt::Key_1, "list", "entryListView"},
              {Qt::Key_3, "columns", "entryColumnsView"},
              {Qt::Key_4, "gallery", "entryGalleryView"}};

} // namespace

class EverydayUiTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void spacePreviewsTheSelectionInEveryView();
  void quickLookIsACatalogCommand();
  void typeToSelectWorksInEveryViewAndKeepsSpaces();
  void recentsIsAPlaceOfTheStoresFiles();

private:
  std::unique_ptr<QTemporaryDir> m_temporary;
  std::unique_ptr<EverydayWindow> m_window;
  QString m_store;
  bool open();
  bool showView(const char *mode, Qt::Key key, const char *focus);
};

void EverydayUiTests::init() {
  m_window.reset();
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  const QString files = m_temporary->filePath(QStringLiteral("files"));
  QVERIFY(QDir().mkpath(files + QStringLiteral("/inner")));
  for (const char *name : {"alpha.txt", "beta.log", "gamma.txt", "my music.txt",
                           "my pictures.txt"}) {
    QVERIFY(touch(files + QLatin1Char('/') + QString::fromLatin1(name)));
  }
  m_store = m_temporary->filePath(QStringLiteral("recently-used.xbel"));
}

bool EverydayUiTests::open() {
  m_window = std::make_unique<EverydayWindow>(m_temporary->path(), m_store);
  if (!m_window->catalogInstalled || m_window->window == nullptr) {
    return false;
  }
  m_window->window->resize(QSize(1280, 800));
  m_window->window->requestActivate();
  return QTest::qWaitForWindowExposed(m_window->window);
}

bool EverydayUiTests::showView(const char *mode, Qt::Key key, const char *focus) {
  auto &w = *m_window;
  QTest::keyClick(w.window, key, Qt::ControlModifier);
  if (!QTest::qWaitFor([&] { return w.navigation.viewMode() == QLatin1String(mode); })) {
    return false;
  }
  auto *item = w.item(focus);
  return item != nullptr && QTest::qWaitFor([item] { return item->hasActiveFocus(); });
}

void EverydayUiTests::spacePreviewsTheSelectionInEveryView() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 6);
  auto *selection = w.object("entrySelection");
  auto *popover = w.object("quickLookPopover");
  QVERIFY(selection && popover);
  const int beta = w.navigation.indexOfName(QStringLiteral("beta.log"));
  for (const auto &view : kViews) {
    QVERIFY2(showView(view.mode, view.key, view.focus), view.mode);
    QVERIFY(QMetaObject::invokeMethod(selection, "selectOnly", Q_ARG(QVariant, beta)));
    // Space opens Quick Look on the selection.
    QTest::keyClick(w.window, Qt::Key_Space);
    QTRY_VERIFY2(w.quickLookShowing(), view.mode);
    QCOMPARE(w.previewedName(), QStringLiteral("beta.log"));
    // The arrows step through the folder, moving the selection underneath.
    QTest::keyClick(w.window, Qt::Key_Right);
    QTRY_COMPARE(w.currentIndex(), beta + 1);
    QVERIFY(w.quickLookShowing());
    QTest::keyClick(w.window, Qt::Key_Up);
    QTRY_COMPARE(w.currentIndex(), beta);
    // Space closes it again and the view has the keyboard back.
    QTest::keyClick(w.window, Qt::Key_Space);
    QTRY_VERIFY2(!w.quickLookShowing(), view.mode);
    QTRY_VERIFY2(w.item(view.focus)->hasActiveFocus(), view.focus);
    // The selection is unchanged by opening and closing.
    QCOMPARE(w.currentName(), QStringLiteral("beta.log"));
  }
  // Escape closes it too.
  QTest::keyClick(w.window, Qt::Key_Space);
  QTRY_VERIFY(w.quickLookShowing());
  QTest::keyClick(w.window, Qt::Key_Escape);
  QTRY_VERIFY(!w.quickLookShowing());
}

void EverydayUiTests::quickLookIsACatalogCommand() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 6);
  auto *selection = w.object("entrySelection");
  // Nothing selected: File > Quick Look is disabled.
  QTRY_VERIFY(!w.actionEnabled(QStringLiteral("file.quick-look")));
  QVERIFY(QMetaObject::invokeMethod(selection, "selectOnly",
                                    Q_ARG(QVariant, w.navigation.indexOfName(
                                                        QStringLiteral("gamma.txt")))));
  QTRY_VERIFY(w.actionEnabled(QStringLiteral("file.quick-look")));
  // Ctrl+Y opens and closes it, as the menu item does.
  QTest::keyClick(w.window, Qt::Key_Y, Qt::ControlModifier);
  QTRY_VERIFY(w.quickLookShowing());
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.quick-look")));
  QTRY_VERIFY(!w.quickLookShowing());
  // Browsing elsewhere ends a preview.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.quick-look")));
  QTRY_VERIFY(w.quickLookShowing());
  w.navigation.navigateTo(m_temporary->filePath(QStringLiteral("files/inner")));
  QTRY_VERIFY(!w.quickLookShowing());
}

void EverydayUiTests::typeToSelectWorksInEveryViewAndKeepsSpaces() {
  QVERIFY(open());
  auto &w = *m_window;
  QTRY_COMPARE(w.navigation.entryCount(), 6);
  for (const auto &view : kViews) {
    QVERIFY2(showView(view.mode, view.key, view.focus), view.mode);
    // Home clears any name being typed, as every arrow key does.
    QTest::keyClick(w.window, Qt::Key_Home);
    QTest::keyClick(w.window, 'g');
    QTRY_COMPARE(w.currentName(), QStringLiteral("gamma.txt"));
    QTest::keyClick(w.window, Qt::Key_Home);
    // A space inside a typed name is part of the name, not Quick Look.
    for (const char c : {'m', 'y', ' ', 'p'}) {
      QTest::keyClick(w.window, c);
    }
    QTRY_COMPARE(w.currentName(), QStringLiteral("my pictures.txt"));
    QVERIFY2(!w.quickLookShowing(), view.mode);
    QTest::keyClick(w.window, Qt::Key_Home);
  }
}

void EverydayUiTests::recentsIsAPlaceOfTheStoresFiles() {
  const QString gamma = m_temporary->filePath(QStringLiteral("files/gamma.txt"));
  QVERIFY(QDir().mkpath(m_temporary->filePath(QStringLiteral("elsewhere"))));
  const QString other = m_temporary->filePath(QStringLiteral("elsewhere/other.txt"));
  QVERIFY(touch(other));
  QFile store(m_store);
  QVERIFY(store.open(QIODevice::WriteOnly));
  QVERIFY(store.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<xbel version=\"1.0\">\n"
                      "  <bookmark href=\"" + QUrl::fromLocalFile(gamma).toEncoded()
                      + "\" added=\"2026-09-20T10:00:00.000001Z\"/>\n"
                      "  <bookmark href=\"" + QUrl::fromLocalFile(other).toEncoded()
                      + "\" added=\"2026-09-21T10:00:00Z\"/>\n</xbel>\n") > 0);
  store.close();
  QVERIFY(open());
  auto &w = *m_window;

  // The sidebar's Recents place opens it.
  auto *place = findVisual(w.window->contentItem(), QStringLiteral("placeButton_recents"));
  QVERIFY(place);
  QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier,
                    place->mapToScene(QPointF(place->width() / 2, place->height() / 2)).toPoint());
  QTRY_COMPARE(w.navigation.currentPath(), RecentsLocation::location());
  QTRY_COMPARE(w.navigation.entryCount(), 2);
  QVERIFY(w.coordinator.windowTitle().endsWith(QStringLiteral("Recents")));
  QVERIFY(w.navigation.indexOfName(QStringLiteral("other.txt")) >= 0);

  // Recents is no folder: nothing is created, pasted or bookmarked here.
  for (const char *id : {"file.new-folder", "file.new-file", "edit.paste", "bookmark.add",
                         "file.open-terminal"}) {
    QTRY_VERIFY2(!w.actionEnabled(QString::fromLatin1(id)), id);
  }
  QVERIFY(!w.actionEnabled(QStringLiteral("file.properties")));
  // Its rows are ordinary files: selected, they can be previewed and inspected.
  auto *selection = w.object("entrySelection");
  QVERIFY(QMetaObject::invokeMethod(selection, "selectOnly",
                                    Q_ARG(QVariant, w.navigation.indexOfName(
                                                        QStringLiteral("gamma.txt")))));
  QTRY_VERIFY(w.actionEnabled(QStringLiteral("file.quick-look")));
  QVERIFY(w.actionEnabled(QStringLiteral("file.properties")));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.quick-look")));
  QTRY_VERIFY(w.quickLookShowing());
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.quick-look")));
  QTRY_VERIFY(!w.quickLookShowing());

  // Go > Recents reaches it too, from any folder.
  w.navigation.navigateTo(m_temporary->filePath(QStringLiteral("files")));
  QTRY_VERIFY(!w.navigation.recentsPlace());
  QTest::keyClick(w.window, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
  QTRY_COMPARE(w.navigation.currentPath(), RecentsLocation::location());
}

QTEST_MAIN(EverydayUiTests)
#include "tst_everyday_ui.moc"
