// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: "offer the executables the installer created for the user
// to confirm as the game" (ADR-0275 section 4, any setup file). The scan is
// bounded (entries and depth), never follows symlinks, and looks only where
// Windows installers put programs:
//   drive_c/Program Files, drive_c/Program Files (x86),
//   drive_c/users/*/AppData/Local/Programs, drive_c/Games, drive_c/GOG Games.
// The ranking is a suggestion; the UI must let the user confirm or choose.

struct ExecutableScanLimits final {
  int maxEntries = 50000;
  int maxDepth = 10;
};

// unix path -> size in bytes, for every *.exe under the roots above.
using ExecutableSnapshot = QHash<QString, qint64>;

[[nodiscard]] ExecutableSnapshot scanPrefixExecutables(const QString &prefixDir,
                                                       ExecutableScanLimits limits = {});

// Installer helpers that are never the game: unins*, *setup*, *uninstall*,
// *installer*, crash reporters and handlers, redistributables, prerequisite
// installers, bug reporters. Case-insensitive on the file name.
[[nodiscard]] bool isHelperExecutableName(const QString &fileName);

struct ExecutableCandidate final {
  QString unixPath;
  qint64 sizeBytes = 0;
  int score = 0;

  friend bool operator==(const ExecutableCandidate &, const ExecutableCandidate &) = default;
};

// Executables new in `after` (or whose size changed), helpers removed,
// ranked by name similarity to the title (file and parent folders), then
// size, then path. At most maxCandidates.
[[nodiscard]] QVector<ExecutableCandidate> rankNewExecutables(const ExecutableSnapshot &before,
                                                              const ExecutableSnapshot &after,
                                                              const QString &title,
                                                              int maxCandidates = 10);

} // namespace QindaQt::QindaLutris
