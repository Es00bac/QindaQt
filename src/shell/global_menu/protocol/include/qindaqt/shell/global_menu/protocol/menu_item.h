// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// Canonical, toolkit-neutral menu node. A native Qt exporter, a future
// legacy/compatibility adapter, and a future cross-toolkit adapter must all
// converge on this shape; nothing downstream may branch on the originating
// toolkit. See docs/wiki/shell/global-menu.md.
enum class MenuItemKind {
    Action,
    Separator,
    Submenu,
};

// AGENT-CONTRACT: `text` never contains a mnemonic marker character; the
// mnemonic position is `mnemonicIndex` (a UTF-16 code-unit offset into
// `text`, or -1 when the item has none). This keeps toolkit-specific escaping
// ('&', '_', ...) out of the canonical model. `id` is stable across snapshots
// of the same menu when the source can provide one (see the Qt Widgets
// adapter); it is the join key for delta computation and safe invocation.
struct MenuItem final {
    QString id{};
    MenuItemKind kind = MenuItemKind::Action;
    QString text{};
    int mnemonicIndex = -1;
    QString shortcutText{};
    bool enabled = true;
    bool visible = true;
    bool checkable = false;
    bool checked = false;
    // Empty means the item is not part of an exclusive (radio) group. Items
    // sharing a non-empty value under the same parent form one group.
    QString radioGroup{};
    QList<MenuItem> children{};

    [[nodiscard]] bool operator==(const MenuItem &other) const;
};

inline bool MenuItem::operator==(const MenuItem &other) const = default;

} // namespace QindaQt::Shell::GlobalMenu::Protocol
