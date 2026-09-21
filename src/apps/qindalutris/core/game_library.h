// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"

#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the merge rule of ADR-0231. Four independently optional
// sources contribute games; the result is one deterministic, title-sorted
// list. A Lutris row whose normalized title matches a Steam game's is folded
// into the STEAM record (the Steam record carries the honest install anchor
// and the launch path that needs no extra runner), so the game appears once.
// No source being present is a normal state: the merge of nothing is an
// empty library with zero warnings, which the UI renders as an honest empty
// state.

struct GameLibrary final {
  QVector<Game> games;
  // Source-parallel degradation notes from every reader, capped overall.
  QStringList warnings;
};

// Merges the four already-read source results. Inputs are taken by value of
// their games vectors to keep the merge pure and trivially testable.
[[nodiscard]] GameLibrary mergeGameSources(QVector<Game> steam,
                                           QVector<Game> lutris,
                                           QVector<Game> desktop,
                                           QVector<Game> wine,
                                           QStringList warnings);

} // namespace QindaQt::QindaLutris
