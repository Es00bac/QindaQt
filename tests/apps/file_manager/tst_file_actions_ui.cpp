// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_application_actions.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_item_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/clipboard_controller.h"
#include "model/entry_properties.h"
#include "model/file_templates.h"
#include "model/folder_launch_controller.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/open_with_controller.h"
#include "model/places_controller.h"
#include "model/search_controller.h"
#include "mutation/karchive_codec.h"
#include "mutation/local_mutation_backend.h"
#include "mutation/mutation_controller.h"
#include "preview/local_preview.h"
#include "preview/preview_provider.h"
#include "preview/theme_icon_provider.h"
#include "window_fixtures.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

using namespace QindaQt::Apps::FileManager;

// ADR-0269: the right-click set in the production window -- which items the
// context menu offers for what is selected, and that each one runs through
// the AppShell catalog to its controller. Nothing is spawned: program starts
// are recorded, and every file lives in a temporary directory.

namespace {

using Start = std::pair<QString, QStringList>;

// main.cpp's composition over temporary storage: every action binder, the
// real coordinator, the production backend with the KArchive codec, and the
// three right-click owners with a recording starter.
struct FileActionsWindow final {
  explicit FileActionsWindow(const QString &root)
      : trashFiles(root + QStringLiteral("/data/Trash/files")),
        terminal(root + QStringLiteral("/bin/qqterm")),
        fileManager(root + QStringLiteral("/bin/qindaqt-file-manager")),
        applications(QStringList{}),
        mutation(std::make_unique<LocalMutationBackend>(
            root + QStringLiteral("/data/Trash"), std::make_shared<LocalDeviceResolver>(),
            std::make_shared<KArchiveCodec>())),
        clipboard(mutation, *QGuiApplication::clipboard()),
        places(std::make_unique<BookmarksStore>(root + QStringLiteral("/state"))),
        support(root),
        openWith([] { return QindaQt::ApplicationCatalog::DirectoryScan{}; },
                 [] { return ListingResult{}; },
                 std::make_unique<QindaQt::Apps::SettingsDefaultApps::MimeAppsDefaultApplicationsStore>(
                     root + QStringLiteral("/mimeapps.list"),
                     QStringList{root + QStringLiteral("/mimeapps.list")},
                     QindaQt::ApplicationCatalog::DirectoryScan{}),
                 OpenWithLauncher(starter(), {})),
        folders(starter(), {terminal}, {fileManager}),
        templates(root + QStringLiteral("/Templates")),
        navigation(std::make_unique<LocalDirectoryLister>(),
                   std::make_unique<Test::FakeFileLauncher>()) {
    catalogInstalled = coordinator.replaceActions(fileManagerActionCatalog()).ok();
    bindFileManagerBrowsingActions(coordinator, navigation);
    bindFileManagerTransferActions(coordinator, navigation, clipboard, mutation);
    bindFileManagerMutationActions(coordinator, navigation, mutation);
    bindFileManagerApplicationActions(coordinator, navigation, clipboard);
    bindFileManagerItemActions(coordinator, navigation, clipboard, mutation, trashFiles);
    navigation.navigateTo(root + QStringLiteral("/work"));

    engine.addImageProvider(QStringLiteral("previews"),
                            new PreviewProvider(std::make_unique<LocalPreviewDecoder>()));
    engine.addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
    const auto object = [](QObject *value) { return QVariant::fromValue(value); };
    QVariantMap initial{{"navigationController", object(&navigation)},
                        {"mutationController", object(&mutation)},
                        {"clipboardController", object(&clipboard)},
                        {"propertiesController", object(&properties)},
                        {"searchController", object(&search)},
                        {"placesController", object(&places)},
                        {"applicationsController", object(&applications)},
                        {"openWithController", object(&openWith)},
                        {"folderLaunchController", object(&folders)},
                        {"fileTemplates", object(&templates)},
                        {"coordinator", object(&coordinator)}};
    support.insertInto(initial);
    engine.setInitialProperties(initial);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
                                    + QStringLiteral("/src/apps/file_manager/ui/Main.qml")));
    window = engine.rootObjects().isEmpty()
        ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  }

  ~FileActionsWindow() {
    // Tear QML down while the injected controllers are still alive.
    const auto roots = engine.rootObjects();
    for (QObject *root : roots) {
      delete root;
    }
  }

  [[nodiscard]] DetachedStarter starter() {
    return [this](const QString &program, const QStringList &arguments) {
      started.append({program, arguments});
      return true;
    };
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

  // Selects exactly the entry called `name`, as a click would.
  [[nodiscard]] bool select(const QString &name) {
    auto *selection = window->findChild<QObject *>(QStringLiteral("entrySelection"));
    const int index = navigation.indexOfName(name);
    return selection != nullptr && index >= 0 &&
        QMetaObject::invokeMethod(selection, "selectOnly", Q_ARG(QVariant, index));
  }

  [[nodiscard]] bool clearSelection() {
    auto *selection = window->findChild<QObject *>(QStringLiteral("entrySelection"));
    return selection != nullptr &&
        QMetaObject::invokeMethod(selection, "applyIndexSet", Q_ARG(QVariant, QVariantList{}),
                                  Q_ARG(QVariant, 0));
  }

  // Opens the visible view's context menu for `selectionCount` entries
  // (0: the background), as the views do before popup().
  [[nodiscard]] QObject *popupMenu(int selectionCount) {
    auto *menu = window->findChild<QObject *>(
        navigation.viewMode() == QLatin1String("grid") ? QStringLiteral("gridContextMenu")
                                                       : QStringLiteral("listContextMenu"));
    if (menu != nullptr) {
      menu->setProperty("selectionCount", selectionCount);
      QMetaObject::invokeMethod(menu, "popup");
    }
    return menu;
  }

  QString trashFiles;
  QString terminal;
  QString fileManager;
  QVector<Start> started;
  ApplicationsController applications;
  MutationController mutation;
  ClipboardController clipboard;
  EntryPropertiesController properties;
  SearchController search;
  PlacesController places;
  Test::WindowSupportControllers support;
  OpenWithController openWith;
  FolderLaunchController folders;
  FileTemplates templates;
  NavigationController navigation;
  QindaQt::AppShell::ApplicationCoordinator coordinator;
  bool catalogInstalled = false;
  QQmlApplicationEngine engine;
  QQuickWindow *window = nullptr;
};

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &data = "payload") {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

[[nodiscard]] bool makeExecutable(const QString &path) {
  return writeFile(path, "#!/bin/false\n") &&
      QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                      QFileDevice::ExeOwner);
}

