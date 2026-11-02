// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

namespace QindaQt::Link {

// Creates `path` (and missing parents) so that the leaf directory is created
// with mode 0700 from the start (no umask window) and an existing leaf is
// tightened to 0700. Returns false on failure.
[[nodiscard]] bool ensurePrivateDirectory(const QString &path);

} // namespace QindaQt::Link
