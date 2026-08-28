// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_types.h"

namespace QindaQt::Apps::Terminal {

enum class ProcessState { Unknown, Running, Exited };

struct ProcessExitInfo final {
  ProcessState state = ProcessState::Unknown;
  bool signaled = false; // Valid when state == Exited.
  int code = 0;          // Exit code, or signal number when signaled.

  [[nodiscard]] bool operator==(const ProcessExitInfo &) const = default;
};

// AGENT-CONTRACT: ProcessMonitor is the only seam through which the session
// observes or signals other processes. Tests inject fakes; production injects
// PosixProcessMonitor. Implementations must be safe against PID reuse: never
// signal a bare PID, only a process-group leader after re-validating that the
// leader still leads its own group (see PosixProcessMonitor). reap() is
// single-shot per process: it consumes the child status.
class ProcessMonitor {
public:
  virtual ~ProcessMonitor() = default;

  [[nodiscard]] virtual ProcessExitInfo reap(ProcessId pid) = 0;
  [[nodiscard]] virtual bool signalProcessGroup(ProcessId groupLeader,
                                                int signalNumber) = 0;
};

// Production monitor over waitpid/getpgid/kill. The Terminal application is
// always the direct parent of the terminal child, so waitpid(WNOHANG) is the
// correct reap primitive; ECHILD maps to Exited because another component in
// this process (never QindaQt code) already reaped it.
class PosixProcessMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] ProcessExitInfo reap(ProcessId pid) override;
  [[nodiscard]] bool signalProcessGroup(ProcessId groupLeader,
                                        int signalNumber) override;
};

} // namespace QindaQt::Apps::Terminal
