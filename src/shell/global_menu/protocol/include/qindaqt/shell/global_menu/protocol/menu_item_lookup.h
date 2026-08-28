// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_item.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// Depth-first lookup by stable id across a validated item forest. Returns
// nullptr when no item carries that id. Every caller that authorizes an
// invocation must use this rather than trusting a caller-supplied index or
// path, since ids are the only stable cross-snapshot key.
[[nodiscard]] const MenuItem *findMenuItemById(const QList<MenuItem> &items, const QString &id);

} // namespace QindaQt::Shell::GlobalMenu::Protocol
