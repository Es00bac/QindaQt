// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: bounded, strictly read-only discovery against Lutris's own
// library database (ADR-0231). The operator's real Lutris must keep working
// exactly as it does now, so this adapter opens pga.db through SQLite's
// immutable read-only URI, never writes, never recovers a journal, never
// shells out to the `lutris` binary, and never imports its Python. A missing
// file, a locked file, a schema without the columns we read, or a row with a
// null name degrades to zero or fewer games plus a warning string -- never an
// exception, an error dialog, or a write to Lutris's data.

struct LutrisDiscovery final {
  QVector<Game> games;
  QStringList warnings;
};

// dbPath is injected (production: $XDG_DATA_HOME/lutris/pga.db). Only rows
// with installed = 1 become games. Row count, string lengths and warning
// count are capped; cover art is looked up under the database's sibling
// coverart/ and banners/ directories by fixed "<slug>.jpg" name only.
[[nodiscard]] LutrisDiscovery scanLutrisDatabase(const QString &dbPath);

// The fixed column set this reader requires of Lutris's `games` table. A
// database whose schema lacks any of them yields zero games.
[[nodiscard]] QStringList requiredLutrisGameColumns();

} // namespace QindaQt::QindaLutris
