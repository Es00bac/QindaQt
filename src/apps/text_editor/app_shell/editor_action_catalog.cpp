// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_action_catalog.h"

#include <QKeySequence>
#include <QString>

namespace QindaQt::Apps::TextEditor {

QList<QindaQt::AppShell::ActionSpec> editorActionCatalog() {
  using QindaQt::AppShell::ActionSpec;

  return {
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::FileNew),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("New"),
          .accessibleDescription =
              QStringLiteral("Start an untitled document after dirty consent"),
          .shortcut = QKeySequence(QKeySequence::New),
          .menuOrder = 0,
          .order = 0,
          .enabled = true,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::FileOpen),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Open…"),
          .accessibleDescription =
              QStringLiteral("Open one local document after dirty consent"),
          .shortcut = QKeySequence(QKeySequence::Open),
          .menuOrder = 0,
          .order = 1,
          .enabled = true,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::FileSave),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Save"),
          .accessibleDescription = QStringLiteral(
              "Save only when the external byte revision still matches"),
          .shortcut = QKeySequence(QKeySequence::Save),
          .menuOrder = 0,
          .order = 2,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::FileSaveAs),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Save As…"),
          .accessibleDescription = QStringLiteral(
              "Choose a new local target with explicit replacement consent"),
          .shortcut = QKeySequence(QKeySequence::SaveAs),
          .menuOrder = 0,
          .order = 3,
          .enabled = true,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::FileQuit),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Quit"),
          .accessibleDescription = QStringLiteral("Close after dirty consent"),
          .shortcut = QKeySequence(QKeySequence::Quit),
          .menuOrder = 0,
          .order = 4,
          .enabled = true,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditUndo),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Undo"),
          .accessibleDescription =
              QStringLiteral("Undo the last document change"),
          .shortcut = QKeySequence(QKeySequence::Undo),
          .menuOrder = 1,
          .order = 0,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditRedo),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Redo"),
          .accessibleDescription =
              QStringLiteral("Redo the last undone document change"),
          .shortcut = QKeySequence(QKeySequence::Redo),
          .menuOrder = 1,
          .order = 1,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditCut),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Cut"),
          .accessibleDescription =
              QStringLiteral("Cut the current selection to the clipboard"),
          .shortcut = QKeySequence(QKeySequence::Cut),
          .menuOrder = 1,
          .order = 2,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditCopy),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Copy"),
          .accessibleDescription =
              QStringLiteral("Copy the current selection to the clipboard"),
          .shortcut = QKeySequence(QKeySequence::Copy),
          .menuOrder = 1,
          .order = 3,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditPaste),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Paste"),
          .accessibleDescription =
              QStringLiteral("Paste clipboard contents at the cursor"),
          .shortcut = QKeySequence(QKeySequence::Paste),
          .menuOrder = 1,
          .order = 4,
          .enabled = false,
      },
      ActionSpec{
          .id = QString::fromLatin1(AppShellActionIds::EditSelectAll),
          .menuId = QStringLiteral("edit"),
          .menuLabel = QStringLiteral("Edit"),
          .label = QStringLiteral("Select All"),
          .accessibleDescription =
              QStringLiteral("Select the complete document"),
          .shortcut = QKeySequence(QKeySequence::SelectAll),
          .menuOrder = 1,
          .order = 5,
          .enabled = true,
      },
  };
}

} // namespace QindaQt::Apps::TextEditor
