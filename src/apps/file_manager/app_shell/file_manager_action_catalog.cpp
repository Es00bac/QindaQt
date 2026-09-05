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
    int menuOrder, int order, bool destructive = false) {
  return {.id = id,
          .menuId = menuId,
          .menuLabel = menuLabel,
          .label = label,
          .accessibleDescription = description,
          .shortcut = shortcut,
          .menuOrder = menuOrder,
          .order = order,
          .enabled = true,
          .checkable = false,
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
      action(QStringLiteral("edit.undo"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Undo File Operation"),
             QStringLiteral("Undo the last recoverable create, rename, or move"),
             QKeySequence::Undo, 1, 0),
      action(QStringLiteral("operation.cancel"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Cancel Operation"),
             QStringLiteral("Request cancellation of the running file operation"),
             QKeySequence(QStringLiteral("Ctrl+Escape")), 1, 1),
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
