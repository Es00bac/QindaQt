// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_action_catalog.h"

#include "public/file_manager_menu_catalog.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QDBusConnection>
#include <QKeySequence>
#include <QWindow>
#include <QPointer>

namespace QindaQt::Apps::FileManager {

QList<QindaQt::AppShell::ActionSpec> fileManagerActionCatalog() {
  // AGENT-CONTRACT (ADR-0260): every label, menu title, description, and
  // shortcut comes from the shared public menu catalog, which the shell's
  // desktop menu projects too. Never spell one here: the desktop and a File
  // Manager window must read as one program.
  QList<QindaQt::AppShell::ActionSpec> actions;
  const QList<MenuCatalog::ActionDefinition> definitions = MenuCatalog::windowActions();
  actions.reserve(definitions.size());
  for (const MenuCatalog::ActionDefinition &definition : definitions) {
    const std::optional<MenuCatalog::MenuDefinition> menu =
        MenuCatalog::findMenu(definition.menuId);
    actions.append({.id = definition.id,
                    .menuId = definition.menuId,
                    .menuLabel = menu ? menu->label : QString{},
                    .label = definition.label,
                    .accessibleDescription = definition.description,
                    .shortcut = MenuCatalog::shortcutSequence(definition.shortcut),
                    .menuOrder = menu ? menu->order : 0,
                    .order = definition.order,
                    .enabled = true,
                    .checkable = definition.checkable,
                    .checked = false,
                    .destructive = definition.destructive});
  }
  return actions;
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
