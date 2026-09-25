// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_application_actions.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_item_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "application_fixtures.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/applications_place.h"
#include "model/applications_place_order.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/search_controller.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/local_preview.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include "window_fixtures.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

// The production composition of main.cpp -- the Applications place reached
// through the navigation's lister/launcher decorators, every action binder,
// the real AppShell coordinator and Main.qml -- over a fixture catalog and a
// recording launch seam, so nothing is ever spawned or sent to a bus.
struct ApplicationsWindow final
{
    ApplicationsWindow(const QString &temporaryPath, bool chooser)
        : applications({temporaryPath + QStringLiteral("/data")}, recorder.seams())
        , mutation(std::make_unique<LocalMutationBackend>(temporaryPath + QStringLiteral("/Trash")))
        , clipboard(mutation, *QGuiApplication::clipboard())
        , places(std::make_unique<BookmarksStore>(temporaryPath + QStringLiteral("/state")))
        , support(temporaryPath)
    {
        applications.setChooserMode(chooser);
        navigation = std::make_unique<NavigationController>(
            std::make_unique<ApplicationsDirectoryLister>(
                std::make_unique<LocalDirectoryLister>(),
                [this] { applications.refresh(); return applications.listing(); }),
            std::make_unique<ApplicationsFileLauncher>(
                std::make_unique<Test::FakeFileLauncher>(),
                [this](const QString &id) { return applications.open(id); }));
        order = std::make_unique<ApplicationsPlaceOrder>(*navigation);
        catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
        bindFileManagerBrowsingActions(coordinator, *navigation);
        bindFileManagerTransferActions(coordinator, *navigation, clipboard, mutation);
        bindFileManagerMutationActions(coordinator, *navigation, mutation);
        bindFileManagerApplicationActions(coordinator, *navigation, clipboard);
        bindFileManagerItemActions(coordinator, *navigation, clipboard, mutation,
                                   temporaryPath + QStringLiteral("/Trash/files"));
        // main.cpp: a picker opens straight into Applications.
        navigation->navigateTo(chooser ? ApplicationsController::location() : temporaryPath);

        engine.addImageProvider(QStringLiteral("previews"),
                                new PreviewProvider(std::make_unique<LocalPreviewDecoder>()));
        engine.addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
        const auto object = [](QObject *value) { return QVariant::fromValue(value); };
        QVariantMap initial{{"navigationController", object(navigation.get())},
                            {"mutationController", object(&mutation)},
                            {"clipboardController", object(&clipboard)},
                            {"propertiesController", object(&properties)},
                            {"searchController", object(&search)},
                            {"placesController", object(&places)},
                            {"applicationsController", object(&applications)},
                            {"chooserMode", chooser},
                            {"coordinator", object(&coordinator)}};
        support.insertInto(initial);
        engine.setInitialProperties(initial);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
                                        + QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
        window = engine.rootObjects().isEmpty()
            ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    }

    ~ApplicationsWindow()
    {
        // Tear QML down while the injected controllers are still alive.
        const auto roots = engine.rootObjects();
        for (QObject *root : roots) {
            delete root;
        }
    }

    [[nodiscard]] QQuickItem *item(const char *name) const
    {
        return window->findChild<QQuickItem *>(QString::fromLatin1(name));
    }

    [[nodiscard]] bool actionEnabled(const QString &id) const
    {
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

    Test::RecordingLaunchSeams recorder;
    ApplicationsController applications;
    MutationController mutation;
    ClipboardController clipboard;
    EntryPropertiesController properties;
    SearchController search;
    PlacesController places;
    Test::WindowSupportControllers support;
    QindaQt::AppShell::ApplicationCoordinator coordinator;
    std::unique_ptr<NavigationController> navigation;
    std::unique_ptr<ApplicationsPlaceOrder> order;
    bool catalogInstalled = false;
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;
};

// Model-instantiated delegates are visual children of the view's content
// item only; find the one currently showing a model row.
QQuickItem *delegateFor(QQuickItem *view, int index)
{
    auto *content = view->property("contentItem").value<QQuickItem *>();
    for (QQuickItem *child : content->childItems()) {
        const QVariant row = child->property("index");
        if (row.isValid() && child->property("modelData").isValid() && row.toInt() == index
            && child->isVisible()) {
            return child;
        }
    }
    return nullptr;
}

QPoint centerOf(QQuickItem *item)
{
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}

QQuickItem *findVisual(QQuickItem *root, const QString &name)
{
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

} // namespace

// ADR-0262 view rows: Applications behaves like every other folder in the
// production window, with application actions where file actions would be.
class ApplicationsUiTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void sidebarOpensThePlaceAndClicksSelectBeforeOpening();
    void keyboardOpensAndTypeToSelectFinds();
    void filterDetailsViewAndGroupByCategory();
    void contextMenuOffersApplicationActions();
    void fileActionsStayOffInsideApplications();
    void chooserModeKeepsItsSemantics();
    void aDockedWindowIsReplacedRatherThanSpawning();

private:
    std::unique_ptr<QTemporaryDir> m_temporary;
    std::unique_ptr<ApplicationsWindow> m_window;
    bool open(bool chooser = false);
};

