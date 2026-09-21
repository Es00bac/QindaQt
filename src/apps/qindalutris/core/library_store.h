// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"
#include "wine_source.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: QindaLutris's durable state is app-local JSON under
// $XDG_CONFIG_HOME/qindaqt/qindalutris (ADR-0231, on the ADR-0198 precedent:
// an open-ended per-game key space does not belong in Settings1, which
// rejects a whole snapshot on one unknown key). Two documents, both exact
// and both bounded: wine-entries-v1.json (the hand-added games) and
// launch-options-v1.json (per-game tuning keyed by stable game id). Reads
// refuse a document from a newer version, an oversized document, a symlinked
// path, or any out-of-set value WHOLE -- a refused document leaves defaults
// standing rather than a half-understood mixture. Writes commit atomically
// through QSaveFile in the same directory. The root is injected, so tests
// never touch the real home.

class LibraryStore final {
public:
  enum class Error {
    None,
    Absent,       // first run; not a failure
    Refused,      // hostile/corrupt/newer document; defaults stand
    WriteFailed,
  };

  explicit LibraryStore(QString configRoot);

  [[nodiscard]] QString wineEntriesPath() const;
  [[nodiscard]] QString launchOptionsPath() const;

  [[nodiscard]] QVector<WineEntryRecord> readWineEntries(Error *error) const;
  [[nodiscard]] Error writeWineEntries(const QVector<WineEntryRecord> &records) const;

  // Options keyed by Game::id. Ids referencing games that no longer exist
  // are harmless: they stay stored and apply again if the game returns.
  [[nodiscard]] QHash<QString, LaunchOptions> readLaunchOptions(Error *error) const;
  [[nodiscard]] Error writeLaunchOptions(
      const QHash<QString, LaunchOptions> &options) const;

private:
  QString m_root;
};

// One extra-environment line ("KEY=VALUE") against the launch-time rules:
// bounded length, a C-identifier key, no control characters. Empty means
// invalid. Pure and shared by the store reader, the planner, and the UI.
[[nodiscard]] bool isValidEnvironmentAssignment(const QString &line);

} // namespace QindaQt::QindaLutris
