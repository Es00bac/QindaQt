// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launch_execution.h"

#include <QSet>

#include <utility>

namespace QindaQt::Shell::Launcher {
namespace {

ExecutionParseResult parseFailure(ExecutionParseError error, QString message)
{
  return ExecutionParseResult { std::nullopt, error, std::move(message) };
}

bool isTargetGroup(const QString &group, const QString &actionId)
{
  if (actionId.isEmpty())
    return group == QLatin1String("Desktop Entry");
  return group == QLatin1String("Desktop Action ") + actionId;
}

// Mirrors the L0 parser's keyfile escape grammar (\s \n \t \r \\). Kept local
// so the execution adapter never reaches into the pure model's internals;
// both decode the same specification subsection.
std::optional<QString> unescapeValue(const QString &value)
{
  QString decoded;
  decoded.reserve(value.size());
  for (int index = 0; index < value.size(); ++index) {
    if (value.at(index) != QLatin1Char('\\')) {
      decoded.append(value.at(index));
      continue;
    }
    if (index + 1 >= value.size())
      return std::nullopt;
    const QChar escape = value.at(++index);
    switch (escape.unicode()) {
    case 's':
      decoded.append(QLatin1Char(' '));
      break;
    case 'n':
      decoded.append(QLatin1Char('\n'));
      break;
    case 't':
      decoded.append(QLatin1Char('\t'));
      break;
    case 'r':
      decoded.append(QLatin1Char('\r'));
      break;
    case '\\':
      decoded.append(QLatin1Char('\\'));
      break;
    default:
      return std::nullopt;
    }
  }
  return decoded;
}

bool parseBoolean(const QString &value, bool *out)
{
  if (value == QLatin1String("true")) {
    *out = true;
    return true;
  }
  if (value == QLatin1String("false")) {
    *out = false;
    return true;
  }
  return false;
}

ExecPlanResult planFailure(ExecPlanError error, QString message)
{
  return ExecPlanResult { std::nullopt, error, std::move(message) };
}

enum class TokenizeError { None, UnterminatedQuote };

// Splits on unquoted whitespace. Double quotes group; inside quotes the
// escapes \" \\ \` \$ are honored; outside quotes a backslash quotes the next
// character literally. Newlines never reach here (keyfile decoding would have
// required \n), but any stray control character ends the token defensively.
std::optional<QStringList> tokenize(const QString &exec, TokenizeError *error)
{
  QStringList tokens;
  QString current;
  bool inToken = false;
  bool quoted = false;
  for (int index = 0; index < exec.size(); ++index) {
    const QChar character = exec.at(index);
    if (character == QLatin1Char('\\') && index + 1 < exec.size()) {
      const QChar next = exec.at(index + 1);
      if (quoted && next != QLatin1Char('"') && next != QLatin1Char('\\')
          && next != QLatin1Char('`') && next != QLatin1Char('$')) {
        current.append(character);
        continue;
      }
      current.append(next);
      ++index;
      inToken = true;
      continue;
    }
    if (quoted) {
      if (character == QLatin1Char('"'))
        quoted = false;
      else
        current.append(character);
      inToken = true;
      continue;
    }
    if (character == QLatin1Char('"')) {
      quoted = true;
      inToken = true;
      continue;
    }
    if (character == QLatin1Char(' ') || character == QLatin1Char('\t')
        || character == QLatin1Char('\n') || character == QLatin1Char('\r')) {
      if (inToken) {
        tokens.append(current);
        current.clear();
        inToken = false;
      }
      continue;
    }
    current.append(character);
    inToken = true;
  }
  if (quoted) {
    *error = TokenizeError::UnterminatedQuote;
    return std::nullopt;
  }
  if (inToken)
    tokens.append(current);
  *error = TokenizeError::None;
  return tokens;
}

bool isDroppedCode(QChar code)
{
  // File/URL codes the launcher never supplies, plus deprecated codes.
  static const QSet<QChar> dropped {
    QLatin1Char('f'), QLatin1Char('F'), QLatin1Char('u'), QLatin1Char('U'),
    QLatin1Char('d'), QLatin1Char('D'), QLatin1Char('n'), QLatin1Char('N'),
    QLatin1Char('v'), QLatin1Char('m'),
  };
  return dropped.contains(code);
}

// Expands one token into zero, one, or two replacement arguments. Returns
// false when a field code is unsupported; an empty result means the token
// disappears entirely (a standalone file/URL or deprecated code, or %i
// without an Icon).
bool expandToken(const QString &token, const ExecExpansionValues &values,
                 QStringList *replacement, ExecPlanError *error)
{
  QString expanded = token;
  qsizetype percent = expanded.indexOf(QLatin1Char('%'));
  while (percent >= 0) {
    if (percent + 1 >= expanded.size()) {
      *error = ExecPlanError::UnsupportedFieldCode;
      return false;
    }
    const QChar code = expanded.at(percent + 1);
    if (code == QLatin1Char('%')) {
      expanded.remove(percent, 1);
      percent = expanded.indexOf(QLatin1Char('%'), percent + 1);
      continue;
    }
    const bool standalone = expanded.size() == 2;
    if (isDroppedCode(code)) {
      if (!standalone) {
        // AGENT-GUARD: A list code embedded in a larger token would expand to
        // zero or many arguments at an attacker-chosen position; refuse it
        // instead of guessing.
        *error = ExecPlanError::UnsupportedFieldCode;
        return false;
      }
      return true; // whole token dropped
    }
    if (code == QLatin1Char('i')) {
      // %i is specified as the two-argument form --icon <name>; it is only
      // legal as a standalone token so it can never merge with other text.
      if (!standalone) {
        *error = ExecPlanError::UnsupportedFieldCode;
        return false;
      }
      if (!values.iconName.isEmpty()) {
        replacement->append(QStringLiteral("--icon"));
        replacement->append(values.iconName);
      }
      return true;
    }
    QString inserted;
    if (code == QLatin1Char('c')) {
      inserted = values.name;
    } else if (code == QLatin1Char('k')) {
      inserted = values.desktopFilePath;
    } else {
      *error = ExecPlanError::UnsupportedFieldCode;
      return false;
    }
    expanded.replace(percent, 2, inserted);
    percent = expanded.indexOf(QLatin1Char('%'), percent + inserted.size());
  }
  replacement->append(expanded);
  return true;
}

} // namespace

ExecutionParseResult LaunchExecutionParser::parse(const QString &documentText,
                                                  const QString &actionId)
{
  ExecutionKeys keys;
  bool foundGroup = false;
  QSet<QString> seenKeys;
  QString currentGroup;

  const QStringList lines = documentText.split(QLatin1Char('\n'));
  for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
    QString line = lines.at(lineNumber);
    if (line.endsWith(QLatin1Char('\r')))
      line.chop(1);
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
      continue;
    if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
      currentGroup = trimmed.mid(1, trimmed.size() - 2);
      continue;
    }
    if (currentGroup.isEmpty())
      continue;
    const bool target = isTargetGroup(currentGroup, actionId);
    if (target)
      foundGroup = true;
    const qsizetype separator = line.indexOf(QLatin1Char('='));
    if (separator <= 0)
      continue;
    const QString key = line.left(separator).trimmed();
    // AGENT-GUARD: Only the four execution keys are decoded, and only inside
    // the target group. Locale variants and extension keys stay opaque so an
    // extension's escape grammar cannot reach execution planning.
    if (!target || key.contains(QLatin1Char('[')))
      continue;
    const bool recognized = key == QLatin1String("Exec")
        || key == QLatin1String("Terminal") || key == QLatin1String("Path")
        || key == QLatin1String("DBusActivatable");
    if (!recognized)
      continue;
    if (seenKeys.contains(key)) {
      return parseFailure(ExecutionParseError::DuplicateKey,
                          QStringLiteral("duplicate execution key in target group"));
    }
    seenKeys.insert(key);

