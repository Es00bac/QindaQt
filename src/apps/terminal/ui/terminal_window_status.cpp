// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "ui/terminal_profile_apply_status.h"

#include <QAccessible>
#include <QLabel>

namespace QindaQt::Apps::Terminal {
namespace {

[[nodiscard]] QString signalName(int signalNumber) {
  switch (signalNumber) {
  case 1: return QStringLiteral("SIGHUP");
  case 2: return QStringLiteral("SIGINT");
  case 3: return QStringLiteral("SIGQUIT");
  case 4: return QStringLiteral("SIGILL");
  case 6: return QStringLiteral("SIGABRT");
  case 8: return QStringLiteral("SIGFPE");
  case 9: return QStringLiteral("SIGKILL");
  case 10: return QStringLiteral("SIGUSR1");
  case 11: return QStringLiteral("SIGSEGV");
  case 12: return QStringLiteral("SIGUSR2");
  case 13: return QStringLiteral("SIGPIPE");
  case 14: return QStringLiteral("SIGALRM");
  case 15: return QStringLiteral("SIGTERM");
  default: return QStringLiteral("signal %1").arg(signalNumber);
  }
}

} // namespace

void TerminalWindow::updateStatusForState(TerminalSession::State state) {
  QString text;
  QPalette palette = m_appearance.windowPalette;
  switch (state) {
  case TerminalSession::State::Idle:
    text = QStringLiteral("No session");
    break;
  case TerminalSession::State::Running:
    text = QStringLiteral("Session running");
    break;
  case TerminalSession::State::Exited:
    // The typed exit status and the Exited state arrive in the same tick;
    // rendering the generic state text here would overwrite the
    // code/signal/unknown detail before the user can read it.
    if (m_activeSession != nullptr &&
        m_activeSession->lastExit().kind != TerminalExitStatus::Kind::None) {
      showExitStatus(m_activeSession->lastExit());
      updateViewActionStates();
      return;
    }
    text = QStringLiteral("Session ended");
    break;
  case TerminalSession::State::ShuttingDown:
    text = QStringLiteral("Closing session…");
    break;
  case TerminalSession::State::ShutdownComplete:
    text = QStringLiteral("Session closed");
    break;
  case TerminalSession::State::ShutdownFailed:
    text = QStringLiteral("Session close failed");
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  }
  showStatusMessage(text, false, palette);
  updateViewActionStates();
}

void TerminalWindow::showExitStatus(const TerminalExitStatus &status) {
  QString text;
  QPalette palette = m_appearance.windowPalette;
  switch (status.kind) {
  case TerminalExitStatus::Kind::Normal:
    text = QStringLiteral("Session exited (code %1)").arg(status.code);
    break;
  case TerminalExitStatus::Kind::Signal:
    text =
        QStringLiteral("Session terminated by %1").arg(signalName(status.code));
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::UnknownExit:
    // Another reaper consumed the status; the truth is "exited, code
    // unknown", never a fabricated normal status.
    text = QStringLiteral("Session exited (status unknown)");
    palette.setColor(QPalette::WindowText,
                     m_appearance.statusWarningForeground);
    break;
  case TerminalExitStatus::Kind::StartFailed:
    text = QStringLiteral("Error: %1").arg(status.diagnostic);
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::None:
    return;
  }
  showStatusMessage(text, false, palette);
}

void TerminalWindow::presentProfileApplyResult(const QVariantList &ledger) {
  const TerminalProfileApplyStatus status = terminalProfileApplyStatus(ledger);
  QPalette palette = m_appearance.windowPalette;
  if (status.severity == TerminalProfileApplySeverity::Warning) {
    palette.setColor(QPalette::WindowText,
                     m_appearance.statusWarningForeground);
  } else if (status.severity == TerminalProfileApplySeverity::Error) {
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
  }
  showStatusMessage(status.text, false, palette);
}

void TerminalWindow::showStatusMessage(const QString &text, bool danger) {
  QPalette palette = m_appearance.windowPalette;
  if (danger) {
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
  }
  showStatusMessage(text, danger, palette);
}

void TerminalWindow::showStatusMessage(const QString &text, bool,
                                       const QPalette &palette) {
  if (m_statusLabel == nullptr) {
    return;
  }
  m_statusLabel->setText(text);
  m_statusLabel->setPalette(palette);
  // AGENT-CONTRACT: Every visible status change updates and announces the
  // screen-reader name; search and selected-link truth must not rely on color.
  m_statusLabel->setAccessibleName(
      QStringLiteral("Session status: %1").arg(text));
  QAccessibleEvent event(m_statusLabel, QAccessible::NameChanged);
  QAccessible::updateAccessibility(&event);
}

} // namespace QindaQt::Apps::Terminal
