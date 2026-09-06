// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_session_collection.h"

#include "session/process_liveness.h"

#include <utility>

namespace QindaQt::Apps::Terminal {

void TerminalSessionCollection::setAppearance(
    const TerminalViewAppearance &appearance) {
  for (TerminalSession *session : std::as_const(m_sessions)) {
    session->setAppearance(appearance);
  }
}

ShellResolution resolveProfileLaunch(const TerminalProfile &profile,
                                     const QString &fallbackProgram,
                                     const QStringList &fallbackArguments,
                                     const QString &workingDirectory,
                                     const QStringList &baseEnvironment) {
  const QString &program =
      profile.shellProgram.isEmpty() ? fallbackProgram : profile.shellProgram;
  const QStringList &arguments = profile.shellProgram.isEmpty()
                                     ? fallbackArguments
                                     : profile.shellArguments;
  auto resolution = TerminalLaunchPolicy::resolveShell(
      program, arguments, workingDirectory, baseEnvironment);
  if (!resolution.outcome.ok) {
    return resolution;
  }
  // The child environment is derived per session exactly like the S0 CLI
  // path: hostile entries dropped, TERM/COLORTERM forced, UTF-8 locale
  // precedence applied.
  const auto environment =
      TerminalLaunchPolicy::childEnvironment(baseEnvironment);
  if (!environment.outcome.ok) {
    return {.outcome = environment.outcome, .request = {}};
  }
  resolution.request.environment = environment.environment;
  return resolution;
}

QString sanitizeSessionTitle(const QString &rawTitle) {
  QString sanitized;
  sanitized.reserve(qMin(rawTitle.size(), kMaxSessionTitleLength + 1));
  bool pendingSpace = false;
  for (qsizetype index = 0; index < rawTitle.size(); ++index) {
    const QChar character = rawTitle.at(index);
    if (character.isSpace()) {
      pendingSpace = !sanitized.isEmpty();
      continue;
    }
    if (character.isHighSurrogate()) {
      if (index + 1 >= rawTitle.size() ||
          !rawTitle.at(index + 1).isLowSurrogate()) {
        continue;
      }
      if (pendingSpace && !sanitized.isEmpty()) {
        sanitized.append(QLatin1Char(' '));
      }
      pendingSpace = false;
      if (sanitized.size() + 2 > kMaxSessionTitleLength) {
        break;
      }
      sanitized.append(character);
      sanitized.append(rawTitle.at(++index));
      continue;
    }
    if (character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format ||
        character.isLowSurrogate()) {
      continue;
    }
    if (pendingSpace) {
      sanitized.append(QLatin1Char(' '));
      pendingSpace = false;
    }
    sanitized.append(character);
    if (sanitized.size() >= kMaxSessionTitleLength) {
      break;
    }
  }
  return sanitized.trimmed();
}

TerminalSessionCollection::TerminalSessionCollection(
    TerminalSessionContext context,
    TerminalSession::BackendFactory backendFactory, ProcessMonitor *monitor,
    TeardownBounds bounds, QObject *parent)
    : QObject(parent), m_context(std::move(context)),
      m_backendFactory(std::move(backendFactory)), m_monitor(monitor),
      m_bounds(bounds) {}

TerminalSessionCollection::~TerminalSessionCollection() {
  // AGENT-GUARD: destruction (window teardown, process exit) must never
  // leave a child running. Each retained session's destructor performs the
  // forced synchronous escalation (backend close, then SIGTERM/SIGKILL to
  // the captured process group), which is the documented forced-destruction
  // path; beginShutdown() remains the only bounded, wait-confirmed route.
  while (!m_sessions.isEmpty()) {
    delete m_sessions.takeLast();
  }
}

TerminalSessionCollection::AddResult
TerminalSessionCollection::addSession(const TerminalProfile &profile) {
  if (m_sessions.size() >= kMaxSessions) {
    const QString diagnostic =
        QStringLiteral("Session limit reached (%1 tabs)").arg(kMaxSessions);
    emit sessionAddRejected(diagnostic);
    return {.session = nullptr, .diagnostic = diagnostic};
  }
  const ProfileValidation validation = validateTerminalProfile(profile);
  if (!validation.ok) {
    emit sessionAddRejected(validation.diagnostic);
    return {.session = nullptr, .diagnostic = validation.diagnostic};
  }
  const auto resolution = resolveProfileLaunch(
      profile, m_context.fallbackProgram, m_context.fallbackArguments,
      m_context.workingDirectory, m_context.baseEnvironment);
  if (!resolution.outcome.ok) {
    emit sessionAddRejected(resolution.outcome.diagnostic);
    return {.session = nullptr, .diagnostic = resolution.outcome.diagnostic};
  }

  auto *session =
      new TerminalSession(m_backendFactory, m_monitor, m_bounds, this);
  wireSession(session);
  m_sessions.append(session);
  emit sessionAdded(session);
  // A typed start failure (PTY/channel/theme) still adds the session: the
  // diagnostic is its exit status, visible in the tab, and Restart applies.
  static_cast<void>(session->start(resolution.request, profile));
  return {.session = session, .diagnostic = {}};
}

void TerminalSessionCollection::requestCloseSession(TerminalSession *session) {
  if (session == nullptr || !m_sessions.contains(session)) {
    return;
  }
  if (m_sessions.size() == 1) {
    // One quit path: the last tab close is the window close.
    requestCloseAll();
    return;
  }
  if (session->state() == TerminalSession::State::ShutdownFailed) {
    emit sessionCloseFailed(
        session, QStringLiteral("Session child survived teardown; close is "
                                "refused"));
    return;
  }
  if (session->state() == TerminalSession::State::ShuttingDown) {
    // Already closing; the in-flight shutdownFinished drives removal.
    return;
  }
  session->beginShutdown();
}

void TerminalSessionCollection::requestCloseAll() {
  m_closeAllInProgress = true;
  m_closeAllFailed = false;
  m_closeAllDiagnostic.clear();
  if (m_sessions.isEmpty()) {
    finishCloseAllIfSettled();
    return;
  }
  for (TerminalSession *session : std::as_const(m_sessions)) {
    if (session->state() == TerminalSession::State::ShutdownFailed) {
      // Refused up front; the survivor stays owned (P1-2).
      m_closeAllFailed = true;
      if (m_closeAllDiagnostic.isEmpty()) {
        m_closeAllDiagnostic =
            QStringLiteral("A session child survived teardown; close is "
                           "refused for that session");
      }
      continue;
    }
    session->beginShutdown();
  }
  finishCloseAllIfSettled();
}

void TerminalSessionCollection::moveSession(TerminalSession *session,
                                            int newIndex) {
  const int from = static_cast<int>(m_sessions.indexOf(session));
  if (from < 0 || newIndex < 0 ||
      newIndex >= static_cast<int>(m_sessions.size()) || from == newIndex) {
    return;
  }
  m_sessions.move(from, newIndex);
  emit sessionMoved(session, newIndex);
}

int TerminalSessionCollection::count() const {
  return static_cast<int>(m_sessions.size());
}

TerminalSession *TerminalSessionCollection::sessionAt(int index) const {
  return index >= 0 && index < m_sessions.size() ? m_sessions.at(index)
                                                 : nullptr;
}

int TerminalSessionCollection::indexOf(const TerminalSession *session) const {
  return static_cast<int>(
      m_sessions.indexOf(const_cast<TerminalSession *>(session)));
}

void TerminalSessionCollection::wireSession(TerminalSession *session) {
  connect(session, &TerminalSession::titleReceived, this,
          [this, session](const QString &title) {
            emit sessionTitleChanged(session, sanitizeSessionTitle(title));
          });
  connect(session, &TerminalSession::shutdownFinished, this,
          [this, session](bool clean, const QString &diagnostic) {
            handleShutdownFinished(session, clean, diagnostic);
          });
}

void TerminalSessionCollection::handleShutdownFinished(
    TerminalSession *session, bool clean, const QString &diagnostic) {
  if (!clean) {
    // P1-2: the SIGKILL survivor stays owned and in the list.
    if (m_closeAllInProgress) {
      m_closeAllFailed = true;
      if (m_closeAllDiagnostic.isEmpty()) {
        m_closeAllDiagnostic = diagnostic;
      }
    }
    emit sessionCloseFailed(session, diagnostic);
    finishCloseAllIfSettled();
    return;
  }
  removeSession(session);
  finishCloseAllIfSettled();
}

void TerminalSessionCollection::finishCloseAllIfSettled() {
  if (!m_closeAllInProgress) {
    return;
  }
  bool anyPending = false;
  for (const TerminalSession *session : std::as_const(m_sessions)) {
    const auto state = session->state();
    if (state != TerminalSession::State::ShutdownComplete &&
        state != TerminalSession::State::ShutdownFailed) {
      anyPending = true;
      break;
    }
  }
  if (anyPending) {
    return;
  }
  m_closeAllInProgress = false;
  emit allSessionsClosed(!m_closeAllFailed, m_closeAllDiagnostic);
}

void TerminalSessionCollection::removeSession(TerminalSession *session) {
  if (!m_sessions.removeOne(session)) {
    return;
  }
  emit sessionRemoved(session);
  // Deferred deletion: we are inside the session's own signal emission, and
  // the window needs the queued event loop to detach the view first.
  session->deleteLater();
}

} // namespace QindaQt::Apps::Terminal