void ApplicationsUiTests::init()
{
    m_window.reset();
    m_temporary = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temporary->isValid());
    QVERIFY(Test::writeSampleCatalog(m_temporary->filePath(QStringLiteral("data"))));
}

bool ApplicationsUiTests::open(bool chooser)
{
    m_window = std::make_unique<ApplicationsWindow>(m_temporary->path(), chooser);
    if (!m_window->catalogInstalled || m_window->window == nullptr) {
        return false;
    }
    m_window->window->resize(QSize(1280, 800));
    m_window->window->requestActivate();
    return QTest::qWaitForWindowExposed(m_window->window);
}

void ApplicationsUiTests::sidebarOpensThePlaceAndClicksSelectBeforeOpening()
{
    QVERIFY(open());
    auto &w = *m_window;
    auto *place = findVisual(w.window->contentItem(), QStringLiteral("placeButton_applications"));
    QVERIFY(place);
    QVERIFY(!place->property("emphasized").toBool());
    QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier, centerOf(place));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    QTRY_VERIFY(place->property("emphasized").toBool());
    QCOMPARE(w.window->title(), QStringLiteral("QindaQt File Manager — Applications"));

    auto *grid = w.item("entryGridView");
    QVERIFY(grid);
    QTRY_COMPARE(grid->property("count").toInt(), 5);
    QTRY_VERIFY(delegateFor(grid, 3));
    // A single click selects and never launches.
    QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier, centerOf(delegateFor(grid, 3)));
    QTRY_COMPARE(w.clipboard.selectionCount(), 1);
    QCOMPARE(grid->property("currentIndex").toInt(), 3);
    QVERIFY(w.recorder.chosen.isEmpty());
    QVERIFY(w.recorder.spawned.isEmpty());

    // A double-click opens that application through the controller.
    QTest::mouseDClick(w.window, Qt::LeftButton, Qt::NoModifier, centerOf(delegateFor(grid, 3)));
    QTRY_COMPARE(w.recorder.chosen, QStringList{QStringLiteral("editor")});
    QCOMPARE(w.recorder.spawned.size(), 1);
    QCOMPARE(w.recorder.spawned.first().first(), QStringLiteral("/usr/bin/editor"));
    QVERIFY(w.navigation->launchError().isEmpty());
}

void ApplicationsUiTests::keyboardOpensAndTypeToSelectFinds()
{
    QVERIFY(open());
    auto &w = *m_window;
    w.coordinator.activateAction(QStringLiteral("go.applications"));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    auto *grid = w.item("entryGridView");
    grid->forceActiveFocus();
    QTRY_VERIFY(grid->hasActiveFocus());

    // Type-to-select finds "Zeta Viewer", then "Console Tool".
    QTest::keyClick(w.window, Qt::Key_Z);
    QTRY_COMPARE(grid->property("currentIndex").toInt(), 4);
    QTest::qWait(1100); // the type-ahead prefix expires after one second
    QTest::keyClick(w.window, Qt::Key_C);
    QTRY_COMPARE(grid->property("currentIndex").toInt(), 2);
    QVERIFY(w.recorder.chosen.isEmpty());

    // Enter opens the focused row; an inert terminal entry explains itself in
    // the window's launch banner and spawns nothing.
    QTest::keyClick(w.window, Qt::Key_Return);
    QTRY_COMPARE(w.recorder.chosen, QStringList{QStringLiteral("console")});
    QVERIFY(w.recorder.spawned.isEmpty());
    QTRY_VERIFY(w.navigation->launchError().contains(QStringLiteral("terminal")));
    auto *banner = w.item("launchErrorBanner");
    QVERIFY(banner && banner->isVisible());
    QCOMPARE(banner->property("title").toString(), QStringLiteral("Couldn't open the application"));

    // Arrow keys move like any folder; Enter on a plain entry spawns it.
    QTest::keyClick(w.window, Qt::Key_Right);
    QTRY_COMPARE(grid->property("currentIndex").toInt(), 3);
    QTest::keyClick(w.window, Qt::Key_Enter);
    QTRY_COMPARE(w.recorder.spawned.size(), 1);
    QCOMPARE(w.recorder.chosen.last(), QStringLiteral("editor"));
}

