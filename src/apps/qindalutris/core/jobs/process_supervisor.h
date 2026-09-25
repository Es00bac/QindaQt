// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "process_tree.h"

#include <QObject>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: watches one running process tree on a WORKER thread
// (ADR-0275 section 4b). Every blocking step -- the periodic /proc scans
// that track descendants and group members, `systemctl --user is-active`,
// and the TERM -> grace -> KILL sequence -- runs there, so the owner's
// (GUI) thread never blocks while a job runs. Results come back as a
// queued `settled` signal on the owner's thread.
//
//  - watch(): starts tracking right after the main process started.
//  - requestStop(): cancel/timeout; stops the whole tree, then settles.
//  - mainProcessExited(): a normal exit; if anything of the tree lingers
//    (a live tracked process or group member, or a still-active scope), it
//    is stopped the same way before settling (outcome.hadLeftovers).
// Exactly one `settled` per watch(). Reused by the game launcher's Force
// quit, which calls requestStop() on the title's supervisor.
// Destruction of a watching, unsettled supervisor stops the tree and JOINS
// the worker (blocking, bounded by grace + kill wait): destructors only.
class ProcessTreeSupervisor final : public QObject {
  Q_OBJECT
public:
  explicit ProcessTreeSupervisor(QObject *parent = nullptr);
  ~ProcessTreeSupervisor() override;

  void watch(const StopTarget &target, int graceMs, int killWaitMs);
  void requestStop();
  void mainProcessExited();
  [[nodiscard]] bool isWatching() const { return m_worker.joinable(); }

Q_SIGNALS:
  void settled(const QindaQt::QindaLutris::TreeStopOutcome &outcome);

private:
  enum class Command { None, Stop, MainExited, Abandon };
  void post(Command command);
  void run(StopTarget target, int graceMs, int killWaitMs);
  void join();

  std::thread m_worker;
  std::mutex m_mutex;
  std::condition_variable m_wake;
  Command m_command = Command::None;
};

} // namespace QindaQt::QindaLutris
