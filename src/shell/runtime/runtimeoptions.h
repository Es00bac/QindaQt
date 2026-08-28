// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

class QCoreApplication;

namespace QindaQt::Shell {

struct RuntimeOptions {
    QString profileId;
    QString themeId;
    QString profileDirectory;
    QString themeDirectory;
    QString appletDirectory;
    QString appletPolicyFile;
    int presentationTokenDescriptor = -1;
    std::optional<qint64> compositorProcessId;
    std::optional<qint64> developmentEvidencePredecessorProcessId;
    bool listOnly = false;
};

struct RuntimeOptionsResult {
    std::optional<RuntimeOptions> options;
    QString error;
};

[[nodiscard]] RuntimeOptionsResult parseRuntimeOptions(QCoreApplication &application);
[[nodiscard]] RuntimeOptionsResult parseRuntimeOptions(const QStringList &arguments);

} // namespace QindaQt::Shell
