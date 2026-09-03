// SPDX-License-Identifier: GPL-3.0-or-later
#include "find_replace_engine.h"

#include <QRegularExpression>

namespace QindaQt::Apps::TextEditor {
namespace {

bool isSafeRegexSubset(const QString &pattern) {
  bool escaped = false;
  bool inClass = false;
  for (const QChar character : pattern) {
    if (escaped) {
      // Numeric escapes are backreferences in PCRE syntax.
      if (character.isDigit()) {
        return false;
      }
      escaped = false;
      continue;
    }
    if (character == u'\\') {
      escaped = true;
      continue;
    }
    if (character == u'[' && !inClass) {
      inClass = true;
      continue;
    }
    if (character == u']' && inClass) {
      inClass = false;
      continue;
    }
    if (!inClass && QStringLiteral("*+?{}()|").contains(character)) {
      return false;
    }
  }
  return !escaped && !inClass;
}

FindResult failure(const FindError error, const QString &diagnostic) {
  FindResult result;
  result.error = error;
  result.diagnostic = diagnostic;
  return result;
}

QRegularExpression buildExpression(const FindOptions &options,
                                   FindResult *failureResult) {
  if (options.pattern.isEmpty()) {
    *failureResult =
        failure(FindError::EmptyPattern, QStringLiteral("Enter text to find"));
    return {};
  }
  if (options.pattern.size() > FindReplaceEngine::maximumPatternLength) {
    *failureResult =
        failure(FindError::PatternTooLong,
                QStringLiteral("Find pattern exceeds 256 characters"));
    return {};
  }
  if (options.regularExpression && !isSafeRegexSubset(options.pattern)) {
    *failureResult = failure(
        FindError::UnsafeRegularExpression,
        QStringLiteral("Regular expression uses an unbounded construct"));
    return {};
  }

  QString expression = options.regularExpression
                           ? options.pattern
                           : QRegularExpression::escape(options.pattern);
  if (options.wholeWord) {
    expression = QStringLiteral("(?<![\\p{L}\\p{N}_])(?:%1)(?![\\p{L}\\p{N}_])")
                     .arg(expression);
  }
  QRegularExpression::PatternOptions patternOptions =
      QRegularExpression::UseUnicodePropertiesOption |
      QRegularExpression::DontCaptureOption;
  if (!options.caseSensitive) {
    patternOptions |= QRegularExpression::CaseInsensitiveOption;
  }
  QRegularExpression compiled(expression, patternOptions);
  if (!compiled.isValid()) {
    *failureResult = failure(FindError::InvalidRegularExpression,
                             compiled.errorString().left(256));
    return {};
  }
  return compiled;
}

} // namespace

FindResult FindReplaceEngine::find(const QString &text,
                                   const FindOptions &options,
                                   qsizetype cursorPosition,
                                   const FindDirection direction) {
  FindResult result;
  const QRegularExpression expression = buildExpression(options, &result);
  if (!result.ok()) {
    return result;
  }

  auto iterator = expression.globalMatch(text);
  while (iterator.hasNext()) {
    const QRegularExpressionMatch match = iterator.next();
    if (!match.hasMatch()) {
      continue;
    }
    if (result.matches.size() >= maximumMatches) {
      return failure(
          FindError::MatchLimitExceeded,
          QStringLiteral("More than 10000 matches; narrow the search"));
    }
    result.matches.append(
        {.start = match.capturedStart(), .length = match.capturedLength()});
  }
  if (result.matches.isEmpty()) {
    result.diagnostic = QStringLiteral("No matches");
    return result;
  }

  cursorPosition = qBound<qsizetype>(0, cursorPosition, text.size());
  if (direction == FindDirection::Next) {
    for (int index = 0; index < result.matches.size(); ++index) {
      if (result.matches.at(index).start >= cursorPosition) {
        result.currentIndex = index;
        return result;
      }
    }
    result.currentIndex = 0;
    result.wrapped = true;
  } else {
    for (int index = static_cast<int>(result.matches.size()) - 1; index >= 0;
         --index) {
      const FindMatch &match = result.matches.at(index);
      if (match.start + match.length <= cursorPosition) {
        result.currentIndex = index;
        return result;
      }
    }
    result.currentIndex = static_cast<int>(result.matches.size()) - 1;
    result.wrapped = true;
  }
  return result;
}

} // namespace QindaQt::Apps::TextEditor
