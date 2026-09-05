// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Shell {

// Returns low-to-high precedence directories. Explicit/env paths are isolated.
// An empty sourcePath contributes nothing.
[[nodiscard]] QStringList resolveProfileCatalogDirectories(const QString &explicitPath,
                                                           const QString &sourcePath);
// Returns the source catalog directory only for the genuine build-tree
// executable; any other caller receives an empty path.
[[nodiscard]] QString buildTreeSourceDirectory(const char *sourcePath,
                                               const char *buildExecutablePath);
[[nodiscard]] QString resolveCatalogDataDirectory(const QString &explicitPath,
                                                  const char *environmentName,
                                                  const char *sourcePath,
                                                  const QString &installedSuffix);
[[nodiscard]] QString resolveCatalogDataFile(const QString &explicitPath,
                                             const char *environmentName,
                                             const char *sourcePath,
                                             const QString &installedSuffix);

} // namespace QindaQt::Shell
