// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

namespace QindaQt::ShellLauncher {

// Match quality, ordered from strongest to weakest. The enum order is the
// score order; rankers must not rely on any numeric encoding outside this
// ordering.
enum class SearchMatchKind {
  NamePrefix,
  NameWordStart,
  NameSubstring,
  KeywordPrefix,
  KeywordSubstring,
  GenericNameSubstring,
  CommentSubstring,
};

struct RankedEntry {
  ApplicationEntry entry;
  SearchMatchKind match = SearchMatchKind::CommentSubstring;

  friend bool operator==(const RankedEntry &, const RankedEntry &) = default;
};

enum class SearchErrorCode {
  None,
  EmptyQuery,
  QueryTooLong,
};

struct SearchOutcome {
  QVector<RankedEntry> results;
  SearchErrorCode error = SearchErrorCode::None;

  bool ok() const { return error == SearchErrorCode::None; }
};

// Deterministic, locale-independent search over validated entries.
// AGENT-NOTE: Case folding uses Qt::CaseInsensitive comparisons only; a
// locale-aware collation would make result order differ between users for
// identical input, which the launcher never accepts.
class LauncherSearchRanker {
public:
  // The query is trimmed and its internal whitespace runs are collapsed
  // before matching. An empty or blank query is an EmptyQuery error, not a
  // request for the full catalog; callers wanting the default listing use the
  // catalog or category model directly.
  static SearchOutcome search(const QVector<ApplicationEntry> &entries,
                              const QString &query);
};

} // namespace QindaQt::ShellLauncher
