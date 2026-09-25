// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"
#include "title_record.h"

#include <QString>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the library slice for titles QindaLutris installed or
// adopted (ADR-0275). Kept apart from title_store.h, which only persists
// records, and from umu_launch.h, which only plans launches: this file
// turns stored records into the Game rows the library shows.

// One Game per record (GameSource::Installed): id and title from the
// record, sourceRef = the record id, installPath = executable, winePrefix =
// prefix, protonPath = the pinned build name, wineRunner = Proton. The
// cover is the executable's icon, extracted into cacheDir like a hand-added
// entry's (executableCoverPath). Install size stays "not tracked": the
// executable's size is not the game's size (ADR-0231 honesty rule).
// Bounded by kMaxTitles; invalid records are skipped, never guessed at.
[[nodiscard]] QVector<Game> gamesFromTitleRecords(
    const QVector<TitleRecord> &records, const QString &cacheDir);

} // namespace QindaQt::QindaLutris
