// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "search/terminal_search.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;

namespace QindaQt::Apps::Terminal {

class TerminalFindBar final : public QWidget {
  Q_OBJECT

public:
  explicit TerminalFindBar(QWidget *parent = nullptr);

  [[nodiscard]] TerminalSearchQuery query() const;
  void setQuery(const TerminalSearchQuery &query);
  void presentResult(const TerminalSearchResult &result);
  void focusEditor();

signals:
  void searchRequested(QindaQt::Apps::Terminal::TerminalSearchDirection direction);
  void closeRequested();

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  QLineEdit *m_editor = nullptr;
  QCheckBox *m_caseSensitive = nullptr;
  QCheckBox *m_regularExpression = nullptr;
  QToolButton *m_previous = nullptr;
  QToolButton *m_next = nullptr;
  QLabel *m_status = nullptr;
};

} // namespace QindaQt::Apps::Terminal
