// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/terminal_action_catalog.h"

#include <QKeySequence>

namespace QindaQt::Apps::Terminal {

QList<QindaQt::AppShell::ActionSpec> terminalActionCatalog() {
  using QindaQt::AppShell::ActionSpec;

  const auto spec = [](const char *id, const char *menu, const char *menuLabel,
                       const char *label, const char *description,
                       const char *shortcut, int menuOrder, int order,
                       bool destructive = false) {
    return ActionSpec{
        .id = QString::fromLatin1(id),
        .menuId = QString::fromLatin1(menu),
        .menuLabel = QString::fromLatin1(menuLabel),
        .label = QString::fromLatin1(label),
        .accessibleDescription = QString::fromLatin1(description),
        .shortcut = QKeySequence(QLatin1String(shortcut)),
        .menuOrder = menuOrder,
        .order = order,
        .enabled = true,
        .checkable = false,
        .checked = false,
        .destructive = destructive,
    };
  };

  return {
      spec(AppShellActionIds::SessionNewTab, "session", "Session", "New Tab",
           "Open a new terminal tab with the default profile",
           "Ctrl+Shift+T", 1, 0),
      spec(AppShellActionIds::SessionCloseTab, "session", "Session",
           "Close Tab", "Close the active terminal tab",
           "Ctrl+Shift+W", 1, 1, true),
      spec(AppShellActionIds::SessionRestart, "session", "Session",
           "Restart Session",
           "Close this session and start a fresh one with the same profile",
           "Ctrl+Shift+R", 1, 2),
      spec(AppShellActionIds::SessionNextTab, "session", "Session",
           "Next Tab", "Switch to the next terminal tab", "Ctrl+Shift+Right",
           1, 3),
      spec(AppShellActionIds::SessionPreviousTab, "session", "Session",
           "Previous Tab", "Switch to the previous terminal tab",
           "Ctrl+Shift+Left", 1, 4),
      spec(AppShellActionIds::SessionMoveTabLeft, "session", "Session",
           "Move Tab Left", "Move the active terminal tab one position left",
           "Ctrl+Shift+Alt+Left", 1, 5),
      spec(AppShellActionIds::SessionMoveTabRight, "session", "Session",
           "Move Tab Right",
           "Move the active terminal tab one position right",
           "Ctrl+Shift+Alt+Right", 1, 6),
      spec(AppShellActionIds::SessionManageProfiles, "session", "Session",
           "Manage Profiles…", "Edit terminal profiles and tab restore",
           "Ctrl+Shift+P", 1, 7),
      spec(AppShellActionIds::EditCopy, "edit", "Edit", "Copy",
           "Copy the terminal selection to the clipboard", "Ctrl+Shift+C", 2,
           0),
      spec(AppShellActionIds::EditPaste, "edit", "Edit", "Paste",
           "Paste the clipboard into the terminal", "Ctrl+Shift+V", 2, 1),
      spec(AppShellActionIds::EditPasteSelection, "edit", "Edit",
           "Paste Selection", "Paste the primary selection into the terminal",
           "Ctrl+Shift+Insert", 2, 2),
      spec(AppShellActionIds::EditSelectAll, "edit", "Edit", "Select All",
           "Select the entire terminal buffer", "Ctrl+Shift+A", 2, 3),
      spec(AppShellActionIds::ViewClear, "view", "View", "Clear Display",
           "Clear the terminal display and scrollback", "Ctrl+Shift+K", 3, 0),
      spec(AppShellActionIds::ViewFind, "view", "View", "Find…",
           "Find text in this session's scrollback", "Ctrl+Shift+F", 3, 1),
      spec(AppShellActionIds::ViewFindNext, "view", "View", "Find Next",
           "Select the next scrollback match", "F3", 3, 2),
      spec(AppShellActionIds::ViewFindPrevious, "view", "View",
           "Find Previous", "Select the previous scrollback match",
           "Shift+F3", 3, 3),
      spec("view.zoom-in", "view", "View", "Zoom In", "Increase terminal text size", "Ctrl+Shift++", 3, 4),
      spec("view.zoom-out", "view", "View", "Zoom Out", "Decrease terminal text size", "Ctrl+Shift+-", 3, 5),
      spec("view.zoom-reset", "view", "View", "Actual Size", "Restore profile text size", "Ctrl+Shift+0", 3, 6),
      spec(AppShellActionIds::LinkPrevious, "links", "Links",
           "Select Previous Link", "Select the previous link in visible output",
           "Ctrl+Shift+Alt+L", 4, 0),
      spec(AppShellActionIds::LinkNext, "links", "Links", "Select Next Link",
           "Select the next link in visible output", "Ctrl+Shift+L", 4, 1),
      spec(AppShellActionIds::LinkCopy, "links", "Links", "Copy Link",
           "Copy the selected link exactly", "Ctrl+Shift+Y", 4, 2),
      spec(AppShellActionIds::LinkOpen, "links", "Links", "Open Link…",
           "Confirm and open the selected link", "Ctrl+Shift+O", 4, 3),
      spec(AppShellActionIds::FileQuit, "file", "File", "Quit",
           "Close every session and quit", "Ctrl+Shift+Q", 0, 0, true),
  };
}

} // namespace QindaQt::Apps::Terminal
