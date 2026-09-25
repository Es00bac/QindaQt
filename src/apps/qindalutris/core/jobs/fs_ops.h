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
[[nodiscard]] bool renameNoReplace(const QString &from, const QString &to,
                                   QString *error);

// The nearest existing ancestor directory of path (path itself when it
// exists); empty when nothing exists (a relative or dangling path).
[[nodiscard]] QString nearestExistingDirectory(const QString &path);

// True when path exists as a symbolic link (the link itself, not its target).
[[nodiscard]] bool isSymlink(const QString &path);

} // namespace QindaQt::QindaLutris
