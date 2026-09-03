// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_discovery/font_discovery.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

// AGENT-CONTRACT: Every discovery test stage carries its own injected
// fontconfig configuration file and cachedir so the host default
// configuration and host font directories are never consulted (F1 contract,
// ADR-0057).
namespace FontDiscoveryTestSupport {

using QindaQt::Services::FontDiscovery::FontDiscoveryLimits;
using QindaQt::Services::FontDiscovery::FontDiscoveryProvider;
using QindaQt::Services::FontDiscovery::FontDiscoveryRequest;
using QindaQt::Services::FontDiscovery::FontDiscoveryResult;

struct Stage {
    QTemporaryDir root;

    [[nodiscard]] QString fontsPath() const { return root.filePath(QStringLiteral("fonts")); }
    [[nodiscard]] QString cachePath() const { return root.filePath(QStringLiteral("cache")); }
    [[nodiscard]] QString configPath() const { return root.filePath(QStringLiteral("fonts.conf")); }
};

[[nodiscard]] inline bool writeConfiguration(const QString &path, const QString &cacheDirectory)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray xml = QByteArrayLiteral("<?xml version=\"1.0\"?>\n<fontconfig><cachedir>")
                           + cacheDirectory.toUtf8() + QByteArrayLiteral("</cachedir></fontconfig>\n");
    return file.write(xml) == xml.size();
}

[[nodiscard]] inline bool copyFixtures(const QStringList &names, const QString &destinationDirectory)
{
    QDir().mkpath(destinationDirectory);
    for (const QString &name : names) {
        const QString source = QStringLiteral(QINDAQT_FONT_FIXTURES) + QLatin1Char('/') + name;
        if (!QFile::copy(source, destinationDirectory + QLatin1Char('/') + name)) {
            return false;
        }
    }
    return true;
}

// Stages a config-only stage (no fonts) with a valid injected configuration.
[[nodiscard]] inline bool stageConfiguration(Stage &stage)
{
    return writeConfiguration(stage.configPath(), stage.cachePath());
}

[[nodiscard]] inline FontDiscoveryRequest requestFor(const Stage &stage)
{
    FontDiscoveryRequest request;
    request.configurationFile = stage.configPath();
    request.fontDirectories = {stage.fontsPath()};
    return request;
}

} // namespace FontDiscoveryTestSupport
