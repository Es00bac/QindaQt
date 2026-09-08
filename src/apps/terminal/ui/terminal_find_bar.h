// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "search/terminal_search.h"

#include <QWidget>

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
  // Refresh GUI-created symbolic pixmaps after icon-theme changes, even when
  // the semantic palette is unchanged.
  void refreshIcons();

signals:
  void searchRequested(QindaQt::Apps::Terminal::TerminalSearchDirection direction);
  void closeRequested();

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  QToolButton *m_close = nullptr;
  QLineEdit *m_editor = nullptr;
  QToolButton *m_caseSensitive = nullptr;
  QToolButton *m_regularExpression = nullptr;
  QToolButton *m_previous = nullptr;
  QToolButton *m_next = nullptr;
  QLabel *m_status = nullptr;
};

} // namespace QindaQt::Apps::Terminal
