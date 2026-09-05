// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/command_search_types.h"

#include <algorithm>

namespace QindaQt::Shell::DesktopControls {

QString commandSourceKindText(CommandSourceKind kind)
{
  switch (kind) {
  case CommandSourceKind::Applications:
    return QStringLiteral("application");
  case CommandSourceKind::MenuActions:
    return QStringLiteral("menuAction");
  case CommandSourceKind::Windows:
    return QStringLiteral("window");
  case CommandSourceKind::Workspaces:
    break;
  }
  return QStringLiteral("workspace");
}

QString normalizeCommandQuery(const QString &query)
{
  const QString bounded = query.left(CommandSearchBounds::maxQueryLength);
  return bounded.simplified();
}

CommandMatch matchCommand(const QString &normalizedQuery,
                          const CommandCandidate &candidate)
{
  if (normalizedQuery.isEmpty()) {
    return CommandMatch::TextSubstring;
  }
  const QString text = candidate.text.simplified();
  if (text.startsWith(normalizedQuery, Qt::CaseInsensitive)) {
    return CommandMatch::TextPrefix;
  }
  const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString &word : words) {
    if (word.startsWith(normalizedQuery, Qt::CaseInsensitive)) {
      return CommandMatch::WordStart;
    }
  }
  if (text.contains(normalizedQuery, Qt::CaseInsensitive)) {
    return CommandMatch::TextSubstring;
  }
  if (candidate.detail.contains(normalizedQuery, Qt::CaseInsensitive)) {
    return CommandMatch::DetailSubstring;
  }
  return CommandMatch::None;
}

QList<CommandCandidate> rankCommands(const QString &query,
                                     const QList<CommandCandidate> &candidates,
                                     int maxResults)
{
  const QString normalized = normalizeCommandQuery(query);
  struct Scored {
    CommandMatch match;
    int order;
    const CommandCandidate *candidate;
  };
  QList<Scored> scored;
  scored.reserve(candidates.size());
  int order = 0;
  for (const CommandCandidate &candidate : candidates) {
    const CommandMatch match = matchCommand(normalized, candidate);
    if (match != CommandMatch::None) {
      scored.append({match, order, &candidate});
    }
    ++order;
  }
  std::stable_sort(scored.begin(), scored.end(),
                   [](const Scored &left, const Scored &right) {
                     if (left.match != right.match) {
                       return static_cast<int>(left.match)
                           < static_cast<int>(right.match);
                     }
                     return left.order < right.order;
                   });
  QList<CommandCandidate> results;
  const int limit = std::max(0, maxResults);
  for (const Scored &entry : scored) {
    if (results.size() >= limit) {
      break;
    }
    results.append(*entry.candidate);
  }
  return results;
}

} // namespace QindaQt::Shell::DesktopControls
