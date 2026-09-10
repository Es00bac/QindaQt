// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "session/terminal_session_types.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Terminal {
struct TerminalViewAppearance;

// App-level launch inputs shared by every session the collection creates:
// the inherited environment and the CLI-resolved shell fallback (already
// validated by TerminalLaunchPolicy in main). Profiles with an empty
// shellProgram inherit this fallback verbatim.
struct TerminalSessionContext final {
  QStringList baseEnvironment;
  QString fallbackProgram;
  QStringList fallbackArguments;
  QString workingDirectory;

  [[nodiscard]] bool operator==(const TerminalSessionContext &) const = default;
};

// Pure request assembly: profile shell policy goes through the exact same
// TerminalLaunchPolicy::resolveShell gate as the CLI, so a profile can
// select among admitted commands but never widen them.
[[nodiscard]] ShellResolution resolveProfileLaunch(
    const TerminalProfile &profile, const QString &fallbackProgram,
    const QStringList &fallbackArguments, const QString &workingDirectory,
    const QStringList &baseEnvironment);

// Presentation-bound sanitization for child-published titles: control
// characters are removed, whitespace runs collapse to one space, the result
// is trimmed and cut to kMaxSessionTitleLength. Never fails; an empty
// result means the caller shows its own fallback label.
[[nodiscard]] QString sanitizeSessionTitle(const QString &rawTitle);
constexpr int kMaxSessionTitleLength = 128;

// AGENT-CONTRACT: TerminalSessionCollection owns the bounded list of
// sessions in one window (at most kMaxSessions) and every lifecycle rule
// that spans sessions: per-session close with the S0 refusal/cancel
// semantics intact, close-all (the window close/quit path) that finishes
// only when every child reached a terminal state, and deterministic forced
// teardown of every remaining child on destruction. Sessions are GUI-thread
// QObjects parented to the collection; sessionRemoved is emitted before the
// session is scheduled for deletion so presentation can detach views.
class TerminalSessionCollection final : public QObject {
  Q_OBJECT

public:
  static constexpr int kMaxSessions = 8;

  TerminalSessionCollection(TerminalSessionContext context,
                            TerminalSession::BackendFactory backendFactory,
                            ProcessMonitor *monitor, TeardownBounds bounds,
                            QObject *parent = nullptr);
  ~TerminalSessionCollection() override;

  struct AddResult final {
    TerminalSession *session = nullptr;
    QString diagnostic;
  };
  // Validates the profile, resolves its launch request through the launch
  // policy, creates the session, and starts it. A backend start failure still
  // adds the session so its diagnostic is visible and restartable. An invalid
  // profile, unresolvable command, or full list is refused with
  // sessionAddRejected and a null session.
  // Optional source must belong to this collection; snapshot its live directory
  // synchronously, falling back to its launch directory when unavailable.
  [[nodiscard]] AddResult addSession(const TerminalProfile &profile,
                                      const TerminalSession *inheritDirectoryFrom = nullptr);

  // S0 semantics per session: refused while a SIGKILL survivor is owned
  // (sessionCloseFailed); closing during a pending restart cancels it.
  // Closing the last session routes through requestCloseAll so the window
  // keeps exactly one quit path.
  void requestCloseSession(TerminalSession *session);
  // The window close/quit path: begins shutdown on every session. Emits
  // allSessionsClosed(clean) once every session is removed or retained as
  // a refused SIGKILL survivor; clean is false when any survivor remains.
  void requestCloseAll();

  void moveSession(TerminalSession *session, int newIndex);

  [[nodiscard]] int count() const;
  void setAppearance(const TerminalViewAppearance &appearance);
  [[nodiscard]] TerminalSession *sessionAt(int index) const;
  [[nodiscard]] int indexOf(const TerminalSession *session) const;
  [[nodiscard]] const TerminalSessionContext &context() const {
    return m_context;
  }
  // AGENT-CONTRACT: session restore rewrites the inherited working directory
  // exactly once, after the Settings1 baseline and before the first
  // addSession. Calling it afterwards silently changes what a later session
  // (Restart, New Window inheritance) would use, so it is not a per-session
  // override and presentation code must never call it.
  void setFallbackWorkingDirectory(const QString &workingDirectory) {
    m_context.workingDirectory = workingDirectory;
  }

signals:
  void sessionAdded(QindaQt::Apps::Terminal::TerminalSession *session);
  // Emitted after the session left the list and before its deletion is
  // scheduled; the pointer must not be dereferenced afterwards.
  void sessionRemoved(QindaQt::Apps::Terminal::TerminalSession *session);
  void sessionMoved(QindaQt::Apps::Terminal::TerminalSession *session,
                    int newIndex);
  void sessionTitleChanged(QindaQt::Apps::Terminal::TerminalSession *session,
                           const QString &title);
  void sessionAddRejected(const QString &diagnostic);
  void sessionCloseFailed(QindaQt::Apps::Terminal::TerminalSession *session,
                          const QString &diagnostic);
  void allSessionsClosed(bool clean, const QString &diagnostic);

private:
  void wireSession(TerminalSession *session);
  void handleShutdownFinished(TerminalSession *session, bool clean,
                              const QString &diagnostic);
  void finishCloseAllIfSettled();
  void removeSession(TerminalSession *session);

  TerminalSessionContext m_context;
  TerminalSession::BackendFactory m_backendFactory;
  ProcessMonitor *m_monitor = nullptr;
  TeardownBounds m_bounds;
  QList<TerminalSession *> m_sessions;
  bool m_closeAllInProgress = false;
  bool m_closeAllFailed = false;
  QString m_closeAllDiagnostic;
};

} // namespace QindaQt::Apps::Terminal
