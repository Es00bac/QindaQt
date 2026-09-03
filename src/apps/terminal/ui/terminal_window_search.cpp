// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "ui/terminal_find_bar.h"

#include <QShortcut>

namespace QindaQt::Apps::Terminal {

void TerminalWindow::buildFindBar() {
  m_findBar = new TerminalFindBar(this);
  connect(m_findBar, &TerminalFindBar::searchRequested, this,
          &TerminalWindow::runSearch);
  connect(m_findBar, &TerminalFindBar::closeRequested, this,
          &TerminalWindow::closeFindBar);
  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_findBar);
  escape->setContext(Qt::WidgetWithChildrenShortcut);
  connect(escape, &QShortcut::activated, this, &TerminalWindow::closeFindBar);
}

void TerminalWindow::showFindBar() {
  if (m_activeSession == nullptr || m_activeSession->terminalWidget() == nullptr) {
    return;
  }
  m_findVisibleBySession.insert(m_activeSession, true);
  m_findBar->setQuery(m_searchBySession.value(m_activeSession));
  m_findBar->show();
  m_findBar->focusEditor();
}

void TerminalWindow::closeFindBar() {
  if (m_activeSession != nullptr) {
    m_findVisibleBySession.insert(m_activeSession, false);
    m_activeSession->clearScrollbackSearch();
  }
  m_findBar->hide();
  if (m_terminalView != nullptr) {
    m_terminalView->setFocus(Qt::ShortcutFocusReason);
  }
}

void TerminalWindow::runSearch(TerminalSearchDirection direction) {
  if (m_activeSession == nullptr || m_activeSession->terminalWidget() == nullptr) {
    return;
  }
  const bool findBarShown = !m_findBar->isHidden();
  TerminalSearchQuery query = findBarShown
                                  ? m_findBar->query()
                                  : m_searchBySession.value(m_activeSession);
  if (query.pattern.isEmpty() && !findBarShown) {
    showFindBar();
    return;
  }
  const TerminalSearchQuery previous = m_searchBySession.value(m_activeSession);
  if (query != previous) {
    direction = TerminalSearchDirection::Initial;
  }
  m_searchBySession.insert(m_activeSession, query);
  const TerminalSearchResult result =
      m_activeSession->searchScrollback(query, direction);
  if (findBarShown) {
    m_findBar->presentResult(result);
  }
  updateViewActionStates();
}

void TerminalWindow::restoreSearchPresentation() {
  if (m_activeSession == nullptr) {
    m_findBar->hide();
    return;
  }
  m_findBar->setQuery(m_searchBySession.value(m_activeSession));
  m_findBar->setVisible(m_findVisibleBySession.value(m_activeSession, false));
}

} // namespace QindaQt::Apps::Terminal
