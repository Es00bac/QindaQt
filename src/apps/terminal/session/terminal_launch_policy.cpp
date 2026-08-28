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

bool selectsUtf8(const QString &value) {
  const QString upper = value.toUpper();
  return upper.contains(QLatin1String("UTF-8")) ||
         upper.contains(QLatin1String("UTF8"));
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

  // Locale authority bookkeeping: the first variable present in libc's
  // precedence order decides the child's character set, so only that
  // variable's UTF-8-ness matters.
  enum class LocaleVariable { None, LcAll, LcType, Lang };
  const struct {
    QLatin1String name;
    LocaleVariable kind;
  } localeVariables[3] = {
      {QLatin1String("LC_ALL"), LocaleVariable::LcAll},
      {QLatin1String("LC_CTYPE"), LocaleVariable::LcType},
      {QLatin1String("LANG"), LocaleVariable::Lang},
  };

  QStringList sanitized;
  sanitized.reserve(baseEnvironment.size() + 3);
  LocaleVariable effectiveAuthority = LocaleVariable::None;
  bool effectiveAuthorityIsUtf8 = false;
  for (const QString &entry : baseEnvironment) {
    const auto split = splitEntry(entry);
    if (!split.has_value()) {
      continue;
    }

    bool isLocaleVariable = false;
    for (const auto &variable : localeVariables) {
      if (split->name == variable.name) {
        isLocaleVariable = true;
        // First occurrence wins as the deterministic decision input; the
        // repair below removes every occurrence of the variable it forces.
        if (effectiveAuthority == LocaleVariable::None) {
          effectiveAuthority = variable.kind;
          effectiveAuthorityIsUtf8 = selectsUtf8(split->value);
        }
        break;
      }
    }
    if (isLocaleVariable) {
      // Kept for now; the authority repair below rewrites exactly the
      // variables that must be forced.
      sanitized.append(entry);
      continue;
    }
    if (split->name == kTermVariable || split->name == kColorTermVariable) {
      // Forced values are appended below; inherited ones never pass through.
      continue;
    }
    sanitized.append(entry);
  }

  // AGENT-GUARD: Effective precedence, not string presence, is the
  // guarantee. A UTF-8 LANG must not mask a non-UTF-8 LC_ALL, and a UTF-8
  // LC_ALL must not cause an unnecessary rewrite of LC_CTYPE/LANG. The
  // forced replacement keeps the inherited variable's authority while making
  // the child emit UTF-8 bytes the renderer can decode.
  switch (effectiveAuthority) {
  case LocaleVariable::LcAll:
  case LocaleVariable::LcType:
  case LocaleVariable::Lang:
    if (!effectiveAuthorityIsUtf8) {
      for (const auto &variable : localeVariables) {
        if (variable.kind == effectiveAuthority) {
          appendForcedEntry(sanitized, variable.name, kUtf8FallbackLocale);
          break;
        }
      }
    }
    break;
  case LocaleVariable::None:
    // Minimal sessions legitimately carry no locale selection; this is a
    // fallback, not an error.
    appendForcedEntry(sanitized, QStringLiteral("LANG"), kUtf8FallbackLocale);
    break;
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
