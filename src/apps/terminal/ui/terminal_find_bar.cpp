// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_find_bar.h"

#include <QAccessible>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QToolButton>

namespace QindaQt::Apps::Terminal {

TerminalFindBar::TerminalFindBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("terminalFindBar"));
  setAccessibleName(QStringLiteral("Find in terminal scrollback"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 4, 6, 4);

  auto *label = new QLabel(QStringLiteral("Find:"), this);
  m_editor = new QLineEdit(this);
  m_editor->setObjectName(QStringLiteral("terminalFindText"));
  m_editor->setMaxLength(int{kTerminalSearchPatternLimit});
  m_editor->setAccessibleName(QStringLiteral("Search text"));
  label->setBuddy(m_editor);
  m_caseSensitive = new QCheckBox(QStringLiteral("Match case"), this);
  m_caseSensitive->setObjectName(QStringLiteral("terminalFindCase"));
  m_regularExpression = new QCheckBox(QStringLiteral("Regular expression"), this);
  m_regularExpression->setObjectName(QStringLiteral("terminalFindRegex"));
  m_previous = new QToolButton(this);
  m_previous->setObjectName(QStringLiteral("terminalFindPreviousButton"));
  m_previous->setText(QStringLiteral("Previous"));
  m_previous->setAccessibleName(QStringLiteral("Previous match"));
  m_next = new QToolButton(this);
  m_next->setObjectName(QStringLiteral("terminalFindNextButton"));
  m_next->setText(QStringLiteral("Next"));
  m_next->setAccessibleName(QStringLiteral("Next match"));
  m_status = new QLabel(QStringLiteral("Enter text to search"), this);
  m_status->setObjectName(QStringLiteral("terminalFindStatus"));
  m_status->setAccessibleName(QStringLiteral("Search status: Enter text to search"));

  layout->addWidget(label);
  layout->addWidget(m_editor, 1);
  layout->addWidget(m_caseSensitive);
  layout->addWidget(m_regularExpression);
  layout->addWidget(m_previous);
  layout->addWidget(m_next);
  layout->addWidget(m_status);

  connect(m_editor, &QLineEdit::returnPressed, this, [this] {
    emit searchRequested(TerminalSearchDirection::Next);
  });
  connect(m_previous, &QToolButton::clicked, this,
          [this] { emit searchRequested(TerminalSearchDirection::Previous); });
  connect(m_next, &QToolButton::clicked, this,
          [this] { emit searchRequested(TerminalSearchDirection::Next); });
  const auto criteriaChanged = [this] {
    emit searchRequested(TerminalSearchDirection::Initial);
  };
  connect(m_editor, &QLineEdit::textChanged, this, criteriaChanged);
  connect(m_caseSensitive, &QCheckBox::toggled, this, criteriaChanged);
  connect(m_regularExpression, &QCheckBox::toggled, this, criteriaChanged);
  hide();
}

TerminalSearchQuery TerminalFindBar::query() const {
  return {.pattern = m_editor->text(),
          .caseSensitive = m_caseSensitive->isChecked(),
          .regularExpression = m_regularExpression->isChecked()};
}

void TerminalFindBar::setQuery(const TerminalSearchQuery &query) {
  const QSignalBlocker editorBlocker(m_editor);
  const QSignalBlocker caseBlocker(m_caseSensitive);
  const QSignalBlocker regexBlocker(m_regularExpression);
  m_editor->setText(query.pattern);
  m_caseSensitive->setChecked(query.caseSensitive);
  m_regularExpression->setChecked(query.regularExpression);
}

void TerminalFindBar::presentResult(const TerminalSearchResult &result) {
  QString text = result.diagnostic;
  if (result.found) {
    text = QStringLiteral("Match %1 of %2%3")
               .arg(result.current)
               .arg(result.total)
               .arg(result.wrapped ? QStringLiteral(" (wrapped)") : QString());
  } else if (result.accepted && text.isEmpty()) {
    text = QStringLiteral("No matches");
  }
  if (text.isEmpty()) {
    text = QStringLiteral("Enter text to search");
  }
  m_status->setText(text);
  m_status->setAccessibleName(QStringLiteral("Search status: %1").arg(text));
  QAccessibleEvent event(m_status, QAccessible::NameChanged);
  QAccessible::updateAccessibility(&event);
}

void TerminalFindBar::focusEditor() {
  m_editor->setFocus(Qt::ShortcutFocusReason);
  m_editor->selectAll();
}

void TerminalFindBar::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    emit closeRequested();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

} // namespace QindaQt::Apps::Terminal
