// SPDX-License-Identifier: LGPL-3.0-or-later
#include "coordinator_menu_projection_p.h"

#include <qindaqt/app_shell/application_coordinator.h>

#include <algorithm>
#include <tuple>
#include <utility>

namespace QindaQt::AppShell::MenuExport {

CoordinatorMenuProjection::CoordinatorMenuProjection(
    const ApplicationCoordinator &coordinator)
    : m_coordinator(coordinator) {}

Shell::GlobalMenu::Protocol::MenuTree CoordinatorMenuProjection::snapshot() const {
  namespace Protocol = Shell::GlobalMenu::Protocol;
  QList<ActionSpec> actions = m_coordinator.actionRegistry().actions();
  std::sort(actions.begin(), actions.end(),
            [](const ActionSpec &left, const ActionSpec &right) {
              return std::tie(left.menuOrder, left.menuId, left.order, left.id) <
                     std::tie(right.menuOrder, right.menuId, right.order, right.id);
            });
  Protocol::MenuTree tree;
  for (qsizetype index = 0; index < actions.size();) {
    const QString menuId = actions.at(index).menuId;
    Protocol::MenuItem menu{.id = QStringLiteral("menu:") + menuId,
                            .kind = Protocol::MenuItemKind::Submenu,
                            .text = actions.at(index).menuLabel};
    while (index < actions.size() && actions.at(index).menuId == menuId) {
      const ActionSpec &action = actions.at(index++);
      menu.children.append(Protocol::MenuItem{
          .id = action.id,
          .kind = Protocol::MenuItemKind::Action,
          .text = action.label,
          .mnemonicIndex = -1,
          .shortcutText = action.shortcut.toString(QKeySequence::PortableText),
          .enabled = action.enabled,
          .visible = true,
          .checkable = action.checkable,
          .checked = action.checked});
    }
    tree.items.append(std::move(menu));
  }
  return tree;
}

} // namespace QindaQt::AppShell::MenuExport
