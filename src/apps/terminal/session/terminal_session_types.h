// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Terminal {

// PID and process-group values cross the Qt boundary as qint64 so headers stay
// free of POSIX types. Only process_liveness narrows to the platform pid_t.
using ProcessId = qint64;

// TerminalExitStatus is the single exit-truth value published by the session.
// The rendering widget cannot provide exit codes in teletype mode; only the
// application-owned reap fills in code/signal. See ADR-0030.
struct TerminalExitStatus final {
  enum class Kind {
    None,        // No child was started, or it is still running.
    Normal,      // The child exited; code is the exit status.
    Signal,      // The child was killed by signal number `code`.
    StartFailed, // The PTY/child could not be created at all.
  };

  Kind kind = Kind::None;
  int code = 0;
  QString diagnostic;

  [[nodiscard]] bool operator==(const TerminalExitStatus &) const = default;
};

// One launch is one argv: program is an absolute executable path, arguments
// are passed verbatim to execve. Nothing here is ever joined into a shell
// string; that invariant is what keeps hostile arguments non-executable.
struct TerminalLaunchRequest final {
  QString program;
  QStringList arguments;
  QString workingDirectory; // Empty inherits the launcher's directory.
  QStringList environment;  // Complete KEY=VALUE list handed to execve.
  QString title;

  [[nodiscard]] bool operator==(const TerminalLaunchRequest &) const = default;
};

// Bounds are injected so tests can exercise the whole teardown sequence in
// milliseconds and the application can use human-scale defaults. The poll
// interval also drives exit detection latency; 20 ms is imperceptible for a
// status line and keeps the GUI thread cheap.
struct TeardownBounds final {
  int closeGraceMs = 3000;
  int termGraceMs = 1000;
  int killGraceMs = 1000;
  int pollIntervalMs = 20;

  [[nodiscard]] bool operator==(const TeardownBounds &) const = default;
};

} // namespace QindaQt::Apps::Terminal

Q_DECLARE_METATYPE(QindaQt::Apps::Terminal::TerminalExitStatus)
