// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-GUARD: The child pid is captured once as the session leader created by
// setsid(), so pid == pgid at start. A recycled PID would belong to a different
// session; the getpgid(pid) == pid check below rejects such a process before
// any signal leaves this application. Removing that check would allow the
// terminal to signal an unrelated recycled process.
bool leaderStillLeadsItsGroup(ProcessId pid) {
  const auto narrowPid = static_cast<pid_t>(pid);
  return getpgid(narrowPid) == narrowPid;
}

} // namespace

ProcessExitInfo PosixProcessMonitor::reap(ProcessId pid) {
  int status = 0;
  const pid_t result = waitpid(static_cast<pid_t>(pid), &status, WNOHANG);
  if (result == static_cast<pid_t>(pid)) {
    ProcessExitInfo info;
    info.state = ProcessState::Exited;
    info.statusKnown = true;
    if (WIFSIGNALED(status)) {
      info.signaled = true;
      info.code = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
      info.signaled = false;
      info.code = WEXITSTATUS(status);
    } else {
      // Stopped/continued children are not terminal exits; treat as running
      // so the session keeps waiting for a real terminal disposition.
      info.state = ProcessState::Running;
    }
    return info;
  }
  if (result == 0) {
    return {.state = ProcessState::Running, .signaled = false, .code = 0};
  }
  // result < 0: ECHILD means the exit status was already reaped elsewhere —
  // reported as Exited with statusKnown=false (never a fabricated normal
  // code); any other errno is Unknown so callers treat the process
  // conservatively as alive rather than signaling on a guess.
  const bool alreadyReaped = (errno == ECHILD);
  return {.state = alreadyReaped ? ProcessState::Exited
                                 : ProcessState::Unknown,
          .signaled = false,
          .code = 0,
          .statusKnown = !alreadyReaped};
}

bool PosixProcessMonitor::signalProcessGroup(ProcessId groupLeader,
                                             int signalNumber) {
  if (!leaderStillLeadsItsGroup(groupLeader)) {
    return false;
  }
  // Negative pid targets the whole process group, which covers grandchildren
  // that inherited the session's group. This is the teardown guarantee the
  // acceptance contract names; plain kill(pid) would leak grandchildren.
  return ::kill(-static_cast<pid_t>(groupLeader), signalNumber) == 0;
}

} // namespace QindaQt::Apps::Terminal
