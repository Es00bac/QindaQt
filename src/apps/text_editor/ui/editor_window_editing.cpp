// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_editor.h"
#include "editor_window.h"
#include <QAction>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>

namespace QindaQt::Apps::TextEditor {
void EditorWindow::createEditingActions() {
  const auto add = [this](const QString &id, const QString &name,
                          const QString &label, const QString &shortcut,
                          auto handler, bool view) {
    auto *action = new QAction(label, this);
    action->setObjectName(name);
    action->setShortcut(QKeySequence(shortcut));
    connect(action, &QAction::triggered, this, handler);
    m_appShellActionIds.insert(id, action);
    (view ? m_actions.viewTools : m_actions.editingTools).append(action);
    return action;
  };
  add(
      QStringLiteral("edit.go-to-line"), QStringLiteral("editGoToLineAction"),
      tr("Go to &Line…"), QStringLiteral("Ctrl+G"),
      [this] {
        auto *active = static_cast<DocumentEditor *>(editor());
        if (!active)
          return;
        bool accepted = false;
        const int line =
            QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"),
                                 active->textCursor().blockNumber() + 1, 1,
                                 active->blockCount(), 1, &accepted);
        if (accepted)
          active->goToLine(line);
      },
      false);
  add(
      QStringLiteral("edit.indent"), QStringLiteral("editIndentAction"),
      tr("&Indent Lines"), QStringLiteral("Ctrl+]"),
      [this] {
        if (auto *active = static_cast<DocumentEditor *>(editor()))
          active->indentLines(false);
      },
      false);
  add(
      QStringLiteral("edit.unindent"), QStringLiteral("editUnindentAction"),
      tr("&Unindent Lines"), QStringLiteral("Ctrl+["),
      [this] {
        if (auto *active = static_cast<DocumentEditor *>(editor()))
          active->indentLines(true);
      },
      false);
  auto *wrap = add(
      QStringLiteral("view.word-wrap"), QStringLiteral("viewWordWrapAction"),
      tr("&Word Wrap"), QStringLiteral("Ctrl+Alt+W"),
      [this] {
        if (auto *active = editor())
          active->setLineWrapMode(active->lineWrapMode() ==
                                          QPlainTextEdit::NoWrap
                                      ? QPlainTextEdit::WidgetWidth
                                      : QPlainTextEdit::NoWrap);
        updateActionStates();
      },
      true);
  wrap->setCheckable(true);
  wrap->setChecked(true);
  add(
      QStringLiteral("view.zoom-in"), QStringLiteral("viewZoomInAction"),
      tr("Zoom &In"), QStringLiteral("Ctrl++"),
      [this] {
        if (auto *active = static_cast<DocumentEditor *>(editor()))
          active->zoomText(1);
      },
      true);
  add(
      QStringLiteral("view.zoom-out"), QStringLiteral("viewZoomOutAction"),
      tr("Zoom &Out"), QStringLiteral("Ctrl+-"),
      [this] {
        if (auto *active = static_cast<DocumentEditor *>(editor()))
          active->zoomText(-1);
      },
      true);
  add(
      QStringLiteral("view.zoom-reset"), QStringLiteral("viewZoomResetAction"),
      tr("&Actual Size"), QStringLiteral("Ctrl+0"),
      [this] {
        if (auto *active = static_cast<DocumentEditor *>(editor()))
          active->resetZoom();
      },
      true);
}
void EditorWindow::createViewMenu() {
  auto *view = menuBar()->addMenu(tr("&View"));
  view->setObjectName(QStringLiteral("viewMenu"));
  view->addActions(m_actions.viewTools);
}
} // namespace QindaQt::Apps::TextEditor
