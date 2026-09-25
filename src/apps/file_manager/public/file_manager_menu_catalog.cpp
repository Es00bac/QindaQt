// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_menu_catalog.h"

namespace QindaQt::Apps::FileManager::MenuCatalog {
namespace {

[[nodiscard]] ShortcutDefinition keys(const char *text)
{
  return {.standardKey = std::nullopt, .text = QString::fromLatin1(text)};
}

[[nodiscard]] ShortcutDefinition standard(QKeySequence::StandardKey key)
{
  return {.standardKey = key, .text = {}};
}

[[nodiscard]] ActionDefinition action(const char *id, const char *menuId,
                                      const QString &label,
                                      const QString &description,
                                      ShortcutDefinition shortcut, int order,
                                      bool destructive = false,
                                      bool checkable = false)
{
  return {.id = QString::fromLatin1(id),
          .menuId = QString::fromLatin1(menuId),
          .label = label,
          .description = description,
          .shortcut = std::move(shortcut),
          .order = order,
          .destructive = destructive,
          .checkable = checkable};
}

// ADR-0270: the View menu's folder-view entries, kept apart so
// windowActions() stays within the function-length budget.
[[nodiscard]] QList<ActionDefinition> folderViewActions()
{
  return {
      // Finder's other two views take the next two numbers; the
      // two this catalog always had keep theirs (Ctrl+1 Details, Ctrl+2 Icons).
      action("view.columns-mode", "view", QStringLiteral("Columns View"),
             QStringLiteral("Show this folder as columns, one for each folder level"),
             keys("Ctrl+3"), 4),
      action("view.gallery-mode", "view", QStringLiteral("Gallery View"),
             QStringLiteral("Show a large preview above a strip of this folder's items"),
             keys("Ctrl+4"), 4),
      // The Details column chooser, per-folder views and Group By.
      // Like the sort choices these are commands, never checkable Actions.
      action("view.show-columns", "view", QStringLiteral("Show Columns…"),
             QStringLiteral("Choose and order the Details view's columns"), keys("Ctrl+J"), 14),
      action("view.use-as-defaults", "view", QStringLiteral("Use as Defaults"),
             QStringLiteral("Make this folder's view the one every other folder starts with"),
             keys("Ctrl+Shift+J"), 15),
      action("view.group-none", "view", QStringLiteral("Don't Group"),
             QStringLiteral("Show this folder without groups"), keys("Ctrl+Alt+0"), 16),
      action("view.group-kind", "view", QStringLiteral("Group by Kind"),
             QStringLiteral("Group this folder's items by kind"), keys("Ctrl+Alt+5"), 17),
      action("view.group-date", "view", QStringLiteral("Group by Date Modified"),
             QStringLiteral("Group this folder's items by when they last changed"),
             keys("Ctrl+Alt+6"), 18),
      action("view.group-size", "view", QStringLiteral("Group by Size"),
             QStringLiteral("Group this folder's items by size"), keys("Ctrl+Alt+7"), 19),
  };
}

} // namespace

QString applicationTitle()
{
  return QStringLiteral("File Manager");
}

QString desktopEntryId()
{
  return QStringLiteral("org.qindaqt.FileManager");
}

QString applicationIconName()
{
  return QStringLiteral("org.qindaqt.FileManager");
}

QList<MenuDefinition> menus()
{
  return {{QStringLiteral("file"), QStringLiteral("File"), 0},
          {QStringLiteral("edit"), QStringLiteral("Edit"), 1},
          {QStringLiteral("view"), QStringLiteral("View"), 2},
          {QStringLiteral("go"), QStringLiteral("Go"), 3}};
}

std::optional<MenuDefinition> findMenu(QStringView id)
{
  for (const MenuDefinition &menu : menus()) {
    if (menu.id == id) {
      return menu;
    }
  }
  return std::nullopt;
}

QList<ActionDefinition> windowActions()
{
  // AGENT-NOTE: this order is File Manager's historical catalog order; the
  // AppShell registry sorts by menu and `order`, so reordering here changes
  // nothing visible, but keeping it makes review diffs line up.
  QList<ActionDefinition> actions{
      // ADR-0269: Open is one action for files, folders and (ADR-0262)
      // Applications rows alike; it replaced "application.open". The File
      // menu's orders are spaced in groups of ten -- open, create, change,
      // archive, trash, info -- so an entry can join its group later without
      // renumbering the others.
      action("file.open", "file", QStringLiteral("Open"),
             QStringLiteral("Open the selected items"), keys("Ctrl+O"), 0),
      action("file.open-with", "file", QStringLiteral("Open With…"),
             QStringLiteral("Choose an application to open the selected files with"),
             keys("Ctrl+Shift+O"), 1),
      action("file.open-new-window", "file", QStringLiteral("Open in New Window"),
             QStringLiteral("Open the selected folder in a new window"),
             keys("Ctrl+Alt+O"), 2),
      // ADR-0262: Show Desktop Entry File sits with Get Info and stays
      // disabled outside the Applications place (file_manager_application_actions).
      action("application.show-entry-file", "file", QStringLiteral("Show Desktop Entry File"),
             QStringLiteral("Show the selected application's desktop entry in its folder"),
             keys("Ctrl+Shift+E"), 51),
      action("file.new-folder", "file", QStringLiteral("New Folder"),
             QStringLiteral("Create a folder in the current location"),
             keys("Ctrl+Shift+N"), 10),
      action("file.new-file", "file", QStringLiteral("New File…"),
             QStringLiteral("Create a file in this folder, empty or from a template"),
             keys("Ctrl+Alt+N"), 11),
      action("file.open-terminal", "file", QStringLiteral("Open Terminal Here"),
             QStringLiteral("Open a terminal in this folder"), keys("Shift+F4"), 12),
      action("file.rename", "file", QStringLiteral("Rename"),
             QStringLiteral("Rename the selected item"), keys("F2"), 20),
      action("file.duplicate", "file", QStringLiteral("Duplicate"),
             QStringLiteral("Make a copy of each selected item beside it"),
             keys("Ctrl+Shift+D"), 21),
      action("file.make-link", "file", QStringLiteral("Make Link"),
             QStringLiteral("Make a link to each selected item beside it"), keys("Ctrl+M"), 22),
      action("file.copy", "file", QStringLiteral("Copy To…"),
             QStringLiteral("Copy the selected item to a local path"),
             keys("Ctrl+Shift+C"), 23),
      action("file.move", "file", QStringLiteral("Move To…"),
             QStringLiteral("Move the selected item to a local path"),
             keys("Ctrl+Shift+M"), 24),
      action("file.compress", "file", QStringLiteral("Compress"),
             QStringLiteral("Compress the selected items into a zip archive"),
             keys("Ctrl+Shift+K"), 30),
      action("file.extract", "file", QStringLiteral("Extract"),
             QStringLiteral("Extract each selected archive into a new folder"),
             keys("Ctrl+Shift+X"), 31),
      action("file.add-to-sidebar", "file", QStringLiteral("Add to Sidebar"),
             QStringLiteral("Add the selected folders to the sidebar"),
             keys("Ctrl+Shift+B"), 32),
      action("file.trash", "file", QStringLiteral("Move to Trash"),
             QStringLiteral("Move the selected item to the recoverable home Trash"),
             keys("Delete"), 40, true),
      // ADR-0269: never skips its confirmation, whatever Preferences say.
      action("file.delete", "file", QStringLiteral("Delete Permanently"),
             QStringLiteral("Delete the selected items at once, without Trash"),
             keys("Shift+Delete"), 41, true),
      action("file.put-back", "file", QStringLiteral("Put Back"),
             QStringLiteral("Return the selected Trash items to where they came from"),
             keys("Ctrl+Backspace"), 42),
      action("file.restore-last", "file", QStringLiteral("Restore Last Trashed Item"),
             QStringLiteral("Restore the last item moved to Trash"),
             keys("Ctrl+Shift+R"), 43),
      action("file.empty-trash", "file", QStringLiteral("Empty Trash"),
             QStringLiteral("Permanently remove every item from the home Trash"),
             keys("Ctrl+Shift+Delete"), 44, true),
      // ADR-0198: Preferences is a File-menu item with the platform-standard
      // Ctrl+, shortcut, after the item-specific entries.
      action("app.preferences", "file", QStringLiteral("Preferences"),
             QStringLiteral("Change what a window starts with, and network options"),
             keys("Ctrl+,"), 60),
      // ADR-0269: Finder's name; with nothing selected it describes the folder.
      action("file.properties", "file", QStringLiteral("Get Info"),
             QStringLiteral("Show size, kind, and permissions for the selection or this folder"),
             keys("Alt+Return"), 50),
      action("edit.undo", "edit", QStringLiteral("Undo File Operation"),
             QStringLiteral("Undo the last recoverable create, rename, or move"),
             standard(QKeySequence::Undo), 0),
      action("operation.cancel", "edit", QStringLiteral("Cancel Operation"),
             QStringLiteral("Request cancellation of the running file operation"),
             keys("Ctrl+Escape"), 1),
      // Cut/copy/paste start disabled in a window; its transfer binding drives
      // their enabled state from selection and clipboard ownership.
      action("edit.cut", "edit", QStringLiteral("Cut"),
             QStringLiteral("Move the selected items to the clipboard"),
             standard(QKeySequence::Cut), 2),
      action("edit.copy", "edit", QStringLiteral("Copy"),
             QStringLiteral("Copy the selected items to the clipboard"),
             standard(QKeySequence::Copy), 3),
      // ADR-0269: shares Copy's order and sorts right after it by id.
      action("edit.copy-path", "edit", QStringLiteral("Copy Path"),
             QStringLiteral("Copy the full paths of the selected items as text"),
             keys("Ctrl+Alt+C"), 3),
      action("edit.paste", "edit", QStringLiteral("Paste"),
             QStringLiteral("Paste the clipboard contents into this folder"),
             standard(QKeySequence::Paste), 4),
      action("edit.select-all", "edit", QStringLiteral("Select All"),
             QStringLiteral("Select every entry in the current folder"),
             keys("Ctrl+A"), 5),
      action("view.show-hidden", "view", QStringLiteral("Show Hidden Files"),
             QStringLiteral("Show or hide entries whose name begins with a dot"),
             keys("Ctrl+H"), 0, false, true),
      // AGENT-GUARD: Direct view choices must not be checkable Qt Actions;
      // selecting the current mode again would uncheck the unchanged view.
      action("view.grid-mode", "view", QStringLiteral("Icon View"),
             QStringLiteral("Show this folder as icons"), keys("Ctrl+2"), 1),
      action("view.details-mode", "view", QStringLiteral("Details View"),
             QStringLiteral("Show this folder as a detailed list"), keys("Ctrl+1"), 4),
      action("view.zoom-in", "view", QStringLiteral("Zoom In"),
             QStringLiteral("Increase icon size"), standard(QKeySequence::ZoomIn), 5),
      action("view.zoom-out", "view", QStringLiteral("Zoom Out"),
             QStringLiteral("Decrease icon size"), standard(QKeySequence::ZoomOut), 6),
      action("view.zoom-reset", "view", QStringLiteral("Reset Zoom"),
             QStringLiteral("Restore the default icon size"), keys("Ctrl+0"), 7),
      action("view.filter", "view", QStringLiteral("Filter This Folder"),
             QStringLiteral("Filter filenames in the current folder"), keys("Ctrl+F"), 8),
      action("view.group-by-category", "view", QStringLiteral("Group by Category"),
             QStringLiteral("Group applications under their categories"), keys("Ctrl+G"), 9,
             false, true),
      // ADR-0269: Sort By. Choosing the active sort again reverses it, as a
      // column header does; like the view choices these are not checkable.
      action("view.sort-name", "view", QStringLiteral("Sort by Name"),
             QStringLiteral("Sort this folder by name"), keys("Ctrl+Alt+1"), 10),
      action("view.sort-size", "view", QStringLiteral("Sort by Size"),
             QStringLiteral("Sort this folder by size"), keys("Ctrl+Alt+2"), 11),
      action("view.sort-kind", "view", QStringLiteral("Sort by Kind"),
             QStringLiteral("Sort this folder by kind"), keys("Ctrl+Alt+3"), 12),
      action("view.sort-modified", "view", QStringLiteral("Sort by Date Modified"),
             QStringLiteral("Sort this folder by modification date"), keys("Ctrl+Alt+4"), 13),
      action("view.focus-location", "view", QStringLiteral("Location Bar"),
             QStringLiteral("Type a folder path directly"), keys("Ctrl+L"), 2),
      action("go.home", "go", QStringLiteral("Home Folder"),
             QStringLiteral("Open your home folder"), keys("Alt+Home"), 0),
      action("go.back", "go", QStringLiteral("Back"),
             QStringLiteral("Return to the previous folder"), keys("Alt+Left"), 2),
      action("go.forward", "go", QStringLiteral("Forward"),
             QStringLiteral("Return to the next folder"), keys("Alt+Right"), 3),
      action("go.up", "go", QStringLiteral("Parent Folder"),
             QStringLiteral("Open the parent folder"), keys("Alt+Up"), 4),
      action("go.applications", "go", QStringLiteral("Applications"),
             QStringLiteral("Show every installed application"),
             keys("Ctrl+Shift+A"), 5),
      // ADR-0194: the Go menu's network pair. "go.network" opens the hub of
      // saved locations; "network.connect" opens the hub with the
      // Connect-to-server dialog already up. Alt+N joins the Go menu's other
      // Alt shortcuts; Ctrl+Shift+S is free in this catalog.
      action("go.network", "go", QStringLiteral("Network"),
             QStringLiteral("Show saved network locations"), keys("Alt+N"), 6),
      action("network.connect", "go", QStringLiteral("Connect to Server…"),
             QStringLiteral("Save a network folder and open it"),
             keys("Ctrl+Shift+S"), 7),
      action("view.refresh", "view", QStringLiteral("Refresh"),
             QStringLiteral("Read the current folder again"), keys("F5"), 3),
      action("bookmark.add", "go", QStringLiteral("Bookmark Current Folder"),
             QStringLiteral("Add the current folder to the bookmarks sidebar"),
             keys("Ctrl+D"), 1),
      // ADR-0273: Keep in Dock for the one selected application, beside Get
      // Info; checked while the dock holds it, disabled outside the
      // Applications place (file_manager_dock_actions).
      action("application.keep-in-dock", "file", QStringLiteral("Keep in Dock"),
             QStringLiteral("Keep the selected application in the dock"), keys("Ctrl+Alt+D"),
             51, false, true),
  };
  actions.append(folderViewActions());
  return actions;
}

QList<ActionDefinition> applicationActions()
{
  // AGENT-NOTE: no shortcut on purpose. A shortcut here would promise a key
  // that neither a File Manager window nor the focus-less desktop handles
  // (ADR-0260); add one only together with the handler that honours it.
  return {
      action("file.new-window", "file", QStringLiteral("New File Manager Window"),
             QStringLiteral("Open a new File Manager window at its start folder"),
             ShortcutDefinition{}, 0),
  };
}

std::optional<ActionDefinition> findAction(QStringView id)
{
  for (const QList<ActionDefinition> &list : {windowActions(), applicationActions()}) {
    for (const ActionDefinition &definition : list) {
      if (definition.id == id) {
        return definition;
      }
    }
  }
  return std::nullopt;
}

QKeySequence shortcutSequence(const ShortcutDefinition &shortcut)
{
  if (shortcut.standardKey.has_value()) {
    return QKeySequence(*shortcut.standardKey);
  }
  // Same constructor File Manager always used, so its bindings are unchanged.
  return QKeySequence(shortcut.text);
}

} // namespace QindaQt::Apps::FileManager::MenuCatalog
