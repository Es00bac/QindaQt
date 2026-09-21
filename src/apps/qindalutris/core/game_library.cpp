// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_library.h"

#include <QHash>
#include <QSet>

namespace QindaQt::QindaLutris {
namespace {

bool titleLess(const Game &a, const Game &b) {
  const int byTitle = QString::compare(a.title, b.title, Qt::CaseInsensitive);
  if (byTitle != 0) {
    return byTitle < 0;
  }
  return a.id < b.id;
}

} // namespace

GameLibrary mergeGameSources(QVector<Game> steam, QVector<Game> lutris,
                             QVector<Game> desktop, QVector<Game> wine,
                             QStringList warnings) {
  GameLibrary out;
  // AGENT-GUARD: the de-dup key is the normalized title, and Steam claims
  // the row. Keep the Steam set complete BEFORE folding: two Steam games
  // with the same normalized title both survive (they are distinct appids);
  // only the Lutris copy is dropped.
  QSet<QString> steamTitles;
  for (const Game &game : steam) {
    steamTitles.insert(normalizedTitleForMatch(game.title));
  }

  QSet<QString> seenIds;
  const auto take = [&out, &seenIds](const QVector<Game> &games) {
    for (const Game &game : games) {
      if (out.games.size() >= kMaxGames) {
        return;
      }
      if (seenIds.contains(game.id)) {
        continue;
      }
      seenIds.insert(game.id);
      out.games.append(game);
    }
  };

  take(steam);
  for (const Game &game : lutris) {
    if (out.games.size() >= kMaxGames) {
      break;
    }
    if (steamTitles.contains(normalizedTitleForMatch(game.title))) {
      continue; // One entry, and it is the Steam one (ADR-0231).
    }
    if (seenIds.contains(game.id)) {
      continue;
    }
    seenIds.insert(game.id);
    out.games.append(game);
  }
  take(desktop);
  take(wine);

  std::sort(out.games.begin(), out.games.end(), titleLess);
  out.warnings = warnings.mid(0, 32);
  return out;
}

} // namespace QindaQt::QindaLutris
