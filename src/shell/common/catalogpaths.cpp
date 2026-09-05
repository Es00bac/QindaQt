// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalogpaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace QindaQt::Shell {

QString buildTreeSourceDirectory(const char *sourcePath, const char *buildExecutablePath)
{
    // AGENT-CONTRACT: Source-tree catalogs are a development convenience for
    // the genuine build-tree executable only. An installed or relocated shell
    // must never let a leftover checkout shadow installed and user catalogs
    // (project audit A06); the Settings Customize route applies the same rule.
    if (sourcePath == nullptr || *sourcePath == '\0'
        || buildExecutablePath == nullptr || *buildExecutablePath == '\0') {
        return {};
    }
    const QString running =
        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString built =
        QFileInfo(QString::fromUtf8(buildExecutablePath)).canonicalFilePath();
    if (running.isEmpty() || running != built
        || !QDir(QString::fromUtf8(sourcePath)).exists()) {
        return {};
    }
    return QString::fromUtf8(sourcePath);
}

QStringList resolveProfileCatalogDirectories(const QString &explicitPath,
                                              const QString &sourcePath)
{
    if (!explicitPath.isEmpty()) return {QDir::cleanPath(explicitPath)};
    const QString environment = qEnvironmentVariable("QINDAQT_PROFILE_DIR");
    if (!environment.isEmpty()) return {QDir::cleanPath(environment)};
    QStringList result;
    if (!sourcePath.isEmpty() && QDir(sourcePath).exists()) {
        result.append(sourcePath);
    }
    const auto roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (auto it = roots.crbegin(); it != roots.crend(); ++it) {
        const QString path = QDir(*it).filePath(QStringLiteral("qindaqt/profiles"));
        if (QDir(path).exists() && !result.contains(path)) result.append(path);
    }
    return result;
}

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

QString resolveCatalogDataFile(const QString &explicitPath,
                               const char *environmentName,
                               const char *sourcePath,
                               const QString &installedSuffix)
{
    if (!explicitPath.isEmpty()) {
        return QDir::cleanPath(explicitPath);
    }
    const QString environmentPath = qEnvironmentVariable(environmentName);
    if (!environmentPath.isEmpty()) {
        return QDir::cleanPath(environmentPath);
    }
    if (QFileInfo::exists(QString::fromUtf8(sourcePath))) {
        return QString::fromUtf8(sourcePath);
    }
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  installedSuffix,
                                  QStandardPaths::LocateFile);
}

} // namespace QindaQt::Shell
