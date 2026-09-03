// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "links/terminal_link_opener.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>

namespace QindaQt::Apps::Terminal {

void TerminalWindow::selectRelativeLink(int delta) {
  if (m_activeSession == nullptr) {
    return;
  }
  presentLinkSelection(m_activeSession->selectVisibleLink(delta));
}

void TerminalWindow::presentLinkSelection(
    const TerminalLinkSelection &selection) {
  if (m_activeSession == nullptr) {
    return;
  }
  m_linkBySession.insert(m_activeSession, selection);
  if (!selection.found) {
    showStatusMessage(QStringLiteral("No links in visible terminal output"),
                      false);
  } else {
    const QString status =
        QStringLiteral("Link %1 of %2%3: %4")
            .arg(selection.current)
            .arg(selection.total)
            .arg(selection.wrapped ? QStringLiteral(" (wrapped)") : QString())
            .arg(selection.link.display);
    showStatusMessage(status.left(2300), false);
    m_linkCopyAction->setToolTip(selection.link.tooltip);
    m_linkOpenAction->setToolTip(selection.link.tooltip);
  }
  updateViewActionStates();
}

void TerminalWindow::copyCurrentLink() {
  if (m_activeSession == nullptr) {
    return;
  }
  const TerminalLinkSelection cached = m_linkBySession.value(m_activeSession);
  const TerminalLinkSelection selection = m_activeSession->currentVisibleLink();
  presentLinkSelection(selection);
  if (!selection.found) {
    return;
  }
  // AGENT-GUARD: Child output and scrolling can change after traversal.
  // Never copy a cached target unless the current viewport still reports the
  // same selection; one stale activation must only refresh truth (review P2-2).
  if (cached.found && cached.link != selection.link) {
    return;
  }
  QApplication::clipboard()->setText(selection.link.target);
  showStatusMessage(QStringLiteral("Copied link: %1").arg(selection.link.display)
                        .left(2300),
                    false);
}

void TerminalWindow::openCurrentLink() {
  if (m_activeSession == nullptr || m_linkOpener == nullptr) {
    showStatusMessage(QStringLiteral("Link opener is unavailable"), true);
    return;
  }
  const TerminalLinkSelection cached = m_linkBySession.value(m_activeSession);
  const TerminalLinkSelection selection = m_activeSession->currentVisibleLink();
  presentLinkSelection(selection);
  if (!selection.found) {
    return;
  }
  if (cached.found && cached.link != selection.link) {
    return;
  }
  const TerminalLinkOpenResult result = m_linkOpener->open(selection.link, this);
  if (result.started) {
    showStatusMessage(
        QStringLiteral("Opened target: %1").arg(selection.link.display).left(2300),
        false);
  } else {
    showStatusMessage(result.diagnostic, !result.cancelled);
  }
}

void TerminalWindow::showLinkContextMenu(const QPoint &globalPosition) {
  if (m_activeSession == nullptr) {
    return;
  }
  presentLinkSelection(m_activeSession->currentVisibleLink());
  auto *menu = new QMenu(this);
  menu->setObjectName(QStringLiteral("terminalLinkContextMenu"));
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addAction(m_linkPreviousAction);
  menu->addAction(m_linkNextAction);
  menu->addSeparator();
  menu->addAction(m_linkCopyAction);
  menu->addAction(m_linkOpenAction);
  if (isVisible()) {
    menu->popup(globalPosition);
  }
}

} // namespace QindaQt::Apps::Terminal
