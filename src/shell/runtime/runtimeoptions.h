// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

class QCoreApplication;

namespace QindaQt::Shell {

struct RuntimeOptions {
    // Empty profileId/themeId mean "not explicit on the command line"; the
    // runtime then composes confirmed Settings1 preferences before built-in
    // defaults. The parser itself must not inject a default profile id.
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