[[nodiscard]] bool shown(QObject *menu, const char *name) {
  auto *item = menu->findChild<QQuickItem *>(QString::fromLatin1(name));
  return item != nullptr && item->isVisible();
}

[[nodiscard]] bool enabledItem(QObject *menu, const char *name) {
  auto *item = menu->findChild<QQuickItem *>(QString::fromLatin1(name));
  return item != nullptr && item->isEnabled();
}

} // namespace

class FileActionsUiTest final : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void itemMenuFollowsTheSelection();
  void backgroundMenuOffersTheFolderActions();
  void trashOffersPutBackInsteadOfMoveToTrash();
  void rightClickSetRunsThroughTheCatalog();

private:
  [[nodiscard]] QString path(const QString &relative) const {
    return m_root + QLatin1Char('/') + relative;
  }

  std::unique_ptr<QTemporaryDir> m_temporary;
  QString m_root;
  std::unique_ptr<FileActionsWindow> m_window;
};

void FileActionsUiTest::init() {
  m_temporary = std::make_unique<QTemporaryDir>();
  QVERIFY(m_temporary->isValid());
  m_root = QDir(m_temporary->path()).canonicalPath();
  // The Trash place follows $XDG_DATA_HOME; keep it inside this row.
  qputenv("XDG_DATA_HOME", QFile::encodeName(path(QStringLiteral("data"))));
  for (const char *folder : {"work/photos", "Templates", "bin", "data/Trash/files"}) {
    QVERIFY(QDir().mkpath(path(QString::fromLatin1(folder))));
  }
  QVERIFY(writeFile(path(QStringLiteral("work/notes.txt")), "notes"));
  QVERIFY(writeFile(path(QStringLiteral("work/bundle.zip")), "not really a zip"));
  QVERIFY(writeFile(path(QStringLiteral("Templates/Letter.odt")), "letter"));
  QVERIFY(writeFile(path(QStringLiteral("Templates/.hidden")), "hidden"));
  QVERIFY(makeExecutable(path(QStringLiteral("bin/qqterm"))));
  QVERIFY(makeExecutable(path(QStringLiteral("bin/qindaqt-file-manager"))));
  m_window = std::make_unique<FileActionsWindow>(m_root);
  QVERIFY(m_window->catalogInstalled);
  QVERIFY(m_window->window);
  m_window->window->requestActivate();
  QTRY_VERIFY(m_window->window->isExposed());
  QTRY_COMPARE(m_window->navigation.entryCount(), 3);
}

