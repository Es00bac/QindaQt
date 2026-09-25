// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"

#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the merge rule of ADR-0231. Independently optional
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

// Merges the already-read source results. Inputs are taken by value of
// their games vectors to keep the merge pure and trivially testable.
// `installed` are the ADR-0275 titles (GameSource::Installed); they are not
// title-folded against anything -- an installed launcher and a Lutris row of
// the same name are different ways to start different things -- and they
// are merged first, so the kMaxGames cap never drops one.
[[nodiscard]] GameLibrary mergeGameSources(QVector<Game> steam,
                                           QVector<Game> lutris,
                                           QVector<Game> desktop,
                                           QVector<Game> wine,
                                           QVector<Game> installed,
                                           QStringList warnings);

} // namespace QindaQt::QindaLutris
