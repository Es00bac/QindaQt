// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_backend.h"
#include "session/terminal_session_types.h"

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include <functional>
#include <memory>

namespace QindaQt::Apps::Terminal {

class ProcessMonitor;

// AGENT-CONTRACT: TerminalSession is the single owner of the terminal child's
// lifecycle. It composes a fresh TerminalSessionBackend per generation, owns
// exit truth through the injected ProcessMonitor, and guarantees bounded
// escalating teardown (master close, SIGTERM, SIGKILL to the captured process
// group) before any generation is released. Restart never reuses a backend or
// a PTY. All methods are GUI-thread; the object is not thread-safe by design.
class TerminalSession final : public QObject {
  Q_OBJECT

public:
  enum class State {
    Idle,             // No backend yet, or never started.
    Running,          // Child alive on its own PTY generation.
    Exited,           // Child reaped; backend still displays scrollback.
    ShuttingDown,     // Bounded teardown sequence in progress.
    ShutdownComplete, // Child confirmed gone; backend disposed.
    ShutdownFailed,   // Child survived SIGKILL; ownership retained (P1-2).
  };

  using BackendFactory =
      std::function<std::unique_ptr<TerminalSessionBackend>()>;

  // The monitor is injected, not owned; the factory must be callable at any
  // later time (restart). Bounds make the teardown sequence deterministic in
  // tests and human-scale in production.
  TerminalSession(BackendFactory backendFactory, ProcessMonitor *monitor,
                  TeardownBounds bounds, QObject *parent = nullptr);
  ~TerminalSession() override;

  // Starts the first (or, after a completed shutdown, a fresh) generation.
  // Returns true when a child is running; a start failure still publishes a
  // StartFailed sessionFinished and leaves the object restartable.
  [[nodiscard]] bool start(const TerminalLaunchRequest &request);

  // Teardown-then-start with the last successful request. Rejected while a
  // shutdown is already in flight, while a SIGKILL survivor is owned
  // (ShutdownFailed), or when nothing was ever started.
  [[nodiscard]] bool restart();

  // Begins the bounded teardown sequence. A pending restart is cancelled
  // (close must never launch a fresh child, P1-3). Refused while a SIGKILL
  // survivor is owned.
  void beginShutdown();

  [[nodiscard]] State state() const { return m_state; }
  [[nodiscard]] TerminalExitStatus lastExit() const { return m_lastExit; }
  [[nodiscard]] QWidget *terminalWidget() const;
  [[nodiscard]] const TerminalLaunchRequest &lastRequest() const {
    return m_request;
  }

  // Presentation-facing view operations. Each is a safe no-op when no
  // backend generation is active, so the window never needs the backend
  // object itself.
  void copySelectionToClipboard();
  void pasteClipboardToSession();
  void pastePrimarySelectionToSession();
  void selectAllInView();
  void clearView();

signals:
  void stateChanged(QindaQt::Apps::Terminal::TerminalSession::State state);
  void sessionFinished(
      const QindaQt::Apps::Terminal::TerminalExitStatus &status);
  void terminalWidgetChanged(QWidget *widget);
  // Emitted before the current backend destroys its view so the presentation
  // can detach it from layouts first. Direct connections only.
  void viewDisposalRequested();
  void selectionAvailable(bool hasSelection);
  void titleReceived(const QString &title);
  void shutdownFinished(bool clean, const QString &diagnostic);

private:
  void setState(State state);
  bool spawnGeneration();
  void publishExit(const TerminalExitStatus &status);
  void enterShutdownSequence(bool restartAfterwards);
  void completeShutdown(bool clean, const QString &diagnostic);
  void advanceShutdownPhase();
  void pollTick();

  BackendFactory m_backendFactory;
  ProcessMonitor *m_monitor = nullptr;
  TeardownBounds m_bounds;
  std::unique_ptr<TerminalSessionBackend> m_backend;
  QTimer m_pollTimer;
  QElapsedTimer m_phaseTimer;

  State m_state = State::Idle;
  TerminalLaunchRequest m_request;
  TerminalExitStatus m_lastExit;
  ProcessId m_childPid = 0;
  bool m_exitPublished = false;
  bool m_restartAfterShutdown = false;

  enum class ShutdownPhase { None, Close, Term, Kill };
  ShutdownPhase m_phase = ShutdownPhase::None;
};

} // namespace QindaQt::Apps::Terminal
