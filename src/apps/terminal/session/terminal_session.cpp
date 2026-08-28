// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_session.h"

#include "session/process_liveness.h"

#include <csignal>

namespace QindaQt::Apps::Terminal {

TerminalSession::TerminalSession(BackendFactory backendFactory,
                                 ProcessMonitor *monitor,
                                 TeardownBounds bounds, QObject *parent)
    : QObject(parent), m_backendFactory(std::move(backendFactory)),
      m_monitor(monitor), m_bounds(bounds) {
  m_pollTimer.setInterval(m_bounds.pollIntervalMs);
  connect(&m_pollTimer, &QTimer::timeout, this, &TerminalSession::pollTick);
}

TerminalSession::~TerminalSession() {
  // AGENT-GUARD: Destruction must never silently skip teardown, including a
  // session already inside beginShutdown()'s bounded sequence or one that
  // failed it (P1-2: a SIGKILL survivor stays owned until it is gone). The
  // backend destructor only closes the PTY master (SIGHUP), so an ignoring
  // child or grandchild could survive; whenever a captured child may still
  // be alive, dispose the backend and then finish the escalation
  // synchronously against the captured process group. Bounded waits are
  // impossible in a destructor, so beginShutdown() remains the only
  // guaranteed-complete route and presentation code must prefer it on window
  // close; this guard exists for forced destruction paths only.
  const bool childMayBeAlive = m_state == State::Running ||
                               m_state == State::ShuttingDown ||
                               m_state == State::ShutdownFailed;
  if (childMayBeAlive && m_backend != nullptr) {
    const ProcessId pid = m_backend->shellProcessId();
    m_backend->requestShutdown();
    if (m_monitor != nullptr && pid > 0 &&
        m_monitor->reap(pid).state != ProcessState::Exited) {
      m_monitor->signalProcessGroup(pid, SIGTERM);
      m_monitor->signalProcessGroup(pid, SIGKILL);
    }
  }
}

void TerminalSession::setState(State state) {
  if (m_state == state) {
    return;
  }
  m_state = state;
  emit stateChanged(m_state);
}

QWidget *TerminalSession::terminalWidget() const {
  return m_backend != nullptr ? m_backend->terminalWidget() : nullptr;
}

void TerminalSession::copySelectionToClipboard() {
  if (m_backend != nullptr) {
    m_backend->copySelectionToClipboard();
  }
}

void TerminalSession::pasteClipboardToSession() {
  if (m_backend != nullptr) {
    m_backend->pasteClipboardToSession();
  }
}

void TerminalSession::pastePrimarySelectionToSession() {
  if (m_backend != nullptr) {
    m_backend->pastePrimarySelectionToSession();
  }
}

void TerminalSession::selectAllInView() {
  if (m_backend != nullptr) {
    m_backend->selectAllInView();
  }
}

void TerminalSession::clearView() {
  if (m_backend != nullptr) {
    m_backend->clearView();
  }
}

bool TerminalSession::start(const TerminalLaunchRequest &request) {
  // ShutdownFailed means a child may still be alive; only restart()'s
  // escalation path may retire that generation. Exited generations were
  // already reaped, so their views can be disposed synchronously.
  if (m_state == State::ShuttingDown || m_state == State::Running ||
      m_state == State::ShutdownFailed) {
    return false;
  }
  if (request.program.isEmpty()) {
    // A rejected start is a terminal generation outcome, not a silent
    // return to Idle: the published StartFailed status and the state must
    // agree, and the session must stay restartable.
    publishExit({TerminalExitStatus::Kind::StartFailed, 0,
                 QStringLiteral("No shell program was resolved")});
    setState(State::Exited);
    return false;
  }
  if (m_backend != nullptr) {
    emit viewDisposalRequested();
    m_backend.reset();
  }
  m_request = request;
  return spawnGeneration();
}

bool TerminalSession::restart() {
  if (m_state == State::ShuttingDown || m_state == State::ShutdownFailed) {
    // While shutting down a second lifecycle request races the escalation;
    // with a SIGKILL survivor the generation is retained and owned (P1-2),
    // so replacing it would silently abandon the survivor.
    return false;
  }
  if (m_request.program.isEmpty()) {
    publishExit({TerminalExitStatus::Kind::StartFailed, 0,
                 QStringLiteral("No previous session to restart")});
    setState(State::Exited);
    return false;
  }
  enterShutdownSequence(true);
  return true;
}

void TerminalSession::beginShutdown() {
  if (m_state == State::ShuttingDown) {
    // P1-3: closing during a pending restart must cancel that restart, or
    // completeShutdown() would spawn a fresh child right before the queued
    // application quit destroys it.
    m_restartAfterShutdown = false;
    return;
  }
  if (m_state == State::ShutdownFailed) {
    // P1-2: a SIGKILL survivor stays owned; re-running the escalation is
    // refused instead of pretending the survivor can be released.
    return;
  }
  enterShutdownSequence(false);
}

bool TerminalSession::spawnGeneration() {
  m_exitPublished = false;
  m_lastExit = {};
  m_backend = m_backendFactory();
  if (m_backend == nullptr) {
    publishExit({TerminalExitStatus::Kind::StartFailed, 0,
                 QStringLiteral("Terminal view could not be created")});
    setState(State::Exited);
    return false;
  }

  const auto started = m_backend->start(m_request);
  if (!started.ok) {
    const QString diagnostic = started.diagnostic;
    m_backend.reset();
    publishExit({TerminalExitStatus::Kind::StartFailed, 0, diagnostic});
    setState(State::Exited);
    return false;
  }

  m_childPid = m_backend->shellProcessId();
  connect(m_backend.get(), &TerminalSessionBackend::selectionChanged, this,
          &TerminalSession::selectionAvailable);
  connect(m_backend.get(), &TerminalSessionBackend::titleChanged, this,
          &TerminalSession::titleReceived);

  setState(State::Running);
  emit terminalWidgetChanged(m_backend->terminalWidget());
  // A fresh view carries no selection; publishing that keeps the
  // presentation's action state truthful across restarts (P2-4).
  emit selectionAvailable(false);
  m_pollTimer.start();
  return true;
}

void TerminalSession::publishExit(const TerminalExitStatus &status) {
  m_lastExit = status;
  if (!m_exitPublished) {
    m_exitPublished = true;
    emit sessionFinished(status);
  }
}

void TerminalSession::enterShutdownSequence(bool restartAfterwards) {
  m_restartAfterShutdown = restartAfterwards;
  m_phase = ShutdownPhase::Close;
  m_phaseTimer.start();
  setState(State::ShuttingDown);
  if (m_backend != nullptr) {
    // Presentation detaches the view before the adapter destroys it; the
    // connection is direct, so ordering is synchronous and safe.
    emit viewDisposalRequested();
    // The view is going away; selection availability must not survive it
    // (P2-4).
    emit selectionAvailable(false);
    m_backend->requestShutdown();
  }
  m_pollTimer.start();
}

void TerminalSession::completeShutdown(bool clean,
                                       const QString &diagnostic) {
  m_pollTimer.stop();
  m_phase = ShutdownPhase::None;
  if (clean) {
    // AGENT-GUARD (P1-2): the backend and the captured process-group id may
    // only be released when the child is confirmed gone. A ShutdownFailed
    // generation retains both so the survivor stays owned and quit/restart
    // can be refused honestly.
    m_backend.reset();
    m_childPid = 0;
  }
  setState(clean ? State::ShutdownComplete : State::ShutdownFailed);
  if (clean && m_restartAfterShutdown) {
    m_restartAfterShutdown = false;
    // The replacement generation is spawned before the finished signal so
    // observers never see a "finished" session that is already running.
    spawnGeneration();
  } else {
    m_restartAfterShutdown = false;
  }
  emit shutdownFinished(clean, diagnostic);
}

void TerminalSession::advanceShutdownPhase() {
  // AGENT-GUARD: m_childPid is the captured process-group leader. Signal
  // paths must never run for pid <= 0: POSIX would interpret 0/-others as
  // "my own group", which could signal QindaQt itself.
  if (m_childPid <= 0 || m_monitor == nullptr) {
    return;
  }
  const qint64 elapsed = m_phaseTimer.elapsed();
  switch (m_phase) {
  case ShutdownPhase::Close:
    // Master close already delivered SIGHUP; the TERM escalation only arms
    // after the close grace elapses. Whether signalProcessGroup succeeded is
    // decided by the next reap, never by the send result alone.
    if (elapsed >= m_bounds.closeGraceMs) {
      m_monitor->signalProcessGroup(m_childPid, SIGTERM);
      m_phase = ShutdownPhase::Term;
      m_phaseTimer.start();
    }
    break;
  case ShutdownPhase::Term:
    if (elapsed >= m_bounds.termGraceMs) {
      m_monitor->signalProcessGroup(m_childPid, SIGKILL);
      m_phase = ShutdownPhase::Kill;
      m_phaseTimer.start();
    }
    break;
  case ShutdownPhase::Kill:
    if (elapsed >= m_bounds.killGraceMs) {
      // SIGKILL cannot be ignored by our own child; surviving it means a
      // stuck uninterruptible state, which is reported honestly instead of
      // being mislabelled as a clean exit.
      completeShutdown(
          false,
          QStringLiteral("Terminal child did not exit after SIGKILL"));
    }
    break;
  case ShutdownPhase::None:
    break;
  }
}

void TerminalSession::pollTick() {
  if (m_state == State::Running) {
    if (m_monitor != nullptr && m_childPid > 0) {
      const auto info = m_monitor->reap(m_childPid);
      if (info.state == ProcessState::Exited) {
        m_pollTimer.stop();
        if (info.signaled) {
          publishExit({TerminalExitStatus::Kind::Signal, info.code, {}});
        } else if (info.statusKnown) {
          publishExit({TerminalExitStatus::Kind::Normal, info.code, {}});
        } else {
          // P2-5: another reaper consumed the status. Reporting a fabricated
          // normal exit 0 would violate the exit-truth contract, so the
          // outcome is surfaced as unknown.
          publishExit({TerminalExitStatus::Kind::UnknownExit, 0,
                       QStringLiteral("Exit status was consumed by another "
                                      "reaper")});
        }
        setState(State::Exited);
      }
    }
    return;
  }
  if (m_state == State::ShuttingDown) {
    if (m_childPid <= 0) {
      // No child was ever spawned for this generation (idle or start
      // failure); the view is already disposed, so shutdown is complete.
      completeShutdown(true, {});
      return;
    }
    if (m_monitor != nullptr &&
        m_monitor->reap(m_childPid).state == ProcessState::Exited) {
      completeShutdown(true, {});
      return;
    }
    advanceShutdownPhase();
  }
}

} // namespace QindaQt::Apps::Terminal
