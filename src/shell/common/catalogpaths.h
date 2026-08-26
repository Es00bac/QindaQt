// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Shell {

[[nodiscard]] QString resolveCatalogDataDirectory(const QString &explicitPath,
                                                  const char *environmentName,
                                                  const char *sourcePath,
                                                  const QString &installedSuffix);
[[nodiscard]] QString resolveCatalogDataFile(const QString &explicitPath,
                                             const char *environmentName,
                                             const char *sourcePath,
                                             const QString &installedSuffix);

} // namespace QindaQt::Shell
