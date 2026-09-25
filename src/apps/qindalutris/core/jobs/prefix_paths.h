// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: Windows paths inside a Wine/Proton prefix. umu-run's
// setup_pfx() makes `<WINEPREFIX>/pfx` a symlink to the prefix itself, so
// `drive_c` sits directly under WINEPREFIX (as in Lutris-made prefixes such
// as ~/Games/battlenet); `C:\X\Y` maps to `<prefix>/drive_c/X/Y`. Proton
// names the Windows user `steamuser` (umu links the login name to it), so
// recipes write `C:\users\steamuser\...`.

// `C:\Program Files\A\b.exe` -> `<prefix>/drive_c/Program Files/A/b.exe`.
// Accepts `\` or `/` separators; only drive C: maps (other letters are
// dosdevices links Wine creates per machine, never a fixed folder). Returns
// empty -- never a guess -- for other drives, relative or UNC paths, a
// relative prefix, or any `.`/`..` segment. A `*` segment passes through
// verbatim for expandPrefixCandidate(). Pure.
[[nodiscard]] QString windowsPathToPrefixPath(const QString &prefixDir,
                                              const QString &windowsPath);

// Expands whole-segment `*` wildcards (e.g. a versioned EA Desktop folder)
// against the filesystem, returning existing regular files, highest version
// first per level (numeric runs compare as numbers: 13.2 > 9.9), at most
// maxMatches. Hidden entries and symlinked
// directories are not descended into.
// Natural "version" order: true when a sorts after b (digit runs compare
// numerically, the rest case-insensitively). Pure.
[[nodiscard]] bool versionGreater(const QString &a, const QString &b);

[[nodiscard]] QStringList expandPrefixCandidate(const QString &unixPattern,
                                                int maxMatches = 16);

// The first launcher candidate (Windows paths, in recipe order) that exists
// as a regular file inside the prefix; empty when none does.
[[nodiscard]] QString firstExistingCandidate(const QString &prefixDir,
                                             const QStringList &windowsCandidates);

} // namespace QindaQt::QindaLutris
