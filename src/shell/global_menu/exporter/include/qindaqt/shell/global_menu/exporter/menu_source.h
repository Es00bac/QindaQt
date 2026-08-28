// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

// A toolkit-neutral pull source for one window's menu. The Qt Widgets
// adapter is the concrete native implementation for G0; a later
// legacy/compatibility or cross-toolkit adapter implements the same
// interface without changing MenuExporter or anything downstream of it.
class MenuSource
{
public:
    virtual ~MenuSource() = default;

    // AGENT-CONTRACT: implementations may return an unbounded or otherwise
    // malformed tree; MenuExporter validates independently and never trusts
    // the source. `snapshot()` must be synchronous and side-effect-free.
    [[nodiscard]] virtual Protocol::MenuTree snapshot() const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Exporter
