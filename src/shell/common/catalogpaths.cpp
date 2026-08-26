// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalogpaths.h"

#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Shell {

QString resolveCatalogDataDirectory(const QString &explicitPath, const char *environmentName,
                                    const char *sourcePath, const QString &installedSuffix)
{
    if (!explicitPath.isEmpty()) {
        return QDir::cleanPath(explicitPath);
    }
    const QString environmentPath = qEnvironmentVariable(environmentName);
    if (!environmentPath.isEmpty()) {
        return QDir::cleanPath(environmentPath);
    }
    if (QDir(QString::fromUtf8(sourcePath)).exists()) {
        return QString::fromUtf8(sourcePath);
    }
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  installedSuffix,
                                  QStandardPaths::LocateDirectory);
}

} // namespace QindaQt::Shell
