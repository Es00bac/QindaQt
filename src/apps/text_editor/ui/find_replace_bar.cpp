// SPDX-License-Identifier: GPL-3.0-or-later
#include "find_replace_bar.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace QindaQt::Apps::TextEditor {

FindReplaceBar::FindReplaceBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("findReplaceBar"));
  setAccessibleName(tr("Find and replace"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);

  auto *findRow = new QHBoxLayout;
  auto *findLabel = new QLabel(tr("&Find:"), this);
  m_find = new QLineEdit(this);
  m_find->setObjectName(QStringLiteral("findPatternEditor"));
  m_find->setAccessibleName(tr("Find text"));
  m_find->setMaxLength(FindReplaceEngine::maximumPatternLength);
  findLabel->setBuddy(m_find);
  auto *previous = new QPushButton(tr("Previous"), this);
  previous->setObjectName(QStringLiteral("findPreviousButton"));
  previous->setAccessibleName(tr("Find previous match"));
  auto *next = new QPushButton(tr("Next"), this);
  next->setObjectName(QStringLiteral("findNextButton"));
  next->setAccessibleName(tr("Find next match"));
  auto *close = new QPushButton(tr("Close"), this);
  close->setObjectName(QStringLiteral("findCloseButton"));
  close->setAccessibleName(tr("Close find and replace"));
  findRow->addWidget(findLabel);
  findRow->addWidget(m_find, 1);
  findRow->addWidget(previous);
  findRow->addWidget(next);
  findRow->addWidget(close);
  layout->addLayout(findRow);

  m_replaceFields = new QWidget(this);
  auto *replaceRow = new QHBoxLayout(m_replaceFields);
  replaceRow->setContentsMargins(0, 0, 0, 0);
  auto *replaceLabel = new QLabel(tr("&Replace:"), m_replaceFields);
  m_replace = new QLineEdit(m_replaceFields);
  m_replace->setObjectName(QStringLiteral("replaceTextEditor"));
  m_replace->setAccessibleName(tr("Replacement text"));
  m_replace->setMaxLength(4096);
  replaceLabel->setBuddy(m_replace);
  auto *replace = new QPushButton(tr("Replace"), m_replaceFields);
  replace->setObjectName(QStringLiteral("replaceButton"));
  auto *replaceAll = new QPushButton(tr("Replace All"), m_replaceFields);
  replaceAll->setObjectName(QStringLiteral("replaceAllButton"));
  replaceRow->addWidget(replaceLabel);
  replaceRow->addWidget(m_replace, 1);
  replaceRow->addWidget(replace);
  replaceRow->addWidget(replaceAll);
  layout->addWidget(m_replaceFields);

  auto *optionsRow = new QHBoxLayout;
  m_caseSensitive = new QCheckBox(tr("Match &case"), this);
  m_caseSensitive->setObjectName(QStringLiteral("findCaseSensitive"));
  m_wholeWord = new QCheckBox(tr("&Whole word"), this);
  m_wholeWord->setObjectName(QStringLiteral("findWholeWord"));
  m_regex = new QCheckBox(tr("Regular e&xpression"), this);
  m_regex->setObjectName(QStringLiteral("findRegex"));
  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("findStatus"));
  m_status->setAccessibleName(tr("Find result"));
  optionsRow->addWidget(m_caseSensitive);
  optionsRow->addWidget(m_wholeWord);
  optionsRow->addWidget(m_regex);
  optionsRow->addStretch(1);
  optionsRow->addWidget(m_status);
  layout->addLayout(optionsRow);

  connect(next, &QPushButton::clicked, this,
          &FindReplaceBar::findNextRequested);
  connect(previous, &QPushButton::clicked, this,
          &FindReplaceBar::findPreviousRequested);
  connect(replace, &QPushButton::clicked, this,
          &FindReplaceBar::replaceRequested);
  connect(replaceAll, &QPushButton::clicked, this,
          &FindReplaceBar::replaceAllRequested);
  connect(close, &QPushButton::clicked, this, &FindReplaceBar::closeBar);
  connect(m_find, &QLineEdit::returnPressed, this,
          &FindReplaceBar::findNextRequested);
  hide();
}

FindOptions FindReplaceBar::options() const {
  return {.pattern = m_find->text(),
          .caseSensitive = m_caseSensitive->isChecked(),
          .wholeWord = m_wholeWord->isChecked(),
          .regularExpression = m_regex->isChecked()};
}

QString FindReplaceBar::replacement() const { return m_replace->text(); }
QLineEdit *FindReplaceBar::findEditor() const { return m_find; }
QLineEdit *FindReplaceBar::replaceEditor() const { return m_replace; }

void FindReplaceBar::open(const bool replaceMode) {
  m_replaceFields->setVisible(replaceMode);
  show();
  m_find->setFocus(Qt::ShortcutFocusReason);
  m_find->selectAll();
}

void FindReplaceBar::closeBar() {
  hide();
  emit closed();
}

void FindReplaceBar::setStatus(const QString &text, const bool announce) {
  const QString bounded = text.left(256);
  if (m_status->text() == bounded) {
    return;
  }
  m_status->setText(bounded);
  m_status->setAccessibleDescription(bounded);
  if (announce && !bounded.isEmpty()) {
    QAccessibleAnnouncementEvent event(m_status, bounded);
    event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&event);
  }
}

} // namespace QindaQt::Apps::TextEditor
