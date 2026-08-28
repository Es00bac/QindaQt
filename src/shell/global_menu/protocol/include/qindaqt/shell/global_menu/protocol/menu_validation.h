// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// A short, stable machine-readable code (not localized prose), matching the
// Audio1 ValidationResult convention. Empty when accepted.
struct ValidationResult final {
    bool accepted = false;
    QString reasonCode;
    // Pre-order path to the offending node, e.g. "items[2].children[0]", or
    // empty for a tree-level defect. Diagnostic only; never parsed.
    QString path;
};

// Validates bounds, structural invariants, and text well-formedness for a
// complete tree. Hostile or malformed input rejects the whole tree rather
// than admitting a partial result, matching the fail-closed convention used
// by every other QindaQt wire model. An empty `items` list is valid (a window
// with no menu yet).
[[nodiscard]] ValidationResult validateMenuTree(const MenuTree &tree);

} // namespace QindaQt::Shell::GlobalMenu::Protocol
