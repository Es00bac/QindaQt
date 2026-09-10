// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Apps::TextEditor::StateDirectory {

// Shared confinement primitives for the editor's app-local state stores
// (paths-only restore inventory, crash-recovery journals). open() walks each
// absolute-path component with openat/O_NOFOLLOW and optionally creates
// missing directories 0700; the returned descriptor anchors later relative
// opens so a symlinked ancestor can never redirect a store outside its
// injected root. Callers close the descriptor.
[[nodiscard]] int open(const QString &path, bool create, int *errorNumber);

// Path of fileName beneath an open directory descriptor, via /proc/self/fd.
[[nodiscard]] QString descriptorFilePath(int directoryDescriptor,
                                         const char *fileName);

// True when fileName under the descriptor is a regular file or absent;
// anything else (symlink, directory, ...) refuses the operation.
[[nodiscard]] bool finalEntryIsRegularOrAbsent(int directoryDescriptor,
                                               const char *fileName);

} // namespace QindaQt::Apps::TextEditor::StateDirectory
