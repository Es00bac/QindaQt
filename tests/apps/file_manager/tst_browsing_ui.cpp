// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/places_controller.h"
#include "model/search_controller.h"
#include "window_fixtures.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"

#include <QClipboard>
#include <QFile>
#include <QDir>
#include <QWheelEvent>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <qindaqt/app_shell/application_coordinator.h>

using namespace QindaQt::Apps::FileManager;

class BrowsingUiTests final : public QObject {
  Q_OBJECT
private slots:
  void keyboardWheelAndFilter_data();
  void keyboardWheelAndFilter();
  void contextMenusTargetBackgroundAndSelection_data();
  void contextMenusTargetBackgroundAndSelection();
  void contextMenuDisablesDuringMutation();
};

void BrowsingUiTests::keyboardWheelAndFilter_data() {
  QTest::addColumn<QSize>("windowSize");
  QTest::addColumn<Qt::ColorScheme>("scheme");
  QTest::newRow("compact-light") << QSize(480, 360) << Qt::ColorScheme::Light;
  QTest::newRow("desktop-dark") << QSize(1280, 800) << Qt::ColorScheme::Dark;
  QTest::newRow("1080p-dark") << QSize(1920, 1080) << Qt::ColorScheme::Dark;
}

void BrowsingUiTests::keyboardWheelAndFilter() {
  QFETCH(QSize, windowSize);
  QFETCH(Qt::ColorScheme, scheme);
  // ADR-0116: appearance comes from the platform theme; the declarative color
  // scheme stands in for the session palette under the generic test theme.
  QGuiApplication::styleHints()->setColorScheme(scheme);
  const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath("files");
  QVERIFY(QDir().mkpath(folder));
  for (int i = 0; i < 160; ++i) {
    QFile file(folder + QStringLiteral("/Document-%1.txt").arg(i, 3, 10, QLatin1Char('0')));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("test"), qint64(4));
  }
  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<Test::FakeFileLauncher>());
  MutationController mutation(std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places(std::make_unique<BookmarksStore>(temporary.filePath("state")));
  Test::WindowSupportControllers support(temporary.path());
  ApplicationsController applications(QStringList{});
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions(fileManagerActionCatalog()).ok());
  bindFileManagerBrowsingActions(coordinator, navigation);
  navigation.navigateTo(folder);

  QQmlApplicationEngine engine;
  QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  QVERIFY(QIcon::hasThemeIcon(QStringLiteral("list-add-symbolic")));
  QVERIFY(QIcon::hasThemeIcon(QStringLiteral("list-remove-symbolic")));
  auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", previews);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  previews->setGeneration(navigation.listingGeneration());
  QVariantMap initialProperties{
      {"navigationController", QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {"mutationController", QVariant::fromValue(static_cast<QObject *>(&mutation))},
      {"clipboardController", QVariant::fromValue(static_cast<QObject *>(&clipboard))},
      {"propertiesController", QVariant::fromValue(static_cast<QObject *>(&properties))},
      {"searchController", QVariant::fromValue(static_cast<QObject *>(&search))},
      {"placesController", QVariant::fromValue(static_cast<QObject *>(&places))},
      {"applicationsController", QVariant::fromValue(static_cast<QObject *>(&applications))},
      {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}};
  support.insertInto(initialProperties);
  engine.setInitialProperties(initialProperties);
  engine.load(QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  QVERIFY(!engine.rootObjects().isEmpty());
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  QVERIFY(window);
  window->resize(windowSize);
  window->requestActivate();
  QTRY_VERIFY(window->isExposed());
  QTRY_VERIFY(window->isActive());
  auto *grid = window->findChild<QQuickItem *>("entryGridView");
  auto *list = window->findChild<QQuickItem *>("entryListView");
  QVERIFY(grid && list);
  grid->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_End);
  QTRY_COMPARE(grid->property("currentIndex").toInt(), 159);
  QVERIFY(grid->property("contentY").toReal() > 0);

  QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
  QTRY_COMPARE(navigation.viewMode(), QStringLiteral("list"));
  QTRY_VERIFY(list->hasActiveFocus());
  QCOMPARE(list->property("currentIndex").toInt(), 159);
  QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
  QTRY_VERIFY(grid->hasActiveFocus());
  QTRY_COMPARE(navigation.viewMode(), QStringLiteral("grid"));
  QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
  QCOMPARE(navigation.viewMode(), QStringLiteral("grid"));
  QObject *gridAction = nullptr;
  for (QObject *object : window->findChildren<QObject *>()) {
    if (object->property("modelData").toMap().value("id").toString()
        == QStringLiteral("view.grid-mode")) gridAction = object;
  }
  QVERIFY(gridAction);
  QVERIFY(!gridAction->property("checkable").toBool());
  QVERIFY(!gridAction->property("checked").toBool());

  const QPointF local = grid->mapToScene(QPointF(grid->width() / 2, grid->height() / 2));
  const auto wheel = [&](Qt::KeyboardModifiers modifiers) {
    QWheelEvent event(local, window->mapToGlobal(local.toPoint()), {}, QPoint(0, 120),
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &event);
  };
  wheel(Qt::ControlModifier);
  QTRY_COMPARE(navigation.iconSize(), 96);
  QCOMPARE(grid->property("currentIndex").toInt(), 159);
  QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
  QTRY_COMPARE(navigation.iconSize(), 64);
  wheel(Qt::NoModifier);
  QCOMPARE(navigation.iconSize(), 64);

  QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
  auto *field = window->findChild<QQuickItem *>("folderFilterField");
  QVERIFY(field);
  QTRY_VERIFY(field->hasActiveFocus());
  for (QChar character : QStringLiteral("Document-159"))
    QTest::keyClick(window, character.toLatin1());
  QTRY_COMPARE(navigation.entries().size(), 1);
  QCOMPARE(navigation.entries().first().toMap().value("name").toString(), QStringLiteral("Document-159.txt"));
  QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClick(window, Qt::Key_Delete);
  QTRY_VERIFY(navigation.nameFilter().isEmpty());
  auto *trashDialog = window->findChild<QObject *>("trashConfirmationDialog");
  QVERIFY(trashDialog && !trashDialog->property("visible").toBool());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(navigation.entries().size(), 160);
  QTRY_VERIFY(grid->hasActiveFocus());

  // Location dismissal restores browsing focus. The real notifier must also
  // leave special routes when a folder is opened.
  QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
  auto *locationField = window->findChild<QQuickItem *>("locationField");
  QVERIFY(locationField);
  QTRY_VERIFY(locationField->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(grid->hasActiveFocus());
  // ADR-0262: Applications is an ordinary location now, so only the Network
  // hub is a route the window must leave.
  window->setProperty("networkMode", true);
  navigation.navigateTo(temporary.path());
  QTRY_VERIFY(!window->property("networkMode").toBool());
  navigation.navigateTo(folder);

  auto *forward = window->findChild<QQuickItem *>("navigateForwardButton");
  QVERIFY(forward && forward->isVisible());
  const auto toolbar = window->findChild<QQuickItem *>("fileManagerToolbar");
  QVERIFY(toolbar && toolbar->width() <= window->width());
}

void BrowsingUiTests::contextMenusTargetBackgroundAndSelection_data() {
  QTest::addColumn<QSize>("windowSize");
  QTest::newRow("compact") << QSize(480, 360);
  QTest::newRow("desktop") << QSize(1280, 800);
}

// Failure-before/passing-after coverage for VISIBLE-FM-1: right-clicking (or
// invoking the context menu by keyboard on) empty folder space must offer
// only background actions, a selection must offer only applicable existing
// actions, Rename must disappear for anything but exactly one entry, and
// keyboard invocation must follow the focused item versus true background.
// Every assertion reads the production FileContextMenu instances Main.qml
// actually builds; every dispatch proof goes through the real
// ApplicationCoordinator/MutationDialogs path, not a synthetic shortcut.
void BrowsingUiTests::contextMenusTargetBackgroundAndSelection() {
  QFETCH(QSize, windowSize);
  const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath("files");
  QVERIFY(QDir().mkpath(folder));
  // Exactly two entries: the places sidebar leaves as few as two grid columns
  // at the compact size, so two entries (never three) are guaranteed to fit
  // in row 0 regardless of window width, leaving row 1 reliably empty.
  const QStringList names = {QStringLiteral("alpha.txt"), QStringLiteral("beta.txt")};
  for (const QString &name : names) {
    QFile file(folder + QLatin1Char('/') + name);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("test"), qint64(4));
  }
  // A folder that lists cleanly but whose only entry is hidden stays Ready
  // with an empty *visible* list: real background/no-current-item coverage
  // for the keyboard path, distinct from a truly empty (Empty-status) folder
  // that would swap the view out for the state card entirely.
  const QString hiddenOnlyFolder = temporary.filePath("hidden-only");
  QVERIFY(QDir().mkpath(hiddenOnlyFolder));
  QFile dotFile(hiddenOnlyFolder + QStringLiteral("/.secret"));
  QVERIFY(dotFile.open(QIODevice::WriteOnly));
  QCOMPARE(dotFile.write("x"), qint64(1));

  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<Test::FakeFileLauncher>());
  MutationController mutation(std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places(std::make_unique<BookmarksStore>(temporary.filePath("state")));
  Test::WindowSupportControllers support(temporary.path());
  ApplicationsController applications(QStringList{});
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions(fileManagerActionCatalog()).ok());
  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerTransferActions(coordinator, navigation, clipboard, mutation);
  navigation.navigateTo(folder);

  QQmlApplicationEngine engine;
  auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", previews);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  previews->setGeneration(navigation.listingGeneration());
  QVariantMap initialProperties{
      {"navigationController", QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {"mutationController", QVariant::fromValue(static_cast<QObject *>(&mutation))},
      {"clipboardController", QVariant::fromValue(static_cast<QObject *>(&clipboard))},
      {"propertiesController", QVariant::fromValue(static_cast<QObject *>(&properties))},
      {"searchController", QVariant::fromValue(static_cast<QObject *>(&search))},
      {"placesController", QVariant::fromValue(static_cast<QObject *>(&places))},
      {"applicationsController", QVariant::fromValue(static_cast<QObject *>(&applications))},
      {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}};
  support.insertInto(initialProperties);
  engine.setInitialProperties(initialProperties);
  engine.load(QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  QVERIFY(!engine.rootObjects().isEmpty());
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  QVERIFY(window);
  window->resize(windowSize);
  window->requestActivate();
  QTRY_VERIFY(window->isExposed());
  QTRY_VERIFY(window->isActive());

  auto *grid = window->findChild<QQuickItem *>("entryGridView");
  auto *list = window->findChild<QQuickItem *>("entryListView");
  QVERIFY(grid && list);
  QTRY_COMPARE(navigation.entries().size(), 2);

  // Pristine keyboard case: nothing has been clicked yet, so the first entry
  // is merely focused (EntrySelection::reconcile's initial focusIndex(0)),
  // not yet explicitly selected. The Menu key must still resolve this as a
  // one-item selection (the same "click selects" rule the mouse path already
  // applies), never as background.
  grid->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_Menu);
  auto *gridMenu = window->findChild<QObject *>("gridContextMenu");
  auto *listMenu = window->findChild<QObject *>("listContextMenu");
  QVERIFY(gridMenu && listMenu);
  QCOMPARE(gridMenu->property("selectionCount").toInt(), 1);
  QMetaObject::invokeMethod(gridMenu, "close");

  const auto findItem = [](QObject *scope, const char *name) {
    return scope->findChild<QQuickItem *>(QString::fromLatin1(name));
  };
  const auto itemVisible = [&](QObject *menu, const char *name) {
    auto *item = findItem(menu, name);
    return item && item->property("visible").toBool();
  };
  // Asserts the background/selection item split for a menu currently
  // targeting expectedSelectionCount entries (0 == background).
  const auto assertMenuState = [&](QObject *menu, int expectedSelectionCount) {
    QVERIFY(menu);
    const bool background = expectedSelectionCount == 0;
    QCOMPARE(menu->property("selectionCount").toInt(), expectedSelectionCount);
    QCOMPARE(itemVisible(menu, "contextNewFolderAction"), background);
    QCOMPARE(itemVisible(menu, "contextRefreshAction"), background);
    // ADR-0269: View ▸ (a sub-menu entry named when the menu opens).
    QCOMPARE(itemVisible(menu, "contextViewMenu"), background);
    QCOMPARE(itemVisible(menu, "contextShowHiddenAction"), background);
    QCOMPARE(itemVisible(menu, "contextCutAction"), !background);
    QCOMPARE(itemVisible(menu, "contextClipboardCopyAction"), !background);
    QCOMPARE(itemVisible(menu, "contextCopyAction"), !background);
    QCOMPARE(itemVisible(menu, "contextMoveAction"), !background);
    QCOMPARE(itemVisible(menu, "contextTrashAction"), !background);
    QCOMPARE(itemVisible(menu, "contextPropertiesAction"), !background);
    QCOMPARE(itemVisible(menu, "contextRenameAction"), expectedSelectionCount == 1);
  };

  for (const QString &mode : {QStringLiteral("grid"), QStringLiteral("list")}) {
    navigation.setViewMode(mode);
    const bool isGrid = mode == QStringLiteral("grid");
    QQuickItem *view = isGrid ? grid : list;
    QObject *menu = isGrid ? gridMenu : listMenu;
    QVERIFY(view && menu);
    view->forceActiveFocus();
    QTRY_VERIFY(view->hasActiveFocus());
    // StackLayout only finishes sizing the page that just became current on
    // the next layout pass; wait for it rather than racing a stale height=0.
    QTRY_VERIFY(view->height() > 0);

    // Grid: the sidebar leaves as few as two columns at the compact size, so
    // both entries always land in row 0; a point just inside row 1 is
    // reliably empty and needs far less headroom than that row's center.
    // List rows stack in a single column regardless of width, so a point
    // just inside row 2 (both entries fit in rows 0-1) is the equivalent
    // guaranteed-empty target there.
    const qreal cellWidth = isGrid ? view->property("cellWidth").toReal() : view->width();
    const qreal cellHeight = isGrid ? view->property("cellHeight").toReal() : 44.0;
    const qreal backgroundY = isGrid ? cellHeight + 20 : cellHeight * 2 + 10;
    QVERIFY(view->height() > backgroundY + 10);
    const QPointF backgroundLocal(cellWidth / 2, backgroundY);
    const QPointF firstLocal(cellWidth / 2, cellHeight / 2);
    const QPointF secondLocal = isGrid ? QPointF(cellWidth * 1.5, cellHeight / 2)
                                       : QPointF(cellWidth / 2, cellHeight * 1.5);

    // Background: right-click the row/column reliably below both entries.
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                       view->mapToScene(backgroundLocal).toPoint());
    assertMenuState(menu, 0);
    QMetaObject::invokeMethod(menu, "close");

    // Single selection: left-click selects, right-click on it targets it.
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                       view->mapToScene(firstLocal).toPoint());
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                       view->mapToScene(firstLocal).toPoint());
    assertMenuState(menu, 1);
    QMetaObject::invokeMethod(menu, "close");

    // Multi selection: Ctrl-click the second entry, then right-click the
    // still-selected first entry again — Rename must disappear.
    QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier,
                       view->mapToScene(secondLocal).toPoint());
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                       view->mapToScene(firstLocal).toPoint());
    assertMenuState(menu, 2);
    QMetaObject::invokeMethod(menu, "close");

    // Keyboard invocation with a single selected entry mirrors the mouse
    // single-selection case.
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                       view->mapToScene(firstLocal).toPoint());
    view->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Menu);
    assertMenuState(menu, 1);
    QMetaObject::invokeMethod(menu, "close");
  }

  // Keyboard invocation with no focused item at all: a Ready folder whose
  // only entry is hidden leaves currentIndex at -1, so the Menu key must
  // resolve to the background menu, not silently reuse the last selection.
  // Switch back to the grid page first: the loop above ends in list mode,
  // and StackLayout only lets the current page take active focus.
  navigation.setViewMode(QStringLiteral("grid"));
  navigation.navigateTo(hiddenOnlyFolder);
  QTRY_COMPARE(navigation.entries().size(), 0);
  grid->forceActiveFocus();
  QTRY_VERIFY(grid->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Menu);
  assertMenuState(gridMenu, 0);
  QMetaObject::invokeMethod(gridMenu, "close");

  // New Folder / Paste dispatch: a fresh folder (one file, one empty
  // subfolder) keeps the paste destination collision-free so a real
  // identity-checked copy can land without an already-exists refusal. List
  // mode keeps the row math independent of the sidebar-driven column count.
  const QString dispatchFolder = temporary.filePath("dispatch");
  QVERIFY(QDir().mkpath(dispatchFolder));
  QVERIFY(QDir().mkpath(dispatchFolder + QStringLiteral("/dest")));
  QFile sourceFile(dispatchFolder + QStringLiteral("/source.txt"));
  QVERIFY(sourceFile.open(QIODevice::WriteOnly));
  QCOMPARE(sourceFile.write("test"), qint64(4));
  navigation.setViewMode(QStringLiteral("list"));
  navigation.navigateTo(dispatchFolder);
  QTRY_COMPARE(navigation.entries().size(), 2);
  list->forceActiveFocus();
  QTRY_VERIFY(list->hasActiveFocus());
  QTRY_VERIFY(list->height() > 0);
  const qreal rowHeight = 44.0;
  const qreal listWidth = list->width();

  // New Folder: the context menu's action id reaches the same
  // MutationDialogs/MutationController path the toolbar button uses. Two
  // entries occupy rows 0-1 (through y=88); a point just past that, rather
  // than row 2's full center, needs the least headroom below.
  auto *newFolderDialog = window->findChild<QObject *>("newFolderDialog");
  QVERIFY(newFolderDialog && !newFolderDialog->property("visible").toBool());
  const qreal twoEntriesBackgroundY = rowHeight * 2 + 15;
  QVERIFY(list->height() > twoEntriesBackgroundY + 10);
  QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                     list->mapToScene(QPointF(listWidth / 2, twoEntriesBackgroundY)).toPoint());
  assertMenuState(listMenu, 0);
  auto *newFolderAction = findItem(listMenu, "contextNewFolderAction");
  QVERIFY(newFolderAction && newFolderAction->property("visible").toBool());
  QMetaObject::invokeMethod(newFolderAction, "triggered");
  QTRY_VERIFY(newFolderDialog->property("visible").toBool());
  auto *newFolderNameField = window->findChild<QQuickItem *>("newFolderNameField");
  QVERIFY(newFolderNameField);
  newFolderNameField->setProperty("text", QStringLiteral("Made-via-context-menu"));
  QMetaObject::invokeMethod(newFolderDialog, "accept");
  QTRY_VERIFY(QDir(dispatchFolder).exists(QStringLiteral("Made-via-context-menu")));
  QTRY_COMPARE(navigation.entries().size(), 3);

  // Paste: prove a background invocation lands in the browsed folder itself
  // and NOT in a directory that merely remains focused elsewhere — the exact
  // defect a prior version of this test let through by focusing "dest" and
  // then asserting the pasted file landed inside "dest", which only
  // confirmed the bug. The clipboard source lives in a separate folder
  // (never listed in dispatchFolder or "dest") so the paste destination is
  // unambiguous and collision-free in either directory.
  const QString pasteSourceFolder = temporary.filePath("paste-source");
  QVERIFY(QDir().mkpath(pasteSourceFolder));
  QFile externalFile(pasteSourceFolder + QStringLiteral("/external.txt"));
  QVERIFY(externalFile.open(QIODevice::WriteOnly));
  QCOMPARE(externalFile.write("test"), qint64(4));
  navigation.navigateTo(pasteSourceFolder);
  QTRY_COMPARE(navigation.entries().size(), 1);
  QVERIFY(clipboard.copySelection({navigation.entries().at(0)}));

  // Directories sort first, case-insensitively — "dest" (row 0),
  // "Made-via-context-menu" (row 1), then the file "source.txt" (row 2).
  navigation.navigateTo(dispatchFolder);
  QTRY_COMPARE(navigation.entries().size(), 3);
  QCOMPARE(navigation.entries().at(0).toMap().value("name").toString(), QStringLiteral("dest"));
  list->forceActiveFocus();
  QTRY_VERIFY(list->hasActiveFocus());
  QTRY_VERIFY(list->height() > 0);

  // Focus "dest" (a real directory elsewhere in this folder) so the fix is
  // proven against exactly the state the defect needed: a focused directory
  // that must not receive a background paste.
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                     list->mapToScene(QPointF(listWidth / 2, rowHeight / 2)).toPoint());
  const qreal threeEntriesBackgroundY = rowHeight * 3 + 15;
  QVERIFY(list->height() > threeEntriesBackgroundY + 10);
  QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                     list->mapToScene(QPointF(listWidth / 2, threeEntriesBackgroundY)).toPoint());
  assertMenuState(listMenu, 0);
  QTRY_VERIFY(itemVisible(listMenu, "contextBackgroundPasteAction"));
  auto *pasteAction = findItem(listMenu, "contextBackgroundPasteAction");
  QVERIFY(pasteAction);
  // Proves the fix for the coordinator-boundary blocker: background Paste
  // must cross ApplicationCoordinator::activateAction("edit.paste") exactly
  // once, the same known/enabled gate every other trigger uses, rather than
  // calling clipboardController.pasteInto() directly.
  QSignalSpy actionRequestedSpy(&coordinator, &QindaQt::AppShell::ApplicationCoordinator::actionRequested);
  QMetaObject::invokeMethod(pasteAction, "triggered");
  QTRY_VERIFY(QFile::exists(dispatchFolder + QStringLiteral("/external.txt")));
  QVERIFY(QDir(dispatchFolder + QStringLiteral("/dest"))
              .entryList(QDir::Files | QDir::NoDotAndDotDot)
              .isEmpty());
  const qsizetype editPasteRequests = std::count_if(
      actionRequestedSpy.cbegin(), actionRequestedSpy.cend(),
      [](const QList<QVariant> &signal) {
        return signal.at(0).toString() == QStringLiteral("edit.paste");
      });
  QCOMPARE(editPasteRequests, qsizetype(1));
}

