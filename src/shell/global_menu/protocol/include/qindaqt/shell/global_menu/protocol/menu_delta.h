// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

enum class MenuDeltaOp {
    Removed,
    Inserted,
    Updated,
};

// One deterministic change to a single node, keyed by its stable `id`.
// `parentId` is empty for a root-level item. `Updated` covers any field
// change, including a move to a different parent or sibling index.
struct MenuItemDelta final {
    MenuDeltaOp op = MenuDeltaOp::Updated;
    QString id;
    QString parentId;

    bool operator==(const MenuItemDelta &) const = default;
};

// AGENT-CONTRACT: operations are ordered Removed (previous pre-order), then
// Inserted (next pre-order), then Updated (next pre-order). A consumer that
// applies them in that exact order never observes an intermediate state with
// a dangling parent reference.
struct MenuTreeDelta final {
    QList<MenuItemDelta> operations;

    [[nodiscard]] bool identical() const noexcept { return operations.isEmpty(); }
};

// Both trees must already be individually valid (see validateMenuTree); the
// diff assumes globally unique ids and does not re-validate structure.
[[nodiscard]] MenuTreeDelta computeMenuTreeDelta(const MenuTree &previous, const MenuTree &next);

} // namespace QindaQt::Shell::GlobalMenu::Protocol
