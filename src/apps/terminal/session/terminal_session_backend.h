// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_types.h"

#include <QObject>
#include <QWidget>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: TerminalSessionBackend is the boundary between the session
// lifecycle (policy, state machine, teardown) and the rendering adapter that
// wraps qtermwidget6 (ADR-0030). Implementations live only in the view
// adapter and in test fakes; no implementation may leak widget-library types
// through this interface. A backend instance is single-use: one start, one
// child. start() must be called at most once; requestShutdown() may be called
// from any session state and must not throw. The GUI-thread affinity of the
// widget means every call happens on the constructing thread.
class TerminalSessionBackend : public QObject {
  Q_OBJECT

public:
  struct StartOutcome final {
    bool ok = false;
    QString diagnostic;

    [[nodiscard]] bool operator==(const StartOutcome &) const = default;
  };

  explicit TerminalSessionBackend(QObject *parent = nullptr);
  ~TerminalSessionBackend() override;

  // Starts the child described by the request on a fresh PTY. Returns a typed
  // failure when the PTY or child cannot be created; the diagnostic is
  // bounded, single-line, and user-presentable.
  [[nodiscard]] virtual StartOutcome start(const TerminalLaunchRequest &request) = 0;

  // Closes the PTY master (delivering SIGHUP to the child's session) and
  // disposes the terminal view. After this call terminalWidget() returns
  // nullptr and the backend keeps answering shellProcessId() so the session
  // can finish bounded escalation.
  virtual void requestShutdown() = 0;

  // The child PID; valid only after a successful start and before the child
  // is reaped. Never returns a recycled PID: the value is captured at fork.
  [[nodiscard]] virtual ProcessId shellProcessId() const = 0;

  // The embedded rendering surface, or nullptr before start / after
  // requestShutdown. Ownership stays with the backend.
  [[nodiscard]] virtual QWidget *terminalWidget() = 0;

  virtual void copySelectionToClipboard() = 0;
  virtual void pasteClipboardToSession() = 0;
  virtual void pastePrimarySelectionToSession() = 0;
  virtual void selectAllInView() = 0;
  virtual void clearView() = 0;
  [[nodiscard]] virtual bool hasSelectedText() const = 0;
  virtual void sendTextToSession(const QString &text) = 0;

signals:
  // Published exactly once per backend when the child's exit is first known.
  void sessionFinished(
      const QindaQt::Apps::Terminal::TerminalExitStatus &status);
  void selectionChanged(bool hasSelection);
  void titleChanged(const QString &title);
};

} // namespace QindaQt::Apps::Terminal
