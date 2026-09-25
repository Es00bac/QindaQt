// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: Windows paths inside a Wine/Proton prefix. umu-run keeps
// `drive_c` directly under WINEPREFIX (it links `pfx` back to the prefix
// itself), and Lutris-made prefixes such as ~/Games/battlenet have the same
// layout, so `C:\X\Y` maps to `<prefix>/drive_c/X/Y`. Proton prefixes name
// the Windows user `steamuser` (so recipes write `C:\users\steamuser\...`).

// `C:\Program Files\A\b.exe` -> `<prefix>/drive_c/Program Files/A/b.exe`.
// Accepts `\` or `/` separators and any drive letter (-> drive_<letter>).
// Returns empty -- never a guess -- for relative paths, UNC paths, a
// relative prefix, or any `.`/`..` segment. A `*` segment passes through
// verbatim for expandPrefixCandidate(). Pure.
[[nodiscard]] QString windowsPathToPrefixPath(const QString &prefixDir,
                                              const QString &windowsPath);

// Expands whole-segment `*` wildcards (e.g. a versioned EA Desktop folder)
// against the filesystem, returning existing regular files, highest name
// first per level, at most maxMatches. Hidden entries and symlinked
// directories are not descended into.
[[nodiscard]] QStringList expandPrefixCandidate(const QString &unixPattern,
                                                int maxMatches = 16);

// The first launcher candidate (Windows paths, in recipe order) that exists
// as a regular file inside the prefix; empty when none does.
[[nodiscard]] QString firstExistingCandidate(const QString &prefixDir,
                                             const QStringList &windowsCandidates);

} // namespace QindaQt::QindaLutris
