// SPDX-License-Identifier: GPL-3.0-or-later
#include "search/terminal_search.h"

#include <QRegularExpression>

namespace QindaQt::Apps::Terminal {
namespace {

[[nodiscard]] QString validatePattern(const TerminalSearchQuery &query,
                                      QRegularExpression *expression) {
  if (query.pattern.isEmpty()) {
    return QStringLiteral("Enter text to search");
  }
  if (query.pattern.size() > kTerminalSearchPatternLimit) {
    return QStringLiteral("Search pattern exceeds 256 characters");
  }
  for (qsizetype index = 0; index < query.pattern.size(); ++index) {
    const QChar character = query.pattern.at(index);
    if (character.isHighSurrogate() && index + 1 < query.pattern.size() &&
        query.pattern.at(index + 1).isLowSurrogate()) {
      ++index;
      continue;
    }
    if (character.isNull() || character.category() == QChar::Other_Control ||
        character.isHighSurrogate() || character.isLowSurrogate()) {
      return QStringLiteral("Search pattern contains unsupported characters");
    }
  }

  QString admitted = query.regularExpression
                         ? query.pattern
                         : QRegularExpression::escape(query.pattern);
  if (query.regularExpression) {
    bool escaped = false;
    bool inClass = false;
    int variableQuantifiers = 0;
    int optionalQuantifiers = 0;
    for (qsizetype index = 0; index < admitted.size(); ++index) {
      const QChar character = admitted.at(index);
      if (escaped) {
        if (character.isDigit() || character == QLatin1Char('k') ||
            character == QLatin1Char('g')) {
          return QStringLiteral("Regex backreferences are not supported");
        }
        escaped = false;
        continue;
      }
      if (character == QLatin1Char('\\')) {
        escaped = true;
        continue;
      }
      if (character == QLatin1Char('[')) {
        inClass = true;
        continue;
      }
      if (character == QLatin1Char(']') && inClass) {
        inClass = false;
        continue;
      }
      if (inClass) {
        continue;
      }
      if (character == QLatin1Char('(') && index + 1 < admitted.size() &&
          (admitted.at(index + 1) == QLatin1Char('?') ||
           admitted.at(index + 1) == QLatin1Char('*'))) {
        return QStringLiteral("Regex extensions are not supported");
      }
      const QChar previous = index > 0 ? admitted.at(index - 1) : QChar();
      if (character == QLatin1Char('*') || character == QLatin1Char('+')) {
        if (++variableQuantifiers > 1 || previous == QLatin1Char(')') ||
            previous == QLatin1Char('*') || previous == QLatin1Char('+') ||
            previous == QLatin1Char('?')) {
          return QStringLiteral("Regex repetition is too complex");
        }
      } else if (character == QLatin1Char('?')) {
        if (++optionalQuantifiers > 8 || previous == QLatin1Char(')') ||
            previous == QLatin1Char('*') || previous == QLatin1Char('+') ||
            previous == QLatin1Char('?')) {
          return QStringLiteral("Regex repetition is too complex");
        }
      } else if (character == QLatin1Char('{')) {
        const qsizetype close = admitted.indexOf(QLatin1Char('}'), index + 1);
        if (close < 0 || previous == QLatin1Char(')')) {
          return QStringLiteral("Regex repetition is malformed");
        }
        bool countOk = false;
        const int count = admitted.mid(index + 1, close - index - 1)
                              .toInt(&countOk);
        if (!countOk || count < 1 || count > 32) {
          return QStringLiteral(
              "Regex repetitions must be exact and no greater than 32");
        }
        index = close;
      }
    }
    if (escaped || inClass) {
      return QStringLiteral("Regular expression is incomplete");
    }
  }

  QRegularExpression::PatternOptions options;
  if (!query.caseSensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }
  *expression = QRegularExpression(admitted, options);
  if (!expression->isValid()) {
    return QStringLiteral("Invalid regular expression: %1")
        .arg(expression->errorString().left(384));
  }
  if (expression->match(QString()).hasMatch()) {
    return QStringLiteral("Expressions that match empty text are not supported");
  }
  return {};
}

} // namespace

TerminalSearchScan scanTerminalText(const QString &text,
                                    const TerminalSearchQuery &query) {
  TerminalSearchScan scan;
  QRegularExpression expression;
  const QString diagnostic = validatePattern(query, &expression);
  if (!diagnostic.isEmpty()) {
    scan.result.diagnostic = diagnostic;
    return scan;
  }

  scan.result.accepted = true;
  QRegularExpressionMatchIterator matches = expression.globalMatch(text);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    if (scan.matches.size() >= kTerminalSearchMatchLimit) {
      scan.matches.clear();
      scan.result.accepted = false;
      scan.result.diagnostic =
          QStringLiteral("Search has more than 10000 matches; narrow it");
      return scan;
    }
    scan.matches.append(
        {.start = match.capturedStart(), .length = match.capturedLength()});
  }
  scan.result.total = static_cast<int>(scan.matches.size());
  scan.result.found = !scan.matches.isEmpty();
  scan.result.current = scan.result.found ? 1 : 0;
  scan.result.diagnostic = scan.result.found
                               ? QString()
                               : QStringLiteral("No matches");
  return scan;
}

} // namespace QindaQt::Apps::Terminal