void ApplicationsUiTests::filterDetailsViewAndGroupByCategory()
{
    QVERIFY(open());
    auto &w = *m_window;
    w.coordinator.activateAction(QStringLiteral("go.applications"));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    auto *grid = w.item("entryGridView");
    grid->forceActiveFocus();

    // The filter bar filters application names like file names.
    QTest::keyClick(w.window, Qt::Key_F, Qt::ControlModifier);
    auto *field = w.item("folderFilterField");
    QTRY_VERIFY(field && field->hasActiveFocus());
    auto *subfolders = w.item("filterSubfoldersToggle");
    QVERIFY(subfolders && !subfolders->isEnabled());
    for (const char character : {'z', 'e', 't', 'a'}) {
        QTest::keyClick(w.window, character);
    }
    QTRY_COMPARE(w.navigation->entries().size(), 1);
    QTest::keyClick(w.window, Qt::Key_Escape);
    QTRY_COMPARE(w.navigation->entries().size(), 5);

    // Details view (ADR-0270's Tk.DataTable): the Kind column is the category.
    QTest::keyClick(w.window, Qt::Key_1, Qt::ControlModifier);
    QTRY_COMPARE(w.navigation->viewMode(), QStringLiteral("list"));
    auto *list = w.item("entryListView");
    QTRY_COMPARE(list->property("count").toInt(), 5);
    auto *details = w.window->findChild<QObject *>(QStringLiteral("detailsView"));
    QVERIFY(details);
    auto *columns = details->findChild<QObject *>(QStringLiteral("detailsColumnSet"));
    QVERIFY(columns);
    const auto kindTitle = [columns] {
      QVariant title;
      QMetaObject::invokeMethod(columns, "titleOf", Q_RETURN_ARG(QVariant, title),
                                Q_ARG(QVariant, QStringLiteral("kind")));
      return title.toString();
    };
    QCOMPARE(kindTitle(), QStringLiteral("Category"));

    // View > Group by Category (Ctrl+G) is the category sort with headings.
    QVERIFY(w.actionEnabled(QStringLiteral("view.group-by-category")));
    // Group By proper is for folders: Applications groups by category.
    QVERIFY(!w.actionEnabled(QStringLiteral("view.group-kind")));
    list->forceActiveFocus();
    QTest::keyClick(w.window, Qt::Key_G, Qt::ControlModifier);
    QTRY_COMPARE(w.navigation->sortColumn(), QStringLiteral("kind"));
    QTRY_COMPARE(details->property("headingCount").toInt(), 5);
    QTest::keyClick(w.window, Qt::Key_G, Qt::ControlModifier);
    QTRY_COMPARE(w.navigation->sortColumn(), QStringLiteral("name"));
    QTRY_COMPARE(details->property("headingCount").toInt(), 0);

    // Leaving Applications while grouped restores the folder's column title
    // and its own sort, and disables grouping; coming back keeps it grouped,
    // in the Details view the place was given.
    QTest::keyClick(w.window, Qt::Key_G, Qt::ControlModifier);
    QTRY_COMPARE(w.navigation->sortColumn(), QStringLiteral("kind"));
    w.navigation->navigateTo(m_temporary->path());
    QTRY_COMPARE(kindTitle(), QStringLiteral("Kind"));
    QCOMPARE(w.navigation->sortColumn(), QStringLiteral("name"));
    QTRY_COMPARE(details->property("headingCount").toInt(), 0);
    QVERIFY(!w.actionEnabled(QStringLiteral("view.group-by-category")));
    w.navigation->goBack();
    QTRY_VERIFY(w.navigation->applicationsPlace());
    QCOMPARE(w.navigation->sortColumn(), QStringLiteral("kind"));
    QTRY_COMPARE(w.navigation->viewMode(), QStringLiteral("list"));
    QTRY_COMPARE(details->property("headingCount").toInt(), 5);
}

