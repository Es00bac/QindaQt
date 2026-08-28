// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/launcher_search_ranker.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QRegularExpression>

#include <algorithm>
#include <optional>

namespace QindaQt::ShellLauncher {
namespace {

QString normalizeQuery(const QString &query)
{
  static const QRegularExpression whitespaceRun(QStringLiteral("\\s+"));
  QString normalized = query.trimmed();
  normalized.replace(whitespaceRun, QStringLiteral(" "));
  return normalized;
}

bool containsCaseInsensitive(const QString &haystack, const QString &needle)
{
  return haystack.indexOf(needle, 0, Qt::CaseInsensitive) >= 0;
}

bool startsWithCaseInsensitive(const QString &haystack, const QString &needle)
{
  return haystack.startsWith(needle, Qt::CaseInsensitive);
}

// A word start is position zero or a position preceded by whitespace; the
// normalized query guarantees single-space separation so this stays simple.
bool isWordStartMatch(const QString &haystack, const QString &needle)
{
  qsizetype index = haystack.indexOf(needle, 0, Qt::CaseInsensitive);
  while (index > 0) {
    if (haystack.at(index - 1).isSpace())
      return true;
    index = haystack.indexOf(needle, index + 1, Qt::CaseInsensitive);
  }
  return index == 0;
}

std::optional<SearchMatchKind> classify(const ApplicationEntry &entry,
                                        const QString &query)
{
  if (startsWithCaseInsensitive(entry.name, query))
    return SearchMatchKind::NamePrefix;
  if (isWordStartMatch(entry.name, query))
    return SearchMatchKind::NameWordStart;
  if (containsCaseInsensitive(entry.name, query))
    return SearchMatchKind::NameSubstring;
  for (const QString &keyword : entry.keywords) {
    if (startsWithCaseInsensitive(keyword, query))
      return SearchMatchKind::KeywordPrefix;
  }
  for (const QString &keyword : entry.keywords) {
    if (containsCaseInsensitive(keyword, query))
      return SearchMatchKind::KeywordSubstring;
  }
  if (containsCaseInsensitive(entry.genericName, query))
    return SearchMatchKind::GenericNameSubstring;
  if (containsCaseInsensitive(entry.comment, query))
    return SearchMatchKind::CommentSubstring;
  return std::nullopt;
}

bool rankedLessThan(const RankedEntry &left, const RankedEntry &right)
{
  if (left.match != right.match)
    return left.match < right.match;
  const int nameCompare = left.entry.name.compare(right.entry.name, Qt::CaseInsensitive);
  if (nameCompare != 0)
    return nameCompare < 0;
  return left.entry.id < right.entry.id;
}

} // namespace

SearchOutcome LauncherSearchRanker::search(const QVector<ApplicationEntry> &entries,
                                           const QString &query)
{
  if (query.size() > Bounds::maxQueryLength) {
    return { {}, SearchErrorCode::QueryTooLong };
  }
  const QString normalized = normalizeQuery(query);
  if (normalized.isEmpty())
    return { {}, SearchErrorCode::EmptyQuery };

  QVector<RankedEntry> results;
  for (const ApplicationEntry &entry : entries) {
    const std::optional<SearchMatchKind> match = classify(entry, normalized);
    if (match)
      results.append(RankedEntry { entry, *match });
  }
  std::sort(results.begin(), results.end(), rankedLessThan);
  return { results, SearchErrorCode::None };
}

} // namespace QindaQt::ShellLauncher
