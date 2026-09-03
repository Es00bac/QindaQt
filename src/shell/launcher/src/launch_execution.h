// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Shell::Launcher {

// AGENT-CONTRACT: This header owns the bounded execution-key grammar of the
// launcher. The L0 model (QindaQt::ShellLauncher) deliberately never carries a
// command line (ADR-0042); execution data is extracted here, in the runtime
// adapter, from the raw document text the scanner already validated and
// retained. Nothing in this header touches the filesystem, processes, or
// D-Bus; those side effects live behind the spawner/activator seams.
namespace ExecutionBounds {
inline constexpr int maxExecCodeUnits = 4096;
inline constexpr int maxPathCodeUnits = 4096;
inline constexpr int maxArguments = 64;
inline constexpr int maxExpandedCodeUnits = 8192;
} // namespace ExecutionBounds

// The execution-relevant keys of one desktop-entry document, or the Exec-only
// projection of one action group, extracted under fixed ceilings. Actions
// inherit Terminal/Path/DBusActivatable from the entry-level group.
struct ExecutionKeys {
  QString exec; // keyfile-unescaped, field codes still present
  QString path; // working directory; empty when absent
  bool terminal = false;
  bool dbusActivatable = false;

  friend bool operator==(const ExecutionKeys &, const ExecutionKeys &) = default;
};

enum class ExecutionParseError {
  None,
  GroupNotFound, // the requested [Desktop Action <id>] group does not exist
  MissingExec,
  ExecTooLarge,
  PathTooLarge,
  InvalidBoolean,
  DuplicateKey,
  InvalidEscape,
};

struct ExecutionParseResult {
  std::optional<ExecutionKeys> keys;
  ExecutionParseError error = ExecutionParseError::None;
  QString message;

  bool ok() const { return keys.has_value(); }
};

// Extracts all execution keys of the primary entry group (actionId empty), or
// only Exec from one `[Desktop Action <id>]` group. Total over its
// input: hostile documents produce a typed error, never an exception or a
// partially decoded value. Locale-suffixed, unknown, and action-local
// Terminal/Path/DBusActivatable keys are ignored without decoding, mirroring
// the L0 parser's hostile-input rule and desktop-action scope.
class LaunchExecutionParser {
public:
  static ExecutionParseResult parse(const QString &documentText,
                                    const QString &actionId = {});
};

// Display values used by Exec field-code expansion. desktopFilePath feeds %k;
// it is the scanner-retained absolute path, never caller-invented text.
struct ExecExpansionValues {
  QString name;
  QString iconName;
  QString desktopFilePath;
};

enum class ExecPlanError {
  None,
  MissingProgram, // empty Exec, or every token was a droppable field code
  ExecTooLarge,
  UnterminatedQuote,
  UnsupportedFieldCode, // a field code the launcher cannot satisfy
  ArgumentLimitReached,
};

struct ExecPlan {
  QString program;
  QStringList arguments;

  friend bool operator==(const ExecPlan &, const ExecPlan &) = default;
};

struct ExecPlanResult {
  std::optional<ExecPlan> plan;
  ExecPlanError error = ExecPlanError::None;
  QString message;

  bool ok() const { return plan.has_value(); }
};

// Turns a decoded Exec string into an argv vector without any shell
// interpolation: double-quote grouping with the \" \\ \` \$ escapes, field
// codes per the desktop-entry specification, and fixed output ceilings.
// %f/%F/%u/%U (file/URL arguments the launcher never supplies) and the
// deprecated %d/%D/%n/%N/%v/%m codes are dropped as whole tokens; an unknown
// or embedded list code is a typed error, so a hostile Exec can never smuggle
// text into a different argument position.
class ExecFieldCodeExpander {
public:
  static ExecPlanResult expand(const QString &decodedExec,
                               const ExecExpansionValues &values);
};

} // namespace QindaQt::Shell::Launcher
