// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_menu/desktop_menu_types.h"

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QHash>
#include <QString>

namespace QindaQt::Shell::DesktopMenu {

// One built desktop menu: the canonical tree the global menu presents and
// the id -> command table that is the ONLY way an activated id becomes a
// command. Both come from the same build, so they can never disagree.
struct BuiltDesktopMenu final {
  GlobalMenu::Protocol::MenuTree tree;
  QHash<QString, DesktopMenuCommand> commands;
};

// Pure builder (ADR-0260). Produces the File Manager's menu shown while no
// application is active: the application menu titled like the File Manager
// with the system items, then File, Edit, View, Go (labels and menu titles
// from File Manager's public menu catalog, never spelled here), Window, and
// Help. Entries whose capability is not present are omitted, present but not
// enabled ones are disabled, menus left empty are omitted, and separators
// only ever sit between two non-empty groups.
//
// AGENT-CONTRACT: the tree always validates under the canonical bounds
// (menu_limits.h): dynamic text (workspace names) is sanitized and bounded,
// an id that would exceed the id bound drops its entry, and list lengths are
// capped. The tree carries no lineage (null owner and epoch); the facade's
// desktop channel never runs the application invocation guard.
[[nodiscard]] BuiltDesktopMenu buildDesktopMenu(const DesktopMenuFacts &facts);

// Short title and icon of the application whose menu this is (the File
// Manager), taken from File Manager's public catalog.
[[nodiscard]] QString desktopMenuTitle();
[[nodiscard]] QString desktopMenuIconName();
// File Manager's desktop entry, which "New File Manager Window" activates.
[[nodiscard]] QString fileManagerDesktopEntryId();

// The commands that ask the user first, with the same wording the system
// menu's confirmation dialog uses (log out, restart, shut down).
[[nodiscard]] bool commandNeedsConfirmation(DesktopCommand kind);
[[nodiscard]] QString confirmationTitle(DesktopCommand kind);
[[nodiscard]] QString confirmationText(DesktopCommand kind);

} // namespace QindaQt::Shell::DesktopMenu
