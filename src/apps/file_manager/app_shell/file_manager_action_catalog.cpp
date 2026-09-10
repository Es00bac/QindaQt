// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_action_catalog.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QDBusConnection>
#include <QKeySequence>
#include <QWindow>
#include <QPointer>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QindaQt::AppShell::ActionSpec action(
    const QString &id, const QString &menuId, const QString &menuLabel,
    const QString &label, const QString &description, const QKeySequence &shortcut,
    int menuOrder, int order, bool destructive = false, bool checkable = false) {
  return {.id = id,
          .menuId = menuId,
          .menuLabel = menuLabel,
          .label = label,
          .accessibleDescription = description,
          .shortcut = shortcut,
          .menuOrder = menuOrder,
          .order = order,
          .enabled = true,
          .checkable = checkable,
          .checked = false,
          .destructive = destructive};
}

} // namespace

QList<QindaQt::AppShell::ActionSpec> fileManagerActionCatalog() {
  return {
      action(QStringLiteral("file.new-folder"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("New Folder"),
             QStringLiteral("Create a folder in the current location"),
             QKeySequence(QStringLiteral("Ctrl+Shift+N")), 0, 0),
      action(QStringLiteral("file.rename"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Rename"),
             QStringLiteral("Rename the selected item"),
             QKeySequence(QStringLiteral("F2")), 0, 1),
      action(QStringLiteral("file.copy"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Copy To…"),
             QStringLiteral("Copy the selected item to a local path"),
             QKeySequence(QStringLiteral("Ctrl+Shift+C")), 0, 2),
      action(QStringLiteral("file.move"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Move To…"),
             QStringLiteral("Move the selected item to a local path"),
             QKeySequence(QStringLiteral("Ctrl+Shift+M")), 0, 3),
      action(QStringLiteral("file.trash"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Move to Trash"),
             QStringLiteral("Move the selected item to the recoverable home Trash"),
             QKeySequence(QStringLiteral("Delete")), 0, 4, true),
      action(QStringLiteral("file.restore-last"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Restore Last Trashed Item"),
             QStringLiteral("Restore the last item moved to Trash"),
             QKeySequence(QStringLiteral("Ctrl+Shift+R")), 0, 5),
      action(QStringLiteral("file.empty-trash"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Empty Trash"),
             QStringLiteral("Permanently remove every item from the home Trash"),
             QKeySequence(QStringLiteral("Ctrl+Shift+Delete")), 0, 6, true),
      action(QStringLiteral("file.properties"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Properties"),
             QStringLiteral("Show size, kind, and permissions for the selection"),
             QKeySequence(QStringLiteral("Alt+Return")), 0, 7),
      action(QStringLiteral("edit.undo"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Undo File Operation"),
             QStringLiteral("Undo the last recoverable create, rename, or move"),
             QKeySequence::Undo, 1, 0),
      action(QStringLiteral("operation.cancel"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Cancel Operation"),
             QStringLiteral("Request cancellation of the running file operation"),
             QKeySequence(QStringLiteral("Ctrl+Escape")), 1, 1),
      // Cut/copy/paste start disabled; bindFileManagerTransferActions drives
      // their enabled state from selection and clipboard ownership.
      action(QStringLiteral("edit.cut"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Cut"),
             QStringLiteral("Move the selected items to the clipboard"),
             QKeySequence::Cut, 1, 2),
      action(QStringLiteral("edit.copy"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Copy"),
             QStringLiteral("Copy the selected items to the clipboard"),
             QKeySequence::Copy, 1, 3),
      action(QStringLiteral("edit.paste"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Paste"),
             QStringLiteral("Paste the clipboard contents into this folder"),
             QKeySequence::Paste, 1, 4),
      action(QStringLiteral("edit.select-all"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Select All"),
             QStringLiteral("Select every entry in the current folder"),
             QKeySequence(QStringLiteral("Ctrl+A")), 1, 5),
      action(QStringLiteral("view.show-hidden"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Show Hidden Files"),
             QStringLiteral("Show or hide entries whose name begins with a dot"),
             QKeySequence(QStringLiteral("Ctrl+H")), 2, 0, false, true),
      // AGENT-GUARD: Direct view choices must not be checkable Qt Actions;
      // selecting the current mode again would uncheck the unchanged view.
      action(QStringLiteral("view.grid-mode"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Icon View"),
             QStringLiteral("Show this folder as icons"),
             QKeySequence(QStringLiteral("Ctrl+2")), 2, 1),
      action(QStringLiteral("view.details-mode"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Details View"),
             QStringLiteral("Show this folder as a detailed list"),
             QKeySequence(QStringLiteral("Ctrl+1")), 2, 4),
      action(QStringLiteral("view.zoom-in"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Zoom In"),
             QStringLiteral("Increase icon size"), QKeySequence::ZoomIn, 2, 5),
      action(QStringLiteral("view.zoom-out"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Zoom Out"),
             QStringLiteral("Decrease icon size"), QKeySequence::ZoomOut, 2, 6),
      action(QStringLiteral("view.zoom-reset"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Reset Zoom"),
             QStringLiteral("Restore the default icon size"),
             QKeySequence(QStringLiteral("Ctrl+0")), 2, 7),
      action(QStringLiteral("view.filter"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Filter This Folder"),
             QStringLiteral("Filter filenames in the current folder"),
             QKeySequence(QStringLiteral("Ctrl+F")), 2, 8),
      action(QStringLiteral("view.focus-location"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Location Bar"),
             QStringLiteral("Type a folder path directly"),
             QKeySequence(QStringLiteral("Ctrl+L")), 2, 2),
      action(QStringLiteral("go.home"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Home Folder"),
             QStringLiteral("Open your home folder"),
             QKeySequence(QStringLiteral("Alt+Home")), 3, 0),
      action(QStringLiteral("go.back"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Back"),
             QStringLiteral("Return to the previous folder"),
             QKeySequence(QStringLiteral("Alt+Left")), 3, 2),
      action(QStringLiteral("go.forward"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Forward"),
             QStringLiteral("Return to the next folder"),
             QKeySequence(QStringLiteral("Alt+Right")), 3, 3),
      action(QStringLiteral("go.up"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Parent Folder"),
             QStringLiteral("Open the parent folder"),
             QKeySequence(QStringLiteral("Alt+Up")), 3, 4),
      action(QStringLiteral("view.refresh"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Refresh"),
             QStringLiteral("Read the current folder again"),
             QKeySequence(QStringLiteral("F5")), 2, 3),
      action(QStringLiteral("bookmark.add"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Bookmark Current Folder"),
             QStringLiteral("Add the current folder to the bookmarks sidebar"),
             QKeySequence(QStringLiteral("Ctrl+D")), 3, 1),
  };
}

std::unique_ptr<QObject> composeFileManagerMenuExport(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, QObject *qmlRoot) {
  auto *window = qobject_cast<QWindow *>(qmlRoot);
  if (window == nullptr) {
    return {};
  }
  // AGENT-CONTRACT: one shared first-party composition entry for File
  // Manager, Terminal, and Text Editor; its fail-closed, lifecycle, and
  // test-seam behavior is owned by src/app_shell/menu_export.
  return QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
      coordinator, *window, QDBusConnection::sessionBus(),
      [root = QPointer<QObject>(qmlRoot)](bool visible) {
        if (root) {
          root->setProperty("inWindowMenuVisible", visible);
        }
      });
}

} // namespace QindaQt::Apps::FileManager
