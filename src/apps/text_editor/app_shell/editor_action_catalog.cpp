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
                      .label = QString::fromLatin1(label),
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
           "Open a new untitled document tab", QKeySequence::New, 0, 0),
      spec(AppShellActionIds::FileOpen, "file", "File", "Open…",
           "Open a local document in a new tab", QKeySequence::Open, 0, 1),
      spec(AppShellActionIds::FileCloseTab, "file", "File", "Close Tab",
           "Close the active document after dirty consent", key("Ctrl+W"), 0,
           2),
      spec(AppShellActionIds::FileSave, "file", "File", "Save",
           "Save the active document when its byte revision still matches",
           QKeySequence::Save, 0, 3, false),
      spec(AppShellActionIds::FileSaveAs, "file", "File", "Save As…",
           "Choose a new local target for the active document",
           QKeySequence::SaveAs, 0, 4),
      spec(AppShellActionIds::FileQuit, "file", "File", "Quit",
           "Close all documents after bounded dirty consent",
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
      spec(AppShellActionIds::TabNext, "tabs", "Tabs", "Next Tab",
           "Select the next document tab with wrap", key("Ctrl+Tab"), 2, 0,
           false),
      spec(AppShellActionIds::TabPrevious, "tabs", "Tabs", "Previous Tab",
           "Select the previous document tab with wrap", key("Ctrl+Shift+Tab"),
           2, 1, false),
      spec(AppShellActionIds::RestoreDocuments, "settings", "Settings",
           "Restore Open Documents", "Persist and restore open document paths",
           key("Ctrl+Alt+R"), 3, 0, false, true),
  };
  for (int index = 1; index <= 9; ++index) {
    actions.append(
        spec(qPrintable(QStringLiteral("tabs.select-%1").arg(index)), "tabs",
             "Tabs", qPrintable(QStringLiteral("Select Tab %1").arg(index)),
             qPrintable(QStringLiteral("Select document tab %1").arg(index)),
             key(qPrintable(QStringLiteral("Ctrl+%1").arg(index))), 2,
             index + 1, index == 1));
  }
  return actions;
}

} // namespace QindaQt::Apps::TextEditor