    const QString encoded = line.mid(separator + 1).trimmed();
    const auto value = unescapeValue(encoded);
    if (!value) {
      return parseFailure(ExecutionParseError::InvalidEscape,
                          QStringLiteral("unsupported escape in execution key"));
    }
    if (key == QLatin1String("Exec")) {
      if (value->size() > ExecutionBounds::maxExecCodeUnits) {
        return parseFailure(ExecutionParseError::ExecTooLarge,
                            QStringLiteral("Exec exceeds the execution ceiling"));
      }
      keys.exec = *value;
    } else if (key == QLatin1String("Path")) {
      if (value->size() > ExecutionBounds::maxPathCodeUnits) {
        return parseFailure(ExecutionParseError::PathTooLarge,
                            QStringLiteral("Path exceeds the execution ceiling"));
      }
      keys.path = *value;
    } else {
      bool flag = false;
      if (!parseBoolean(*value, &flag)) {
        return parseFailure(ExecutionParseError::InvalidBoolean,
                            QStringLiteral("execution booleans must be true or false"));
      }
      if (key == QLatin1String("Terminal"))
        keys.terminal = flag;
      else
        keys.dbusActivatable = flag;
    }
  }

  if (!foundGroup) {
    return parseFailure(ExecutionParseError::GroupNotFound,
                        QStringLiteral("no matching desktop entry group"));
  }
  if (keys.exec.isEmpty() && !keys.dbusActivatable) {
    return parseFailure(ExecutionParseError::MissingExec,
                        QStringLiteral("entry has no Exec and is not D-Bus activatable"));
  }
  return ExecutionParseResult { keys, ExecutionParseError::None, {} };
}

ExecPlanResult ExecFieldCodeExpander::expand(const QString &decodedExec,
                                             const ExecExpansionValues &values)
{
  if (decodedExec.size() > ExecutionBounds::maxExecCodeUnits) {
    return planFailure(ExecPlanError::ExecTooLarge,
                       QStringLiteral("Exec exceeds the execution ceiling"));
  }
  TokenizeError tokenizeError = TokenizeError::None;
  const auto tokens = tokenize(decodedExec, &tokenizeError);
  if (!tokens) {
    return planFailure(ExecPlanError::UnterminatedQuote,
                       QStringLiteral("Exec has an unterminated quoted argument"));
  }

  QStringList argv;
  qsizetype totalCodeUnits = 0;
  for (const QString &token : *tokens) {
    QStringList replacement;
    ExecPlanError tokenError = ExecPlanError::None;
    if (!expandToken(token, values, &replacement, &tokenError)) {
      return planFailure(tokenError,
                         QStringLiteral("Exec contains a field code the launcher cannot satisfy"));
    }
    for (const QString &argument : replacement)
      totalCodeUnits += argument.size();
    argv.append(replacement);
    if (argv.size() > ExecutionBounds::maxArguments
        || totalCodeUnits > ExecutionBounds::maxExpandedCodeUnits) {
      return planFailure(ExecPlanError::ArgumentLimitReached,
                         QStringLiteral("expanded Exec exceeds the argument ceiling"));
    }
  }

  if (argv.isEmpty() || argv.constFirst().isEmpty()) {
    return planFailure(ExecPlanError::MissingProgram,
                       QStringLiteral("Exec has no program after field-code expansion"));
  }
  ExecPlan plan;
  plan.program = argv.constFirst();
  plan.arguments = argv.mid(1);
  return ExecPlanResult { plan, ExecPlanError::None, {} };
}

} // namespace QindaQt::Shell::Launcher
