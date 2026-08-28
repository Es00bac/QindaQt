// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_launch_policy.h"

#include <QChar>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::Apps::Terminal {
namespace {

bool hasControlCharacter(const QString &value) {
  for (const QChar character : value) {
    // QChar::isControl was removed in Qt 6.11; the category test is the
    // replacement for the same Other_Control classification.
    if (character.category() == QChar::Other_Control) {
      return true;
    }
  }
  return false;
}

// Environment keys are restricted to the portable subset accepted by every
// libc. Anything else is dropped rather than repaired: repairing hostile keys
// could silently change which executable or locale a child picks up.
const QRegularExpression &environmentKeyPattern() {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
  return pattern;
}

struct NameValue {
  QString name;
  QString value;
};

std::optional<NameValue> splitEntry(const QString &entry) {
  const qsizetype separator = entry.indexOf(QLatin1Char('='));
  if (separator <= 0) {
    return std::nullopt;
  }
  const QString name = entry.left(separator);
  const QString value = entry.mid(separator + 1);
  if (!environmentKeyPattern().match(name).hasMatch()) {
    return std::nullopt;
  }
  if (value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))) {
    return std::nullopt;
  }
  if (entry.size() > TerminalLaunchPolicy::kMaxEnvironmentEntryLength) {
    return std::nullopt;
  }
  return NameValue{name, value};
}

// AGENT-GUARD (P2-1): the UTF-8 decision is a strict codeset comparison, not
// a substring search — a hostile value like "de_DE.UTF8-evil" must not pass.
// The codeset is the part after the first '.', with any '@' modifier
// stripped; "C" and bare language tags have no codeset and are not UTF-8.
bool selectsUtf8(const QString &value) {
  const qsizetype dot = value.indexOf(QLatin1Char('.'));
  if (dot < 0) {
    return false;
  }
  QString codeset = value.mid(dot + 1);
  const qsizetype modifier = codeset.indexOf(QLatin1Char('@'));
  if (modifier >= 0) {
    codeset.truncate(modifier);
  }
  const QString upper = codeset.toUpper();
  return upper == QLatin1String("UTF-8") || upper == QLatin1String("UTF8");
}

void appendForcedEntry(QStringList &environment, const QString &name,
                       const QString &value) {
  // AGENT-GUARD: Remove every inherited entry of this variable, not just an
  // exact "NAME=" match: the child's getenv resolves the FIRST envp entry,
  // so a surviving inherited value would silently win over the forced one.
  const QString prefix = name + QLatin1Char('=');
  environment.removeIf(
      [&prefix](const QString &entry) { return entry.startsWith(prefix); });
  environment.append(prefix + value);
}

