// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"
#include "proton_catalog.h"
#include "wine_source.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the one-time upgrade of hand-added entries to ADR-0275
// pins. Before ADR-0275 a Proton entry could hold no pin ("Any discovered
// Proton") or a pin without a version. The first time the controller sees
// such an entry whose EFFECTIVE runner is Proton (its own runner, or a
// runnerOverride in its launch options) it records, exactly once:
//   - no pin: the current default build (pinForNewEntry), name + version;
//   - a pin without a version: the version of the build that pin names
//     exactly right now (confirmPinnedBuild) -- a legacy path stays a path.
// Each change yields one note ("<title> is now pinned to <build>"), which
// the caller shows. Entries that already carry name + version, Wine-runner
// entries, and pins that do not resolve are left untouched (a launch then
// refuses with its own reason). Pure: the caller persists and reports.

struct WinePinMigration final {
  QVector<WineEntryRecord> records;
  QStringList notes; // one per migrated entry, capped
  bool changed = false;
};

[[nodiscard]] WinePinMigration migrateWineEntryPins(
    const QVector<WineEntryRecord> &records,
    const QHash<QString, LaunchOptions> &options,
    const QVector<ProtonBuild> &builds, const QString &preferredName);

} // namespace QindaQt::QindaLutris
