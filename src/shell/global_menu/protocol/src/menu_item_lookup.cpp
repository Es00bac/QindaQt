// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

const MenuItem *findMenuItemById(const QList<MenuItem> &items, const QString &id)
{
    for (const MenuItem &item : items) {
        if (item.id == id) {
            return &item;
        }
        if (const MenuItem *found = findMenuItemById(item.children, id)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace QindaQt::Shell::GlobalMenu::Protocol