[[nodiscard]] QString quotedForDiagnostic(const QString &value) {
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
  escaped.replace(QLatin1Char('\n'), QLatin1String("\\n"));
  escaped.replace(QLatin1Char('\r'), QLatin1String("\\r"));
  return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace

ShellResolution TerminalLaunchPolicy::resolveShell(
    const QString &explicitProgram, const QStringList &explicitArguments,
    const QString &workingDirectory, const QStringList &baseEnvironment) {
  QString candidate = explicitProgram;
  if (candidate.isEmpty()) {
    for (const QString &entry : baseEnvironment) {
      const auto split = splitEntry(entry);
      if (split.has_value() && split->name == kShellVariable) {
        candidate = split->value;
        break;
      }
    }
  }
  if (candidate.isEmpty()) {
    candidate = kDefaultShell;
  }

  if (candidate.isEmpty()) {
    return {.outcome = {false, QStringLiteral("Shell program is empty")},
            .request = {}};
  }
  // P3-1: bounded hostile-path contract, mirroring the argument bounds.
  if (candidate.size() > kMaxProgramLength) {
    return {.outcome = {false,
                        QStringLiteral("Shell program path exceeds %1 bytes")
                            .arg(kMaxProgramLength)},
            .request = {}};
  }
  if (hasControlCharacter(candidate)) {
    return {.outcome = {false,
                        QStringLiteral("Shell program contains a control "
                                       "character: %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }
  if (!QFileInfo(candidate).isAbsolute()) {
    return {.outcome = {false,
                        QStringLiteral("Shell program must be an absolute "
                                       "path, not %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }
  const QFileInfo info(candidate);
  // AGENT-NOTE: The metadata check is advisory defense against typos and
  // hostile CLI values; execve remains the real authority. TOCTOU between
  // this check and execve is irrelevant because the argv is never
  // interpreted by a shell (ADR-0030).
  if (!info.exists()) {
    return {.outcome = {false,
                        QStringLiteral("Shell program does not exist: %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }
  if (info.isDir()) {
    return {.outcome = {false,
                        QStringLiteral("Shell program is a directory, not an "
                                       "executable: %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }
  if (!info.isFile()) {
    return {.outcome = {false,
                        QStringLiteral("Shell program is not a regular "
                                       "file: %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }
  if (!info.isExecutable()) {
    return {.outcome = {false,
                        QStringLiteral("Shell program is not executable: %1")
                            .arg(quotedForDiagnostic(candidate))},
            .request = {}};
  }

  if (explicitArguments.size() > kMaxArguments) {
    return {.outcome = {false,
                        QStringLiteral("Too many shell arguments: %1 > %2")
                            .arg(explicitArguments.size())
                            .arg(kMaxArguments)},
            .request = {}};
  }
  for (const QString &argument : explicitArguments) {
    if (hasControlCharacter(argument)) {
      return {.outcome = {false,
                          QStringLiteral("Shell argument contains a control "
                                         "character: %1")
                              .arg(quotedForDiagnostic(argument))},
              .request = {}};
    }
    if (argument.size() > kMaxArgumentLength) {
      return {.outcome = {false,
                          QStringLiteral("Shell argument exceeds %1 bytes")
                              .arg(kMaxArgumentLength)},
              .request = {}};
    }
  }

  if (!workingDirectory.isEmpty()) {
    if (workingDirectory.size() > kMaxWorkingDirectoryLength) {
      return {.outcome =
                  {false,
                   QStringLiteral("Working directory path exceeds %1 bytes")
                       .arg(kMaxWorkingDirectoryLength)},
              .request = {}};
    }
    if (!QFileInfo(workingDirectory).isDir()) {
      return {.outcome = {false,
                          QStringLiteral("Working directory is not a "
                                         "directory: %1")
                              .arg(quotedForDiagnostic(workingDirectory))},
              .request = {}};
    }
  }

  return {.outcome = {true, {}},
          .request = TerminalLaunchRequest{
              .program = candidate,
              .arguments = explicitArguments,
              .workingDirectory = workingDirectory,
              .environment = {},
              .title = {}}};
}

EnvironmentResolution
TerminalLaunchPolicy::childEnvironment(const QStringList &baseEnvironment) {
  if (baseEnvironment.size() > kMaxEnvironmentEntries) {
    return {.outcome = {false,
                        QStringLiteral("Base environment exceeds %1 entries")
                            .arg(kMaxEnvironmentEntries)},
            .environment = {}};
  }

  // AGENT-GUARD (P2-1): the effective locale variable is chosen by presence
  // in libc's precedence order — LC_ALL if present, else LC_CTYPE, else
  // LANG — never by envp order, so a UTF-8 LANG cannot mask a non-UTF-8
  // LC_ALL that appears later in the list. First occurrence of each variable
  // is the deterministic decision input; forcing removes every occurrence.
  struct LocaleSelection {
    bool present = false;
    bool utf8 = false;
  };
  struct LocaleVariableInfo {
    QString name;
    LocaleSelection selection;
  };
  LocaleVariableInfo variables[3] = {
      {QStringLiteral("LC_ALL"), {}},
      {QStringLiteral("LC_CTYPE"), {}},
      {QStringLiteral("LANG"), {}},
  };

  QStringList sanitized;
  sanitized.reserve(baseEnvironment.size() + 3);
  for (const QString &entry : baseEnvironment) {
    const auto split = splitEntry(entry);
    if (!split.has_value()) {
      continue;
    }

    bool isLocaleVariable = false;
    for (auto &variable : variables) {
      if (split->name == variable.name) {
        isLocaleVariable = true;
        if (!variable.selection.present) {
          variable.selection.present = true;
          variable.selection.utf8 = selectsUtf8(split->value);
        }
        break;
      }
    }
    if (isLocaleVariable) {
      // Kept for now; the authority repair below rewrites exactly the
      // variable that must be forced.
      sanitized.append(entry);
      continue;
    }
    if (split->name == kTermVariable || split->name == kColorTermVariable) {
      // Forced values are appended below; inherited ones never pass through.
      continue;
    }
    sanitized.append(entry);
  }

  const LocaleVariableInfo *authority = nullptr;
  for (const auto &variable : variables) {
    if (variable.selection.present) {
      authority = &variable;
      break;
    }
  }
  if (authority == nullptr) {
    // Minimal sessions legitimately carry no locale selection; this is a
    // fallback, not an error.
    appendForcedEntry(sanitized, QStringLiteral("LANG"), kUtf8FallbackLocale);
  } else if (!authority->selection.utf8) {
    // The effective authority is replaced in place, keeping its precedence
    // position of power while making the child emit UTF-8 bytes the renderer
    // can decode.
    appendForcedEntry(sanitized, authority->name, kUtf8FallbackLocale);
  }

  appendForcedEntry(sanitized, kTermVariable, kTermValue);
  appendForcedEntry(sanitized, kColorTermVariable, kColorTermValue);
  return {.outcome = {true, {}}, .environment = sanitized};
}

QSize TerminalLaunchPolicy::clampViewSize(int width, int height) {
  const int clampedWidth = qBound(kMinViewWidth, width, kMaxViewWidth);
  const int clampedHeight = qBound(kMinViewHeight, height, kMaxViewHeight);
  return QSize(clampedWidth, clampedHeight);
}

} // namespace QindaQt::Apps::Terminal
