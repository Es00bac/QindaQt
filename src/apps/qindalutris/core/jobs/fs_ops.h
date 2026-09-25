// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::QindaLutris {

// Small filesystem helpers shared by the jobs. Platform integration only; no
// policy lives here.

// rename(2) that never replaces an existing destination (Linux renameat2
// RENAME_NOREPLACE). AGENT-GUARD: an exists-check followed by a plain rename
// can clobber a build that appeared in between; this call is the commit-time
// authority. Returns false and sets *error (errno text) on any failure,
// including "destination exists" and kernels/filesystems without support.
// errnoOut (optional) receives errno: EEXIST means the destination exists;
// EINVAL/ENOSYS/EXDEV mean this filesystem cannot rename atomically.
[[nodiscard]] bool renameNoReplace(const QString &from, const QString &to,
                                   QString *error, int *errnoOut = nullptr);

// Deletes path and everything beneath it. Directories are first given owner
// rwx (an archive may carry 0555 folders), the walk never follows symbolic
// links (a link is removed as a link) and is bounded. Returns true only when
// nothing remains; otherwise *error says what is left.
[[nodiscard]] bool removeTreeForcibly(const QString &path, QString *error = nullptr);

// The nearest existing ancestor directory of path (path itself when it
// exists); empty when nothing exists (a relative or dangling path).
[[nodiscard]] QString nearestExistingDirectory(const QString &path);

// True when path exists as a symbolic link (the link itself, not its target).
[[nodiscard]] bool isSymlink(const QString &path);

} // namespace QindaQt::QindaLutris
