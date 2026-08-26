// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

class QCoreApplication;

namespace QindaQt::Shell {

struct RuntimeOptions {
    QString profileId;
    QString themeId;
    QString profileDirectory;
    QString themeDirectory;
    bool listOnly = false;
};

struct RuntimeOptionsResult {
    std::optional<RuntimeOptions> options;
    QString error;
};

[[nodiscard]] RuntimeOptionsResult parseRuntimeOptions(QCoreApplication &application);

} // namespace QindaQt::Shell
