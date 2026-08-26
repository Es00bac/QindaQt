// SPDX-License-Identifier: GPL-3.0-or-later
#include "installpaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace QindaQt::Session {

QString InstallPaths::pluginRoot()
{
    return pluginRootForExecutableDirectory(QCoreApplication::applicationDirPath());
}

QString InstallPaths::pluginRootForExecutableDirectory(const QString &executableDirectory)
{
    const QString pluginDirectory = QDir::cleanPath(QStringLiteral(QINDAQT_INSTALL_PLUGIN_DIR));
    if (QDir::isAbsolutePath(pluginDirectory)) {
        return pluginDirectory;
    }

    const QString binaryDirectory = QDir::cleanPath(QStringLiteral(QINDAQT_INSTALL_BIN_DIR));
    if (QDir::isAbsolutePath(binaryDirectory)) {
        return QDir::cleanPath(QStringLiteral(QINDAQT_INSTALL_FULL_PLUGIN_DIR));
    }

    QString prefixPath = QDir::cleanPath(executableDirectory);
    const auto binaryComponents = binaryDirectory.split(u'/', Qt::SkipEmptyParts);
    for (const auto &component : binaryComponents) {
        if (component == QStringLiteral(".")) {
            continue;
        }
        const QString parentPath = QFileInfo(prefixPath).path();
        if (component == QStringLiteral("..") || parentPath == prefixPath) {
            // AGENT-GUARD: Never let an unusual packaging path escape an
            // inferred prefix. KDEInstallDirs' absolute result is the safe,
            // configuration-time fallback.
            return QDir::cleanPath(QStringLiteral(QINDAQT_INSTALL_FULL_PLUGIN_DIR));
        }
        // AGENT-NOTE: QFileInfo performs lexical parent resolution even for a staging
        // prefix that has not been created yet; QDir::cdUp() requires it to
        // exist and would incorrectly select the absolute fallback.
        prefixPath = parentPath;
    }

    // AGENT-CONTRACT: Both install destinations come from KDEInstallDirs6.
    // Resolving relative to the executable keeps `cmake --install --prefix`
    // staging and relocatable prefixes aligned with the plugin artifact.
    return QDir::cleanPath(QDir(prefixPath).absoluteFilePath(pluginDirectory));
}

} // namespace QindaQt::Session