void FileActionsUiTest::cleanup() {
  m_window.reset();
  m_temporary.reset();
  qunsetenv("XDG_DATA_HOME");
}

void FileActionsUiTest::itemMenuFollowsTheSelection() {
  auto &w = *m_window;
  // A file: open it, with something else, or act on it.
  QVERIFY(w.select(QStringLiteral("notes.txt")));
  QObject *menu = w.popupMenu(1);
  QVERIFY(menu);
  QTRY_VERIFY(menu->property("visible").toBool());
  for (const char *name : {"contextOpenAction", "contextOpenWithMenu", "contextCopyPathAction",
                           "contextDuplicateAction", "contextMakeLinkAction",
                           "contextCompressAction", "contextTrashAction", "contextDeleteAction",
                           "contextPropertiesAction", "contextRenameAction"}) {
    QVERIFY2(shown(menu, name), name);
  }
  for (const char *name : {"contextOpenNewWindowAction", "contextExtractAction",
                           "contextAddToSidebarAction", "contextPutBackAction",
                           "contextNewFileMenu", "contextOpenTerminalAction",
                           "contextFolderInfoAction", "contextSortMenu", "contextViewMenu",
                           "contextShowEntryFileAction"}) {
    QVERIFY2(!shown(menu, name), name);
  }
  QVERIFY(enabledItem(menu, "contextDuplicateAction"));
  QVERIFY(enabledItem(menu, "contextDeleteAction"));
  // No handler is installed in this row, so only Other Application... is left.
  QVERIFY(enabledItem(menu, "contextOpenWithOtherAction"));
  QMetaObject::invokeMethod(menu, "close");
  QTRY_VERIFY(!menu->property("visible").toBool());

  // A folder: a window of its own, or a place in the sidebar.
  QVERIFY(w.select(QStringLiteral("photos")));
  menu = w.popupMenu(1);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(shown(menu, "contextOpenNewWindowAction"));
  QVERIFY(shown(menu, "contextAddToSidebarAction"));
  QVERIFY(!shown(menu, "contextOpenWithMenu"));
  QVERIFY(!shown(menu, "contextExtractAction"));
  QMetaObject::invokeMethod(menu, "close");
  QTRY_VERIFY(!menu->property("visible").toBool());

  // An archive (recognised by name): Extract joins the menu.
  QVERIFY(w.select(QStringLiteral("bundle.zip")));
  menu = w.popupMenu(1);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(shown(menu, "contextExtractAction"));
  QVERIFY(shown(menu, "contextOpenWithMenu"));
  QVERIFY(!shown(menu, "contextOpenNewWindowAction"));
  QMetaObject::invokeMethod(menu, "close");
  QTRY_VERIFY(!menu->property("visible").toBool());
}