void ApplicationsUiTests::contextMenuOffersApplicationActions()
{
    QVERIFY(open());
    auto &w = *m_window;
    w.coordinator.activateAction(QStringLiteral("go.applications"));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    auto *grid = w.item("entryGridView");
    QTRY_VERIFY(delegateFor(grid, 3));
    auto *menu = w.window->findChild<QObject *>(QStringLiteral("gridContextMenu"));
    QVERIFY(menu);
    const auto menuItem = [menu](const char *name) {
        return menu->findChild<QQuickItem *>(QString::fromLatin1(name));
    };
    const auto shown = [&](const char *name) {
        auto *entry = menuItem(name);
        return entry && entry->property("visible").toBool();
    };

    QTest::mouseClick(w.window, Qt::RightButton, Qt::NoModifier, centerOf(delegateFor(grid, 3)));
    QTRY_VERIFY(menu->property("visible").toBool());
    QCOMPARE(menu->property("selectionCount").toInt(), 1);
    QVERIFY(shown("contextOpenAction"));
    QVERIFY(shown("contextPropertiesAction"));
    QCOMPARE(menuItem("contextPropertiesAction")->property("text").toString(), QStringLiteral("Get Info"));
    QVERIFY(shown("contextShowEntryFileAction"));
    for (const char *fileAction : {"contextCutAction", "contextClipboardCopyAction", "contextRenameAction",
                                   "contextCopyAction", "contextMoveAction", "contextTrashAction"}) {
        QVERIFY2(!shown(fileAction), fileAction);
    }
    QVERIFY(menuItem("contextOpenAction")->isEnabled());

    // Open goes through the same activation path as a double-click.
    QMetaObject::invokeMethod(menuItem("contextOpenAction"), "triggered");
    QTRY_COMPARE(w.recorder.chosen, QStringList{QStringLiteral("editor")});
    QCOMPARE(w.recorder.spawned.size(), 1);

    // Get Info shows the catalog's description of the selected application.
    QMetaObject::invokeMethod(menuItem("contextPropertiesAction"), "triggered");
    auto *info = w.window->findChild<QObject *>(QStringLiteral("applicationInfoDialog"));
    QVERIFY(info);
    QTRY_VERIFY(info->property("visible").toBool());
    auto *command = w.window->findChild<QQuickItem *>(QStringLiteral("applicationInfo_command"));
    QVERIFY(command);
    QCOMPARE(command->property("text").toString(), QStringLiteral("/usr/bin/editor --flag \"two words\""));
    auto *entryFile = w.window->findChild<QQuickItem *>(QStringLiteral("applicationInfo_desktopFilePath"));
    QVERIFY(entryFile);
    QCOMPARE(entryFile->property("text").toString(),
             m_temporary->filePath(QStringLiteral("data/applications/editor.desktop")));
    QMetaObject::invokeMethod(info, "close");
    QTRY_VERIFY(!info->property("visible").toBool());

    // Show Desktop Entry File navigates to the entry's folder and selects it.
    QVERIFY(w.actionEnabled(QStringLiteral("application.show-entry-file")));
    QMetaObject::invokeMethod(menuItem("contextShowEntryFileAction"), "triggered");
    QTRY_COMPARE(w.navigation->currentPath(), m_temporary->filePath(QStringLiteral("data/applications")));
    const int entryIndex = w.navigation->indexOfName(QStringLiteral("editor.desktop"));
    QVERIFY(entryIndex >= 0);
    QTRY_COMPARE(grid->property("currentIndex").toInt(), entryIndex);
    QTRY_COMPARE(w.clipboard.selectionCount(), 1);

    // Back in Applications, the background menu offers grouping instead of
    // New Folder, Paste and Show Hidden Files.
    w.navigation->goBack();
    QTRY_VERIFY(w.navigation->applicationsPlace());
    const qreal cellHeight = grid->property("cellHeight").toReal();
    QTest::mouseClick(w.window, Qt::RightButton, Qt::NoModifier,
                      grid->mapToScene(QPointF(40, cellHeight + 30)).toPoint());
    QTRY_VERIFY(menu->property("visible").toBool());
    QCOMPARE(menu->property("selectionCount").toInt(), 0);
    QVERIFY(shown("contextGroupByCategoryAction"));
    QVERIFY(shown("contextRefreshAction"));
    QVERIFY(!shown("contextNewFolderAction"));
    QVERIFY(!shown("contextShowHiddenAction"));
    QVERIFY(!shown("contextOpenAction"));
    QMetaObject::invokeMethod(menu, "close");
}

