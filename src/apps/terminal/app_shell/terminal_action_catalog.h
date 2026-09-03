// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: These literals are the AppShell action-registry identity
// for the terminal's documented command set (docs/wiki/apps/terminal.md).
// They are a separate identifier space from the stable QAction object
// names (tabNewAction, ...): ActionRegistry requires lowercase dotted ids,
// while the QAction object names are the already-published compatibility
// surface for the global-menu Widgets adapter. Keep both stable; changing
// either is a documented application-contract change. Per-profile "New Tab
// With Profile" entries are runtime data, not fixed commands, so they stay
// out of this catalog: the registry is a flat two-level command snapshot.
namespace AppShellActionIds {
inline constexpr const char *SessionNewTab = "session.new-tab";
inline constexpr const char *SessionCloseTab = "session.close-tab";
inline constexpr const char *SessionRestart = "session.restart";
inline constexpr const char *SessionManageProfiles =
    "session.manage-profiles";
inline constexpr const char *SessionNextTab = "session.next-tab";
inline constexpr const char *SessionPreviousTab = "session.previous-tab";
inline constexpr const char *SessionMoveTabLeft = "session.move-tab-left";
inline constexpr const char *SessionMoveTabRight = "session.move-tab-right";
inline constexpr const char *EditCopy = "edit.copy";
inline constexpr const char *EditPaste = "edit.paste";
inline constexpr const char *EditPasteSelection = "edit.paste-selection";
inline constexpr const char *EditSelectAll = "edit.select-all";
inline constexpr const char *ViewClear = "view.clear";
inline constexpr const char *FileQuit = "file.quit";
} // namespace AppShellActionIds

// Builds the atomic ActionSpec replacement for the terminal's documented
// command set. Pure and window-free so its shape validates against the real
// ActionRegistry without constructing a window. Enabled defaults match the
// actions' construction-time state; TerminalWindow corrects runtime-state
// actions immediately after publishing.
[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> terminalActionCatalog();

} // namespace QindaQt::Apps::Terminal
