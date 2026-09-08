// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_find_bar.h"

#include <QAccessible>
#include <QEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "qindaqt/controls/application_icon.h"
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QToolButton>

namespace QindaQt::Apps::Terminal {

TerminalFindBar::TerminalFindBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("terminalFindBar"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAccessibleName(QStringLiteral("Find in terminal scrollback"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 8, 12, 8);
  auto *searchRow = new QHBoxLayout();
  auto *optionsRow = new QHBoxLayout();
  m_editor = new QLineEdit(this);
  m_editor->setObjectName(QStringLiteral("terminalFindText"));
  m_editor->setMaxLength(int{kTerminalSearchPatternLimit});
  m_editor->setAccessibleName(QStringLiteral("Search text"));
  m_editor->setPlaceholderText(QStringLiteral("Find in scrollback"));
  m_editor->setClearButtonEnabled(true);
  m_caseSensitive = new QToolButton(this);
  m_caseSensitive->setText(QStringLiteral("Aa"));
  m_caseSensitive->setCheckable(true);
  m_caseSensitive->setAccessibleName(QStringLiteral("Match case"));
  m_caseSensitive->setToolTip(QStringLiteral("Match case"));
  m_caseSensitive->setObjectName(QStringLiteral("terminalFindCase"));
  m_regularExpression = new QToolButton(this);
  m_regularExpression->setText(QStringLiteral(".*"));
  m_regularExpression->setCheckable(true);
  m_regularExpression->setAccessibleName(QStringLiteral("Regular expression"));
  m_regularExpression->setToolTip(QStringLiteral("Use a regular expression"));
  m_regularExpression->setObjectName(QStringLiteral("terminalFindRegex"));
  m_previous = new QToolButton(this);
  m_previous->setObjectName(QStringLiteral("terminalFindPreviousButton"));
  m_previous->setToolTip(QStringLiteral("Previous match (Shift+F3)"));
  m_previous->setAccessibleName(QStringLiteral("Previous match"));
  m_next = new QToolButton(this);
  m_next->setObjectName(QStringLiteral("terminalFindNextButton"));
  m_next->setToolTip(QStringLiteral("Next match (F3)"));
  m_next->setAccessibleName(QStringLiteral("Next match"));
  m_status = new QLabel(QStringLiteral("Enter text to search"), this);
  m_status->setObjectName(QStringLiteral("terminalFindStatus"));
  m_status->setAccessibleName(QStringLiteral("Search status: Enter text to search"));

  m_close = new QToolButton(this);
  m_close->setObjectName(QStringLiteral("terminalFindCloseButton"));
  m_close->setAccessibleName(QStringLiteral("Close search"));
  m_close->setToolTip(QStringLiteral("Close search (Escape)"));
  connect(m_close, &QToolButton::clicked, this, &TerminalFindBar::closeRequested);
  searchRow->addWidget(m_editor, 1);
  searchRow->addWidget(m_previous);
  searchRow->addWidget(m_next);
  searchRow->addWidget(m_close);
  refreshIcons();
  optionsRow->addWidget(m_caseSensitive);
  optionsRow->addWidget(m_regularExpression);
  optionsRow->addStretch(1);
  // Long refusal diagnostics wrap instead of widening the terminal window.
  m_status->setWordWrap(true);
  m_status->setMinimumWidth(0);
  optionsRow->addWidget(m_status, 1);
  layout->addLayout(searchRow);
  layout->addLayout(optionsRow);

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
  connect(m_caseSensitive, &QToolButton::toggled, this, criteriaChanged);
  connect(m_regularExpression, &QToolButton::toggled, this, criteriaChanged);
  hide();
}

void TerminalFindBar::refreshIcons() {
  if (!m_previous || !m_next || !m_close) return;
  const auto icon = [this](const QString &name) {
    QPixmap pixmap = QindaQt::Controls::applicationIcon(name).pixmap(48, 48);
    // Essential icon-only controls use semantic ink in every custom theme.
    // Keep the SVG silhouette while avoiding a fixed decorative hue at 2:1.
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), palette().color(QPalette::WindowText));
    painter.end();
    return QIcon(pixmap);
  };
  m_previous->setIcon(icon(QStringLiteral("go-up-symbolic")));
  m_next->setIcon(icon(QStringLiteral("go-down-symbolic")));
  m_close->setIcon(icon(QStringLiteral("window-close-symbolic")));
}

void TerminalFindBar::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange)
    refreshIcons();
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
