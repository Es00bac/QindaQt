// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

// One pulled snapshot plus an explicit completeness verdict. AGENT-CONTRACT:
// when `complete` is false the tree content is undefined and every consumer
// must discard it whole — an adapter that detects overflow, a submenu cycle,
// or a mid-traversal defect never publishes a bounded prefix as if it were
// the complete application menu. `defectCode` is a short stable diagnostic
// (e.g. "too-deep", "too-many-items", "submenu-cycle"); never parsed.
struct MenuSnapshot final {
    Protocol::MenuTree tree;
    bool complete = true;
    QString defectCode;
};

// A toolkit-neutral pull source for one window's menu. The Qt Widgets
// adapter is the concrete native implementation for G0; a later
// legacy/compatibility or cross-toolkit adapter implements the same
// interface without changing MenuExporter or anything downstream of it.
//
// AGENT-CONTRACT: implementations may return an unbounded or otherwise
// malformed tree; MenuExporter validates independently and never trusts the
// source. `snapshot()` must be synchronous, side-effect-free, and called on
// the thread that owns the source; for GUI-backed sources that is the Qt
// GUI thread, and the underlying menus must not be mutated while the call
// is in progress.
class MenuSource
{
public:
    virtual ~MenuSource() = default;

    [[nodiscard]] virtual MenuSnapshot snapshot() const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Exporter
