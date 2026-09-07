// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_types.h"

namespace QindaQt::Apps::Terminal {

enum class ProcessState { Unknown, Running, Exited };

// Group emptiness is separate from leader exit: waitpid() can reap the direct
// child while descendants remain in its captured process group.
enum class ProcessGroupState { Unknown, NonEmpty, Empty };

struct ProcessExitInfo final {
  ProcessState state = ProcessState::Unknown;
  bool signaled = false;   // Valid when state == Exited && statusKnown.
  int code = 0;            // Exit code, or signal number when signaled.
  bool statusKnown = true; // False when the status was reaped elsewhere.

  [[nodiscard]] bool operator==(const ProcessExitInfo &) const = default;
};

// AGENT-CONTRACT: ProcessMonitor is the only seam through which the session
// observes or signals other processes. Tests inject fakes; production injects
// PosixProcessMonitor. Implementations never signal a bare PID. The caller
// captures the child's pid once, while that child is the setsid-created group
// leader, and retains it as the group id until processGroupState() proves the
// complete group empty. reap() is single-shot per process: it consumes the
// direct child status but says nothing about descendants.
class ProcessMonitor {
public:
  virtual ~ProcessMonitor() = default;

  // Best-effort directory of the owned live shell; empty means unavailable.
  [[nodiscard]] virtual QString workingDirectory(ProcessId) { return {}; }
  [[nodiscard]] virtual ProcessExitInfo reap(ProcessId pid) = 0;
  [[nodiscard]] virtual ProcessGroupState
  processGroupState(ProcessId processGroupId) = 0;
  [[nodiscard]] virtual bool signalProcessGroup(ProcessId processGroupId,
                                                int signalNumber) = 0;
};

// Production monitor over waitpid/killpg and Linux /proc. The Terminal is
// always the direct parent of the terminal child, so waitpid(WNOHANG) is the
// correct leader-reap primitive. Group-empty publication requires both a
// signal-probe miss and a complete /proc process-group scan, so an orphaned or
// zombie descendant cannot be mistaken for teardown completion.
class PosixProcessMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] QString workingDirectory(ProcessId pid) override;
  [[nodiscard]] ProcessExitInfo reap(ProcessId pid) override;
  [[nodiscard]] ProcessGroupState
  processGroupState(ProcessId processGroupId) override;
  [[nodiscard]] bool signalProcessGroup(ProcessId processGroupId,
                                        int signalNumber) override;
};

} // namespace QindaQt::Apps::Terminal
