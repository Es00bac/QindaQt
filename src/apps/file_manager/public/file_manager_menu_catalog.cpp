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
  return {
      // ADR-0262: the Applications place's item actions. Open heads the File
      // menu; Show Desktop Entry File sits with Properties (Get Info there).
      // Both stay disabled outside that place (file_manager_application_actions).
      action("application.open", "file", QStringLiteral("Open"),
             QStringLiteral("Open the selected applications"), keys("Ctrl+O"), -1),
      action("application.show-entry-file", "file", QStringLiteral("Show Desktop Entry File"),
             QStringLiteral("Show the selected application's desktop entry in its folder"),
             keys("Ctrl+Shift+E"), 7),
      action("file.new-folder", "file", QStringLiteral("New Folder"),
             QStringLiteral("Create a folder in the current location"),
             keys("Ctrl+Shift+N"), 0),
      action("file.rename", "file", QStringLiteral("Rename"),
             QStringLiteral("Rename the selected item"), keys("F2"), 1),
      action("file.copy", "file", QStringLiteral("Copy To…"),
             QStringLiteral("Copy the selected item to a local path"),
             keys("Ctrl+Shift+C"), 2),
      action("file.move", "file", QStringLiteral("Move To…"),
             QStringLiteral("Move the selected item to a local path"),
             keys("Ctrl+Shift+M"), 3),
      action("file.trash", "file", QStringLiteral("Move to Trash"),
             QStringLiteral("Move the selected item to the recoverable home Trash"),
             keys("Delete"), 4, true),
      action("file.restore-last", "file", QStringLiteral("Restore Last Trashed Item"),
             QStringLiteral("Restore the last item moved to Trash"),
             keys("Ctrl+Shift+R"), 5),
      action("file.empty-trash", "file", QStringLiteral("Empty Trash"),
             QStringLiteral("Permanently remove every item from the home Trash"),
             keys("Ctrl+Shift+Delete"), 6, true),
      // ADR-0198: Preferences is a File-menu item with the platform-standard
      // Ctrl+, shortcut, after the item-specific entries.
      action("app.preferences", "file", QStringLiteral("Preferences"),
             QStringLiteral("Change what a window starts with, and network options"),
             keys("Ctrl+,"), 8),
      action("file.properties", "file", QStringLiteral("Properties"),
             QStringLiteral("Show size, kind, and permissions for the selection"),
             keys("Alt+Return"), 7),
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
             7, false, true),
  };
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
