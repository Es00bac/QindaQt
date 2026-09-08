// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_action_catalog.h"

#include <QKeySequence>

namespace QindaQt::Apps::TextEditor {

QList<QindaQt::AppShell::ActionSpec> editorActionCatalog() {
  using QindaQt::AppShell::ActionSpec;
  const auto spec = [](const char *id, const char *menu, const char *menuLabel,
                       const char *label, const char *description,
                       const QKeySequence &shortcut, int menuOrder, int order,
                       bool enabled = true, bool checkable = false) {
    return ActionSpec{.id = QString::fromLatin1(id),
                      .menuId = QString::fromLatin1(menu),
                      .menuLabel = QString::fromLatin1(menuLabel),
                      .label = QString::fromUtf8(label),
                      .accessibleDescription = QString::fromLatin1(description),
                      .shortcut = shortcut,
                      .menuOrder = menuOrder,
                      .order = order,
                      .enabled = enabled,
                      .checkable = checkable,
                      .checked = false};
  };
  const auto key = [](const char *sequence) {
    return QKeySequence(QString::fromLatin1(sequence));
  };

  QList<ActionSpec> actions{
      spec(AppShellActionIds::FileNew, "file", "File", "New",
           "Open a new untitled document window", QKeySequence::New, 0, 0),
      spec(AppShellActionIds::FileOpen, "file", "File", "Open…",
           "Open a local document in a new window", QKeySequence::Open, 0, 1),
      spec(AppShellActionIds::FileCloseWindow, "file", "File", "Close Window",
           "Close the active document after dirty consent", key("Ctrl+W"), 0,
           2),
      spec(AppShellActionIds::FileSave, "file", "File", "Save",
           "Save the active document when its byte revision still matches",
           QKeySequence::Save, 0, 3, false),
      spec(AppShellActionIds::FileSaveAs, "file", "File", "Save As…",
           "Choose a new local target for the active document",
           QKeySequence::SaveAs, 0, 4),
      spec(AppShellActionIds::FileQuit, "file", "File", "Quit",
           "Close this window after document dirty consent",
           QKeySequence::Quit, 0, 5),
      spec(AppShellActionIds::EditUndo, "edit", "Edit", "Undo",
           "Undo the active document's last change", QKeySequence::Undo, 1, 0,
           false),
      spec(AppShellActionIds::EditRedo, "edit", "Edit", "Redo",
           "Redo the active document's last undone change", QKeySequence::Redo,
           1, 1, false),
      spec(AppShellActionIds::EditCut, "edit", "Edit", "Cut",
           "Cut the active document selection", QKeySequence::Cut, 1, 2, false),
      spec(AppShellActionIds::EditCopy, "edit", "Edit", "Copy",
           "Copy the active document selection", QKeySequence::Copy, 1, 3,
           false),
      spec(AppShellActionIds::EditPaste, "edit", "Edit", "Paste",
           "Paste into the active document", QKeySequence::Paste, 1, 4, false),
      spec(AppShellActionIds::EditSelectAll, "edit", "Edit", "Select All",
           "Select the active document", QKeySequence::SelectAll, 1, 5),
      spec(AppShellActionIds::EditFind, "edit", "Edit", "Find…",
           "Open the in-window find bar", QKeySequence::Find, 1, 6),
      spec(AppShellActionIds::EditReplace, "edit", "Edit", "Replace…",
           "Open the in-window replace bar", key("Ctrl+H"), 1, 7),
      spec(AppShellActionIds::EditFindNext, "edit", "Edit", "Find Next",
           "Select the next match with wrap", key("F3"), 1, 8, false),
      spec(AppShellActionIds::EditFindPrevious, "edit", "Edit", "Find Previous",
           "Select the previous match with wrap", key("Shift+F3"), 1, 9, false),
      spec(AppShellActionIds::EditFindClose, "edit", "Edit", "Close Find",
           "Close the find and replace bar", key("Escape"), 1, 10, false),
      spec("edit.go-to-line", "edit", "Edit", "Go to Line…", "Move to a numbered line", key("Ctrl+G"), 1, 11),
      spec("edit.indent", "edit", "Edit", "Indent Lines", "Indent selected lines by four spaces", key("Ctrl+]"), 1, 12),
      spec("edit.unindent", "edit", "Edit", "Unindent Lines", "Remove one level of indentation", key("Ctrl+["), 1, 13),
      spec("view.word-wrap", "view", "View", "Word Wrap", "Wrap long lines at the window edge", key("Ctrl+Alt+W"), 2, 0, true, true),
      spec("view.zoom-in", "view", "View", "Zoom In", "Increase text size", key("Ctrl++"), 2, 1),
      spec("view.zoom-out", "view", "View", "Zoom Out", "Decrease text size", key("Ctrl+-"), 2, 2),
      spec("view.zoom-reset", "view", "View", "Actual Size", "Restore the configured text size", key("Ctrl+0"), 2, 3),
      spec(AppShellActionIds::RestoreDocuments, "settings", "Settings",
           "Restore Open Documents", "Persist and restore open document paths",
           key("Ctrl+Alt+R"), 4, 0, false, true),
  };
  return actions;
}

} // namespace QindaQt::Apps::TextEditor
