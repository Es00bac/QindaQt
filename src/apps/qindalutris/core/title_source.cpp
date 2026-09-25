// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_source.h"

#include "wine_source.h"

namespace QindaQt::QindaLutris {

QVector<Game> gamesFromTitleRecords(const QVector<TitleRecord> &records,
                                    const QString &cacheDir) {
  QVector<Game> out;
  out.reserve(qMin(records.size(), qsizetype(kMaxTitles)));
  for (const TitleRecord &record : records) {
    if (out.size() >= kMaxTitles) {
      break;
    }
    if (!validateTitleRecord(record, nullptr)) {
      continue;
    }
    Game game;
    game.id = record.id;
    game.title = record.title.left(kMaxGameTitleChars);
    game.source = GameSource::Installed;
    game.sourceRef = record.id;
    game.installPath = record.executable;
    game.winePrefix = record.prefixPath;
    game.wineRunner = WineRunner::Proton;
    game.protonPath = record.protonBuild;
    // "title/<slug>" -> "title-<slug>": a cache key is one path component.
    QString cacheKey = record.id;
    cacheKey.replace(QLatin1Char('/'), QLatin1Char('-'));
    game.coverPath = executableCoverPath(cacheKey, record.executable, cacheDir);
    out.append(game);
  }
  return out;
}

} // namespace QindaQt::QindaLutris