void FileActionsUiTest::backgroundMenuOffersTheFolderActions() {
  auto &w = *m_window;
  QObject *menu = w.popupMenu(0);
  QVERIFY(menu);
  QTRY_VERIFY(menu->property("visible").toBool());
  for (const char *name : {"contextNewFolderAction", "contextNewFileMenu",
                           "contextOpenTerminalAction", "contextSelectAllAction",
                           "contextSortMenu", "contextViewMenu", "contextShowHiddenAction",
                           "contextRefreshAction", "contextFolderInfoAction"}) {
    QVERIFY2(shown(menu, name), name);
  }
  for (const char *name : {"contextOpenAction", "contextOpenWithMenu", "contextDeleteAction",
                           "contextCopyPathAction", "contextPutBackAction"}) {
    QVERIFY2(!shown(menu, name), name);
  }
  // New File offers an empty file and each visible template.
  auto *newFile = menu->findChild<QObject *>(QStringLiteral("contextNewFileSubMenu"));
  QVERIFY(newFile);
  QTRY_COMPARE(newFile->property("count").toInt(), 3);
  QCOMPARE(w.templates.templates().size(), 1);
  // Sort By and View show the current choice.
  auto *byName = menu->findChild<QQuickItem *>(QStringLiteral("contextSortNameAction"));
  QVERIFY(byName && byName->property("checked").toBool());

  // Get Info on the background describes the folder being browsed.
  auto *folderInfo = menu->findChild<QQuickItem *>(QStringLiteral("contextFolderInfoAction"));
  QVERIFY(folderInfo);
  QMetaObject::invokeMethod(folderInfo, "triggered");
  auto *dialog = w.window->findChild<QObject *>(QStringLiteral("propertiesDialog"));
  QVERIFY(dialog);
  QTRY_VERIFY(dialog->property("visible").toBool());
  QCOMPARE(w.properties.name(), QStringLiteral("work"));
  QCOMPARE(w.properties.kindText(), QStringLiteral("Folder"));
  QMetaObject::invokeMethod(dialog, "close");
  QTRY_VERIFY(!dialog->property("visible").toBool());
}

void FileActionsUiTest::trashOffersPutBackInsteadOfMoveToTrash() {
  auto &w = *m_window;
  const QString notes = path(QStringLiteral("work/notes.txt"));
  const auto identity = LocalMutationBackend::identityForPath(notes);
  QVERIFY(identity);
  QVERIFY(w.mutation.trashItem(
      notes, {{QStringLiteral("device"), QString::number(identity->device)},
              {QStringLiteral("inode"), QString::number(identity->inode)},
              {QStringLiteral("identitySize"), QString::number(identity->size)},
              {QStringLiteral("modifiedNanoseconds"), QString::number(identity->modifiedNanoseconds)},
              {QStringLiteral("mode"), QString::number(identity->mode)}}));
  QTRY_VERIFY_WITH_TIMEOUT(!w.mutation.busy(), 10000);
  QCOMPARE(w.mutation.failureCode(), QStringLiteral("none"));

  w.navigation.navigateTo(w.trashFiles);
  QTRY_COMPARE(w.navigation.entryCount(), 1);
  QVERIFY(w.select(QStringLiteral("notes.txt")));
  QTRY_VERIFY(w.actionEnabled(QStringLiteral("file.put-back")));
  QVERIFY(!w.actionEnabled(QStringLiteral("file.duplicate")));
  QVERIFY(!w.actionEnabled(QStringLiteral("file.new-file")));
  QVERIFY(w.actionEnabled(QStringLiteral("file.delete")));

  QObject *menu = w.popupMenu(1);
  QTRY_VERIFY(menu->property("visible").toBool());
  QVERIFY(shown(menu, "contextPutBackAction"));
  QVERIFY(enabledItem(menu, "contextPutBackAction"));
  QVERIFY(shown(menu, "contextDeleteAction"));
  for (const char *name : {"contextTrashAction", "contextDuplicateAction", "contextMakeLinkAction",
                           "contextCompressAction"}) {
    QVERIFY2(!shown(menu, name), name);
  }
  QMetaObject::invokeMethod(menu, "close");
  QTRY_VERIFY(!menu->property("visible").toBool());

  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.put-back")));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(notes), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!w.mutation.busy(), 10000);
  QVERIFY(!QFileInfo::exists(w.trashFiles + QStringLiteral("/notes.txt")));
}

