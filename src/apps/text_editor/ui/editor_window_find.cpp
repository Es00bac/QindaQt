// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "ui/find_replace_bar.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>

namespace QindaQt::Apps::TextEditor {

void EditorWindow::openFind(const bool replaceMode) {
  m_findBar->open(replaceMode);
  updateActionStates();
}

void EditorWindow::findMatch(const FindDirection direction) {
  QPlainTextEdit *active = editor();
  if (!active) {
    return;
  }
  if (!m_findBar->isVisible()) {
    openFind(false);
    return;
  }
  const QTextCursor cursor = active->textCursor();
  const qsizetype position =
      direction == FindDirection::Next
          ? (cursor.hasSelection() ? cursor.selectionEnd() : cursor.position())
          : (cursor.hasSelection() ? cursor.selectionStart()
                                   : cursor.position());
  const FindResult result = FindReplaceEngine::find(
      active->toPlainText(), m_findBar->options(), position, direction);
  selectFindResult(result);
}

void EditorWindow::selectFindResult(const FindResult &result) {
  QPlainTextEdit *active = editor();
  if (!active) {
    return;
  }
  if (!result.ok()) {
    m_findBar->setStatus(result.diagnostic);
    return;
  }
  if (!result.hasMatch()) {
    m_findBar->setStatus(tr("No matches"));
    return;
  }
  const FindMatch match = result.matches.at(result.currentIndex);
  QTextCursor cursor(active->document());
  cursor.setPosition(static_cast<int>(match.start));
  cursor.setPosition(static_cast<int>(match.start + match.length),
                     QTextCursor::KeepAnchor);
  active->setTextCursor(cursor);
  active->ensureCursorVisible();
  QString status = tr("Match %1 of %2")
                       .arg(result.currentIndex + 1)
                       .arg(result.matches.size());
  if (result.wrapped) {
    status.append(tr(" (wrapped)"));
  }
  m_findBar->setStatus(status);
}

void EditorWindow::replaceCurrent() {
  QPlainTextEdit *active = editor();
  if (!active) {
    return;
  }
  QTextCursor cursor = active->textCursor();
  const FindResult result =
      FindReplaceEngine::find(active->toPlainText(), m_findBar->options(),
                              cursor.selectionStart(), FindDirection::Next);
  if (!result.ok() || !result.hasMatch()) {
    selectFindResult(result);
    return;
  }
  const FindMatch match = result.matches.at(result.currentIndex);
  if (!cursor.hasSelection() || cursor.selectionStart() != match.start ||
      cursor.selectionEnd() != match.start + match.length) {
    selectFindResult(result);
    return;
  }
  cursor.beginEditBlock();
  cursor.insertText(m_findBar->replacement());
  cursor.endEditBlock();
  m_findBar->setStatus(tr("Replaced 1 match"));
  findMatch(FindDirection::Next);
}

void EditorWindow::replaceAll() {
  QPlainTextEdit *active = editor();
  if (!active) {
    return;
  }
  const FindResult result = FindReplaceEngine::find(
      active->toPlainText(), m_findBar->options(), 0, FindDirection::Next);
  if (!result.ok() || result.matches.isEmpty()) {
    selectFindResult(result);
    return;
  }
  QTextCursor editBlock(active->document());
  editBlock.beginEditBlock();
  for (auto it = result.matches.crbegin(); it != result.matches.crend(); ++it) {
    QTextCursor replacement(active->document());
    replacement.setPosition(static_cast<int>(it->start));
    replacement.setPosition(static_cast<int>(it->start + it->length),
                            QTextCursor::KeepAnchor);
    replacement.insertText(m_findBar->replacement());
  }
  editBlock.endEditBlock();
  m_findBar->setStatus(tr("Replaced %1 matches").arg(result.matches.size()));
}

} // namespace QindaQt::Apps::TextEditor
