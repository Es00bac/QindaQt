// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_launch_policy.h"

#include <QChar>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::Apps::Terminal {
namespace {

bool hasControlCharacter(const QString &value) {
  for (const QChar character : value) {
    if (character.isControl()) {
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

bool selectsUtf8(const QString &value) {
  const QString upper = value.toUpper();
  return upper.contains(QLatin1String("UTF-8")) ||
         upper.contains(QLatin1String("UTF8"));
}

void appendForcedEntry(QStringList &environment, const QString &name,
                       const QString &value) {
  environment.removeAll(name + QLatin1Char('='));
  environment.append(name + QLatin1Char('=') + value);
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
  // interpreted by a shell (ADR-0028).
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

  if (!workingDirectory.isEmpty() &&
      !QFileInfo(workingDirectory).isDir()) {
    return {.outcome = {false,
                        QStringLiteral("Working directory is not a "
                                       "directory: %1")
                            .arg(quotedForDiagnostic(workingDirectory))},
            .request = {}};
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

  QStringList sanitized;
  sanitized.reserve(baseEnvironment.size() + 3);
  bool hasUtf8Locale = false;
  for (const QString &entry : baseEnvironment) {
    const auto split = splitEntry(entry);
    if (!split.has_value()) {
      continue;
    }
    if (split->name == QLatin1String("LC_ALL") ||
        split->name == QLatin1String("LC_CTYPE") ||
        split->name == QLatin1String("LANG")) {
      hasUtf8Locale = hasUtf8Locale || selectsUtf8(split->value);
    }
    if (split->name == kTermVariable || split->name == kColorTermVariable) {
      // Forced values are appended below; inherited ones never pass through.
      continue;
    }
    sanitized.append(entry);
  }
  if (!hasUtf8Locale) {
    // AGENT-CONTRACT: qtermwidget decodes child bytes as UTF-8 (ADR-0028), so
    // the child must run under a UTF-8 locale or its output would be decoded
    // wrong. Missing UTF-8 selection in the base environment is a normal
    // condition (minimal sessions), so this is a fallback, not an error.
    appendForcedEntry(sanitized, QStringLiteral("LANG"), kUtf8FallbackLocale);
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
