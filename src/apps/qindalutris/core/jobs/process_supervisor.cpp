// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_supervisor.h"

#include <QMetaObject>

#include <chrono>

namespace QindaQt::QindaLutris {

namespace {

constexpr auto kTrackInterval = std::chrono::milliseconds(500);

bool anyTrackedAlive(const StopTarget &target) {
  for (const ProcessIdentity &process : target.known) {
    if (isProcessAlive(process)) {
      return true;
    }
  }
  return false;
}

} // namespace

ProcessTreeSupervisor::ProcessTreeSupervisor(QObject *parent) : QObject(parent) {}

ProcessTreeSupervisor::~ProcessTreeSupervisor() {
  post(Command::Abandon);
  join();
}

void ProcessTreeSupervisor::join() {
  if (m_worker.joinable()) {
    m_worker.join();
  }
}

void ProcessTreeSupervisor::watch(const StopTarget &target, int graceMs, int killWaitMs) {
  post(Command::Abandon); // a previous, already settled worker just exits
  join();
  {
    const std::lock_guard lock(m_mutex);
    m_command = Command::None;
  }
  m_worker = std::thread([this, target, graceMs, killWaitMs] { run(target, graceMs, killWaitMs); });
}

void ProcessTreeSupervisor::requestStop() { post(Command::Stop); }

void ProcessTreeSupervisor::mainProcessExited() { post(Command::MainExited); }

void ProcessTreeSupervisor::post(Command command) {
  {
    const std::lock_guard lock(m_mutex);
    // A stop outranks an exit notice; Abandon outranks both.
    if (m_command == Command::None || command == Command::Abandon ||
        (command == Command::Stop && m_command == Command::MainExited)) {
      m_command = command;
    }
  }
  m_wake.notify_all();
}

void ProcessTreeSupervisor::run(StopTarget target, int graceMs, int killWaitMs) {
  Command command = Command::None;
  for (;;) {
    {
      std::unique_lock lock(m_mutex);
      m_wake.wait_for(lock, kTrackInterval, [this] { return m_command != Command::None; });
      command = m_command;
    }
    trackProcessTree(target);
    if (command != Command::None) {
      break;
    }
  }
  TreeStopOutcome outcome;
  if (command == Command::MainExited) {
    const bool scopeActive =
        !target.scopeUnit.isEmpty() && isScopeActive(target.tools, target.scopeUnit);
    if (anyTrackedAlive(target) || scopeActive) {
      outcome = stopProcessTree(target, graceMs, killWaitMs);
      outcome.hadLeftovers = true;
    } else {
      outcome.trackedGone = true;
      outcome.proven = !target.scopeUnit.isEmpty(); // scope inactive, nothing tracked
      outcome.detail = describeOutcome(target, outcome, 0);
    }
  } else {
    // Stop, or Abandon from the destructor: the tree must not outlive us.
    outcome = stopProcessTree(target, graceMs, killWaitMs);
  }
  if (command != Command::Abandon) {
    // Safe: the destructor joins this thread before `this` goes away, and
    // Qt drops queued calls to a destroyed receiver.
    QMetaObject::invokeMethod(
        this, [this, outcome] { Q_EMIT settled(outcome); }, Qt::QueuedConnection);
  }
}

} // namespace QindaQt::QindaLutris
