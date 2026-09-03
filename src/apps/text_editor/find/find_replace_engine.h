// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

namespace QindaQt::Apps::TextEditor {

struct FindOptions final {
  QString pattern;
  bool caseSensitive = false;
  bool wholeWord = false;
  bool regularExpression = false;
};

struct FindMatch final {
  qsizetype start = 0;
  qsizetype length = 0;

  [[nodiscard]] bool operator==(const FindMatch &) const = default;
};

enum class FindError {
  None,
  EmptyPattern,
  PatternTooLong,
  UnsafeRegularExpression,
  InvalidRegularExpression,
  MatchLimitExceeded,
};

struct FindResult final {
  QVector<FindMatch> matches;
  int currentIndex = -1;
  bool wrapped = false;
  FindError error = FindError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == FindError::None; }
  [[nodiscard]] bool hasMatch() const {
    return ok() && currentIndex >= 0 && currentIndex < matches.size();
  }
};

enum class FindDirection { Next, Previous };

// Pure bounded search policy. User regexes are admitted only from a
// deliberately linear subset: character classes, anchors, literals, escapes,
// and dot. Quantifiers, groups, alternation, lookaround, and backreferences are
// rejected before QRegularExpression sees document text, preventing hostile
// backtracking from occupying the GUI thread.
class FindReplaceEngine final {
public:
  static constexpr qsizetype maximumPatternLength = 256;
  static constexpr qsizetype maximumMatches = 10'000;

  [[nodiscard]] static FindResult find(const QString &text,
                                       const FindOptions &options,
                                       qsizetype cursorPosition,
                                       FindDirection direction);
};

} // namespace QindaQt::Apps::TextEditor
