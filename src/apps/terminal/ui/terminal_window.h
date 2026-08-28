// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session.h"
#include "session/terminal_session_types.h"
#include "ui/terminal_appearance.h"

#include <QLabel>
#include <QMainWindow>
#include <QVBoxLayout>
#include <memory>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: TerminalWindow owns presentation only: actions, menus,
// status reporting, focus, and accessibility metadata. It never touches PTYs,
// processes, or the rendering library. The injected session keeps that
// ownership. Every window shortcut is Shift-modified on purpose: plain
// Ctrl+C/S/Q/A/Z key sequences must reach the child shell (readline job and
// flow control), so a window-level binding on them would be a functional
// regression, not a style choice.
class TerminalWindow final : public QMainWindow {
  Q_OBJECT

public:
  // The window takes ownership of the session. Presentation consumes the
  // appearance value only; theme selection stays with main().
  explicit TerminalWindow(std::unique_ptr<TerminalSession> session,
                          const TerminalViewAppearance &appearance,
                          QWidget *parent = nullptr);
  ~TerminalWindow() override;

  [[nodiscard]] TerminalSession *session() const { return m_session.get(); }

signals:
  // Emitted when the close path has requested shutdown and the application
  // may quit; never emitted before the session reached a terminal state, so
  // connecting QApplication::quit() here preserves the teardown guarantee.
  void closeShutdownFinished();

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void buildActions();
  void buildMenus();
  void buildStatusBar();
  void embedTerminalWidget(QWidget *widget);
  void updateStatusForState(TerminalSession::State state);
  void showExitStatus(const TerminalExitStatus &status);
  void reportShutdownOutcome(bool clean, const QString &diagnostic);
  void requestCloseShutdown();

  std::unique_ptr<TerminalSession> m_session;
  TerminalViewAppearance m_appearance;
  QWidget *m_terminalHolder = nullptr;
  QVBoxLayout *m_terminalLayout = nullptr;
  QWidget *m_terminalView = nullptr;
  QLabel *m_statusLabel = nullptr;
  QAction *m_restartAction = nullptr;
  QAction *m_copyAction = nullptr;
  QAction *m_pasteAction = nullptr;
  QAction *m_pasteSelectionAction = nullptr;
  QAction *m_selectAllAction = nullptr;
  QAction *m_clearAction = nullptr;
  QAction *m_quitAction = nullptr;
  bool m_quitRequested = false;
};

} // namespace QindaQt::Apps::Terminal