// Focused coverage for VISIBLE-FM-1's second blocking review finding: the
// shared context menu's enabled state must track the exact authoritative
// truth bindFileManagerTransferActions/the mutation-busy handler in main.cpp
// already drive (the same coordinator.menus[].actions[].enabled the menu bar
// reads), not an independently-computed check that can drift out of sync
// with it. A real MutationController operation — not a synthetic flag flip —
// drives the busy window: MutationController::submit sets busy synchronously
// before the worker thread starts, so busy() is already true, and the
// coordinator's busy-driven sync has already run, the instant createFolder()
// returns; no event-loop wait is needed to observe either.
void BrowsingUiTests::contextMenuDisablesDuringMutation() {
  const QString sourceRoot = QStringLiteral(QINDAQT_SOURCE_DIR);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath("files");
  QVERIFY(QDir().mkpath(folder));
  QFile existing(folder + QStringLiteral("/existing.txt"));
  QVERIFY(existing.open(QIODevice::WriteOnly));
  QCOMPARE(existing.write("test"), qint64(4));

  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<Test::FakeFileLauncher>());
  MutationController mutation(std::make_unique<LocalMutationBackend>(temporary.filePath("Trash")));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places(std::make_unique<BookmarksStore>(temporary.filePath("state")));
  Test::WindowSupportControllers support(temporary.path());
  ApplicationsController applications(QStringList{});
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions(fileManagerActionCatalog()).ok());
  bindFileManagerBrowsingActions(coordinator, navigation);
  bindFileManagerTransferActions(coordinator, navigation, clipboard, mutation);
  // main.cpp wires this exact busy-state disabling for file.new-folder/
  // rename/copy/move/trash/empty-trash directly in configureAppShell rather
  // than through a reusable bind*Actions helper, so it is reproduced here
  // verbatim (same action ids, same idle condition) to give this test's
  // coordinator the identical authoritative state production Main.qml relies
  // on — not a substitute policy.
  QObject::connect(&mutation, &MutationController::stateChanged, &coordinator,
                   [&coordinator, &mutation] {
    const bool idle = !mutation.busy();
    for (const QString &actionId :
         {QStringLiteral("file.new-folder"), QStringLiteral("file.rename"),
          QStringLiteral("file.copy"), QStringLiteral("file.move"),
          QStringLiteral("file.trash"), QStringLiteral("file.empty-trash")}) {
      const auto result = coordinator.setActionEnabled(actionId, idle);
      Q_UNUSED(result);
    }
  });
  navigation.navigateTo(folder);

  QQmlApplicationEngine engine;
  auto *previews = new PreviewProvider(std::make_unique<LocalPreviewDecoder>());
  engine.addImageProvider("previews", previews);
  engine.addImageProvider("theme-icons", new ThemeIconProvider());
  previews->setGeneration(navigation.listingGeneration());
  QVariantMap initialProperties{
      {"navigationController", QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {"mutationController", QVariant::fromValue(static_cast<QObject *>(&mutation))},
      {"clipboardController", QVariant::fromValue(static_cast<QObject *>(&clipboard))},
      {"propertiesController", QVariant::fromValue(static_cast<QObject *>(&properties))},
      {"searchController", QVariant::fromValue(static_cast<QObject *>(&search))},
      {"placesController", QVariant::fromValue(static_cast<QObject *>(&places))},
      {"applicationsController", QVariant::fromValue(static_cast<QObject *>(&applications))},
      {"coordinator", QVariant::fromValue(static_cast<QObject *>(&coordinator))}};
  support.insertInto(initialProperties);
  engine.setInitialProperties(initialProperties);
  engine.load(QUrl::fromLocalFile(sourceRoot + "/src/apps/file_manager/ui/Main.qml"));
  QVERIFY(!engine.rootObjects().isEmpty());
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  QVERIFY(window);
  window->resize(QSize(480, 360));
  window->requestActivate();
  QTRY_VERIFY(window->isExposed());
  QTRY_VERIFY(window->isActive());
  QTRY_COMPARE(navigation.entries().size(), 1);

  auto *gridMenu = window->findChild<QObject *>("gridContextMenu");
  QVERIFY(gridMenu);
  const auto findItem = [](QObject *scope, const char *name) {
    return scope->findChild<QQuickItem *>(QString::fromLatin1(name));
  };
  const auto itemEnabled = [&](const char *name) {
    auto *item = findItem(gridMenu, name);
    return item && item->property("enabled").toBool();
  };
  const auto assertSelectionActionsEnabled = [&](bool expected) {
    gridMenu->setProperty("selectionCount", 1);
    QCOMPARE(itemEnabled("contextCutAction"), expected);
    QCOMPARE(itemEnabled("contextClipboardCopyAction"), expected);
    QCOMPARE(itemEnabled("contextRenameAction"), expected);
    QCOMPARE(itemEnabled("contextCopyAction"), expected);
    QCOMPARE(itemEnabled("contextMoveAction"), expected);
    QCOMPARE(itemEnabled("contextTrashAction"), expected);
  };
  const auto assertBackgroundActionsEnabled = [&](bool expected) {
    gridMenu->setProperty("selectionCount", 0);
    QCOMPARE(itemEnabled("contextNewFolderAction"), expected);
    QCOMPARE(itemEnabled("contextBackgroundPasteAction"), expected);
  };

  // A real clipboard snapshot and a real (independently-tracked) selection
  // count, so the idle baseline below is a genuine "true" that busy-gating
  // could not be accidentally taking credit for.
  QVERIFY(clipboard.copySelection({navigation.entries().at(0)}));
  QVERIFY(clipboard.canPaste());
  clipboard.setSelectionCount(1);

  assertBackgroundActionsEnabled(true);
  assertSelectionActionsEnabled(true);

  QVERIFY(mutation.createFolder(folder, QStringLiteral("Busy-Folder")));
  QVERIFY(mutation.busy());
  assertBackgroundActionsEnabled(false);
  assertSelectionActionsEnabled(false);

  QTRY_VERIFY(!mutation.busy());
  QVERIFY(QDir(folder).exists(QStringLiteral("Busy-Folder")));
  // The real Main.qml wiring (mutationCommitted -> navigation.refresh() ->
  // EntrySelection's reconciliation -> clipboardController.selectionCount)
  // legitimately drops the live selection count back to 0 once the new
  // folder's creation refreshes the real listing — correct production
  // behavior, not something this busy/idle test is about. Re-establish it so
  // this final check is exactly what it was before: proving idle re-enables
  // these actions, not re-proving an unrelated, already-correct reconciliation.
  clipboard.setSelectionCount(1);
  assertBackgroundActionsEnabled(true);
  assertSelectionActionsEnabled(true);
}

QTEST_MAIN(BrowsingUiTests)
#include "tst_browsing_ui.moc"