void FileActionsUiTest::rightClickSetRunsThroughTheCatalog() {
  auto &w = *m_window;
  const QString work = path(QStringLiteral("work"));
  QVERIFY(!w.actionEnabled(QStringLiteral("file.open")));
  QVERIFY(w.actionEnabled(QStringLiteral("file.new-file")));
  QVERIFY(w.actionEnabled(QStringLiteral("file.open-terminal")));

  // Duplicate and Copy Path act on the selected file.
  QVERIFY(w.select(QStringLiteral("notes.txt")));
  QTRY_VERIFY(w.actionEnabled(QStringLiteral("file.duplicate")));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.duplicate")));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(work + QStringLiteral("/notes copy.txt")), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!w.mutation.busy(), 10000);
  QVERIFY(w.coordinator.activateAction(QStringLiteral("edit.copy-path")));
  QTRY_COMPARE(QGuiApplication::clipboard()->text(), work + QStringLiteral("/notes.txt"));

  // Delete Permanently always asks first.
  QTRY_VERIFY(w.select(QStringLiteral("notes copy.txt")));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.delete")));
  auto *confirm = w.window->findChild<QObject *>(QStringLiteral("deleteConfirmationDialog"));
  QVERIFY(confirm);
  QTRY_VERIFY(confirm->property("visible").toBool());
  QVERIFY(QFileInfo::exists(work + QStringLiteral("/notes copy.txt")));
  QVERIFY(QMetaObject::invokeMethod(confirm, "accept", Qt::DirectConnection));
  QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(work + QStringLiteral("/notes copy.txt")), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!w.mutation.busy(), 10000);

  // A folder opens in a new File Manager window and joins the sidebar.
  QTRY_VERIFY(w.select(QStringLiteral("photos")));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.open-new-window")));
  QCOMPARE(w.started.size(), 1);
  QCOMPARE(w.started.last(), Start(w.fileManager, {work + QStringLiteral("/photos")}));
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.add-to-sidebar")));
  bool bookmarked = false;
  for (const Bookmark &bookmark : w.places.bookmarkValues()) {
    bookmarked = bookmarked || bookmark.path == work + QStringLiteral("/photos");
  }
  QVERIFY(bookmarked);

  // Open Terminal Here starts the desktop terminal in the folder, as argv.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.open-terminal")));
  QCOMPARE(w.started.last(),
           Start(w.terminal, {QStringLiteral("--working-directory"), work}));

  // Sort By: choosing the active sort again reverses it.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.sort-size")));
  QCOMPARE(w.navigation.sortColumn(), QStringLiteral("size"));
  const QString direction = w.navigation.sortDirection();
  QVERIFY(w.coordinator.activateAction(QStringLiteral("view.sort-size")));
  QVERIFY(w.navigation.sortDirection() != direction);

  // New File asks for a name, then creates the empty file.
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.new-file")));
  auto *newFile = w.window->findChild<QObject *>(QStringLiteral("newFileDialog"));
  auto *nameField = w.window->findChild<QObject *>(QStringLiteral("newFileNameField"));
  QVERIFY(newFile && nameField);
  QTRY_VERIFY(newFile->property("visible").toBool());
  QVERIFY(nameField->setProperty("text", QStringLiteral("Empty.txt")));
  QVERIFY(QMetaObject::invokeMethod(newFile, "accept", Qt::DirectConnection));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(work + QStringLiteral("/Empty.txt")), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!w.mutation.busy(), 10000);
  QCOMPARE(QFileInfo(work + QStringLiteral("/Empty.txt")).size(), qint64(0));

  // Get Info with nothing selected describes the folder itself.
  QVERIFY(w.clearSelection());
  QTRY_COMPARE(w.clipboard.selectionCount(), 0);
  QVERIFY(w.coordinator.activateAction(QStringLiteral("file.properties")));
  auto *info = w.window->findChild<QObject *>(QStringLiteral("propertiesDialog"));
  QVERIFY(info);
  QTRY_VERIFY(info->property("visible").toBool());
  QCOMPARE(w.properties.name(), QStringLiteral("work"));
  QMetaObject::invokeMethod(info, "close");
}

QTEST_MAIN(FileActionsUiTest)
#include "tst_file_actions_ui.moc"
