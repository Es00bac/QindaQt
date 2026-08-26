// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Session {

// AGENT-CONTRACT: Resolves CMake's KDE install layout for the running launcher.
// The class has no retained state; returned paths are clean absolute paths or
// the configured absolute fallback when a relative prefix cannot be reconstructed.
class InstallPaths final
{
public:
    [[nodiscard]] static QString pluginRoot();

    // AGENT-NOTE: Kept public for deterministic layout tests. Consumers normally call
    // pluginRoot(), which supplies QCoreApplication::applicationDirPath().
    [[nodiscard]] static QString pluginRootForExecutableDirectory(
        const QString &executableDirectory);
};

} // namespace QindaQt::Session