void ApplicationsUiTests::fileActionsStayOffInsideApplications()
{
    QVERIFY(open());
    auto &w = *m_window;
    auto *newFolder = w.item("newFolderButton");
    QVERIFY(newFolder && newFolder->property("available").toBool());
    QVERIFY(w.actionEnabled(QStringLiteral("file.new-folder")));
    QVERIFY(w.actionEnabled(QStringLiteral("bookmark.add")));

    w.coordinator.activateAction(QStringLiteral("go.applications"));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    w.coordinator.activateAction(QStringLiteral("edit.select-all"));
    QTRY_COMPARE(w.clipboard.selectionCount(), 5);
    // Application rows are never files: nothing can be created, cut, copied,
    // renamed, moved, trashed or bookmarked here, while Open and Get Info can.
    for (const char *id : {"file.new-folder", "file.rename", "file.copy", "file.move", "file.trash",
                           "edit.cut", "edit.copy", "edit.paste", "bookmark.add",
                           "application.show-entry-file"}) {
        QVERIFY2(!w.actionEnabled(QString::fromLatin1(id)), id);
    }
    for (const char *id : {"file.open", "file.properties", "view.filter", "view.grid-mode",
                           "view.details-mode", "view.zoom-in", "edit.select-all", "go.back"}) {
        QVERIFY2(w.actionEnabled(QString::fromLatin1(id)), id);
    }
    QVERIFY(!newFolder->property("available").toBool());
    QVERIFY(!w.navigation->canGoUp());

    // Opening several selected applications opens each one (ADR-0269: the
    // window-wide file.open, the same NavigationController activation).
    w.coordinator.activateAction(QStringLiteral("file.open"));
    QTRY_COMPARE(w.recorder.chosen.size(), 5);

    // Zoom resizes application icons like file icons.
    const int before = w.navigation->iconSize();
    w.coordinator.activateAction(QStringLiteral("view.zoom-in"));
    QTRY_VERIFY(w.navigation->iconSize() > before);

    w.coordinator.activateAction(QStringLiteral("go.back"));
    QTRY_VERIFY(!w.navigation->applicationsPlace());
    QTRY_VERIFY(w.actionEnabled(QStringLiteral("file.new-folder")));
    QVERIFY(!w.actionEnabled(QStringLiteral("file.open")));
}

void ApplicationsUiTests::chooserModeKeepsItsSemantics()
{
    QVERIFY(open(true));
    auto &w = *m_window;
    QVERIFY(w.navigation->applicationsPlace());
    auto *hint = w.item("chooserHint");
    QVERIFY(hint);
    QTRY_VERIFY(hint->isVisible());
    // Every entry is choosable in a picker, terminal ones included.
    for (const auto &row : w.navigation->entries()) {
        QVERIFY(row.toMap().value(QStringLiteral("launchable")).toBool());
    }
    QSignalSpy quit(&w.coordinator, &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested);
    auto *grid = w.item("entryGridView");
    grid->forceActiveFocus();
    QTRY_VERIFY(grid->hasActiveFocus());

    // A rejected choice keeps the picker open with the reason; never a spawn.
    QTest::keyClick(w.window, Qt::Key_C);
    QTRY_COMPARE(grid->property("currentIndex").toInt(), 2);
    QTest::keyClick(w.window, Qt::Key_Return);
    QTRY_COMPARE(w.recorder.chosen, QStringList{QStringLiteral("console")});
    QTRY_COMPARE(w.navigation->launchError(), w.recorder.rejection);
    QVERIFY(w.recorder.spawned.isEmpty());
    QCOMPARE(quit.count(), 0);

    // An accepted choice closes the picker through the standard quit request.
    w.recorder.accept = true;
    QTest::keyClick(w.window, Qt::Key_Return);
    QTRY_COMPARE(quit.count(), 1);
    QCOMPARE(quit.first().at(1).toString(), QStringLiteral("application-chosen"));
    QVERIFY(w.recorder.spawned.isEmpty());
}

void ApplicationsUiTests::aDockedWindowIsReplacedRatherThanSpawning()
{
    QVERIFY(open());
    auto &w = *m_window;
    w.recorder.accept = true; // the compositor answers for a docked window
    QSignalSpy quit(&w.coordinator, &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested);
    w.coordinator.activateAction(QStringLiteral("go.applications"));
    QTRY_VERIFY(w.navigation->applicationsPlace());
    auto *hint = w.item("chooserHint");
    QVERIFY(hint && !hint->isVisible());
    auto *grid = w.item("entryGridView");
    QTRY_VERIFY(delegateFor(grid, 2));
    // ADR-0172: the application takes this window's place, so nothing is
    // spawned locally -- even a terminal entry -- and the window closes.
    QTest::mouseDClick(w.window, Qt::LeftButton, Qt::NoModifier, centerOf(delegateFor(grid, 2)));
    QTRY_COMPARE(quit.count(), 1);
    QCOMPARE(w.recorder.chosen, QStringList{QStringLiteral("console")});
    QVERIFY(w.recorder.spawned.isEmpty());
}

QTEST_MAIN(ApplicationsUiTests)
#include "tst_applications_ui.moc"
