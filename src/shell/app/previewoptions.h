// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

class QCoreApplication;

namespace QindaQt::Shell {

struct PreviewOptions {
    QString profileId;
    QString themeId;
    QString profileDirectory;
    QString themeDirectory;
    QString screenshotPath;
    int width = 1280;
    int height = 720;
    bool listOnly = false;

    [[nodiscard]] bool capturesScreenshot() const;
};

struct PreviewOptionsResult {
    std::optional<PreviewOptions> options;
    QString error;
};

[[nodiscard]] PreviewOptionsResult parsePreviewOptions(QCoreApplication &application);

} // namespace QindaQt::Shell
