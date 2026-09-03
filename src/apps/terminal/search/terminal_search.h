// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace QindaQt::Apps::Terminal {

struct TerminalSearchQuery final {
  QString pattern;
  bool caseSensitive = false;
  bool regularExpression = false;

  [[nodiscard]] bool operator==(const TerminalSearchQuery &) const = default;
};

enum class TerminalSearchDirection { Initial, Next, Previous };

struct TerminalSearchResult final {
  bool accepted = false;
  bool found = false;
  int current = 0;
  int total = 0;
  bool wrapped = false;
  QString diagnostic;
};

struct TerminalSearchSpan final {
  qsizetype start = 0;
  qsizetype length = 0;
};

struct TerminalSearchScan final {
  TerminalSearchResult result;
  QList<TerminalSearchSpan> matches;
};

// AGENT-CONTRACT: This pure policy is the admission gate in front of the
// pinned qtermwidget search implementation. It accepts at most 256 UTF-16
// code units and a deliberately bounded regex subset, rejects empty matches,
// and caps the result inventory. The adapter must never invoke qtermwidget's
// synchronous regex search before this function accepts the same pattern.
[[nodiscard]] TerminalSearchScan
scanTerminalText(const QString &text, const TerminalSearchQuery &query);

inline constexpr qsizetype kTerminalSearchPatternLimit = 256;
inline constexpr qsizetype kTerminalSearchSnapshotByteLimit = 4 * 1024 * 1024;
inline constexpr int kTerminalSearchMatchLimit = 10000;

} // namespace QindaQt::Apps::Terminal
