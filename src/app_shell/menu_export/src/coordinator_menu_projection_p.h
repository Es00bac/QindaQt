// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::AppShell::MenuExport {

class CoordinatorMenuProjection final {
public:
  explicit CoordinatorMenuProjection(const ApplicationCoordinator &coordinator);
  [[nodiscard]] Shell::GlobalMenu::Protocol::MenuTree snapshot() const;

private:
  const ApplicationCoordinator &m_coordinator;
};

} // namespace QindaQt::AppShell::MenuExport
