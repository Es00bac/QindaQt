// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"

#include <QString>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the hand-added Wine/Proton slice of the library
// (ADR-0231). The user names a Windows executable, a prefix, and a runner;
// the entry persists app-locally (see library_store.h) and its cover is the
// icon extracted from the executable by the compositor's public, bounded PE
// parser (QindaQt::Compositor::extractPeIcon, ADR-0230) -- linked, never
// copied. A missing or icon-less executable is still a valid entry; only its
// cover stays empty.

// One stored manual entry (pre-Game form).
struct WineEntryRecord final {
  QString slug;     // stable id suffix, assigned at add time ("wine/<slug>")
  QString title;
  QString executablePath;
  QString prefixPath;
  WineRunner runner = WineRunner::Wine;
  // The pinned Proton build (ADR-0275): a build directory name for entries
  // added since ADR-0275, or the absolute `proton` script path older
  // entries stored. Empty = no build chosen; a Proton launch refuses.
  QString protonPath;
  // The pinned build's version text (identity = name + version). Empty in
  // entries saved before it was recorded; wine_pin_migration fills it once.
  QString protonVersion;

  friend bool operator==(const WineEntryRecord &, const WineEntryRecord &) = default;
};

// Turns stored records into games. The executable is statted (bounded, no
// directory walk): when present, install size is its byte size -- the only
// source whose size is honestly knowable (ADR-0231) -- and the icon is
// extracted into cacheDir as "<slug>.png" when absent or stale. cacheDir is
// injected; extraction failures leave the cover empty, never an error.
[[nodiscard]] QVector<Game> gamesFromWineEntries(
    const QVector<WineEntryRecord> &records, const QString &cacheDir);

// The cached cover for a Windows executable: its PE icon, extracted into
// cacheDir as "<cacheKey>.png" when absent or older than the executable.
// cacheKey must be a single path component. Empty when the file is missing,
// not a PE, or has no icon -- never an error. Shared by hand-added entries
// and installed titles (title_source.h).
[[nodiscard]] QString executableCoverPath(const QString &cacheKey,
                                          const QString &executablePath,
                                          const QString &cacheDir);

// The deterministic slug for a new entry: normalized title plus a short
// hash of the executable path so two executables sharing a title stay
// distinct. Pure.
[[nodiscard]] QString wineSlugFor(const QString &title,
                                  const QString &executablePath);

} // namespace QindaQt::QindaLutris
