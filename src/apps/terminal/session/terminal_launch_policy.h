// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_types.h"

#include <QSize>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Terminal {

struct PolicyOutcome final {
  bool ok = false;
  QString diagnostic;
};

struct ShellResolution final {
  PolicyOutcome outcome;
  TerminalLaunchRequest request;
};

struct EnvironmentResolution final {
  PolicyOutcome outcome;
  QStringList environment;
};

// AGENT-CONTRACT: Pure launch policy shared by the application CLI, the
// session restart path, and tests. It validates hostile input before anything
// forks, and its outputs are always argv semantics. It performs no I/O beyond
// filesystem metadata checks, never executes anything, and never touches Qt
// GUI types, so it is usable from any thread.
class TerminalLaunchPolicy final {
public:
  static constexpr int kMaxArguments = 64;
  static constexpr int kMaxArgumentLength = 4096;
  static constexpr int kMaxEnvironmentEntries = 4096;
  static constexpr int kMaxEnvironmentEntryLength = 4096;
  // P3-1: program and working-directory paths follow the same bounded
  // hostile-input contract as arguments and environment entries.
  static constexpr int kMaxProgramLength = 4096;
  static constexpr int kMaxWorkingDirectoryLength = 4096;

  // TERM/COLORTERM are always forced: a hostile or missing inherited value
  // must never reach the child, and a remote/attacker-controlled environment
  // cannot downgrade the terminal's advertised capabilities.
  static inline const QString kTermValue = QStringLiteral("xterm-256color");
  static inline const QString kColorTermValue = QStringLiteral("truecolor");
  static inline const QString kTermVariable = QStringLiteral("TERM");
  static inline const QString kColorTermVariable =
      QStringLiteral("COLORTERM");
  static inline const QString kUtf8FallbackLocale =
      QStringLiteral("C.UTF-8");
  static inline const QString kDefaultShell = QStringLiteral("/bin/bash");
  static inline const QString kShellVariable = QStringLiteral("SHELL");

  // Resolves program/arguments into a validated launch request. Resolution
  // order: explicit program, then SHELL from the base environment, then the
  // built-in default. The program must be an absolute, existing, executable
  // regular file path without control characters. Arguments are validated for
  // count, length, and control characters; they are never interpreted.
  [[nodiscard]] static ShellResolution
  resolveShell(const QString &explicitProgram,
               const QStringList &explicitArguments,
               const QString &workingDirectory,
               const QStringList &baseEnvironment);

  // Builds the complete child environment from KEY=VALUE pairs. Entries with
  // malformed keys, newline-bearing values, or oversized entries are dropped,
  // never repaired; TERM and COLORTERM are forced to the policy values.
  //
  // AGENT-CONTRACT: The child's effective character-set locale follows libc
  // precedence LC_ALL > LC_CTYPE > LANG. The rendering layer decodes child
  // bytes as UTF-8 (ADR-0030), so the first locale variable present in that
  // precedence order must select UTF-8: when it does not, exactly that
  // variable is replaced with LANG's policy fallback value, and when none is
  // present LANG=C.UTF-8 is appended. Hostile input can only shrink or
  // normalize the environment, never inject an entry, and the effective
  // UTF-8 outcome is what tests assert.
  [[nodiscard]] static EnvironmentResolution
  childEnvironment(const QStringList &baseEnvironment);

  // Clamps the embedded terminal view's pixel extents to a sane range. Zero
  // or negative extents collapse to the minimum instead of failing; huge
  // extents clamp to the maximum. The widget owns the actual PTY winsize
  // ioctl and cell conversion (ADR-0030); this policy exists so hostile
  // window sizes cannot reach it verbatim.
  static constexpr int kMinViewWidth = 1;
  static constexpr int kMinViewHeight = 1;
  static constexpr int kMaxViewWidth = 100000;
  static constexpr int kMaxViewHeight = 100000;
  [[nodiscard]] static QSize clampViewSize(int width, int height);
};

} // namespace QindaQt::Apps::Terminal
