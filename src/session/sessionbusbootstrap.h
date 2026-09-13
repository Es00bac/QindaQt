// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace QindaQt::Session {

struct SessionBusBootstrapCommand final
{
    QString executable;
    QStringList arguments;

    [[nodiscard]] bool required() const noexcept { return !executable.isEmpty(); }
};

class SessionBusBootstrap final
{
public:
    [[nodiscard]] static SessionBusBootstrapCommand command(
        const QStringList &launcherArguments,
        const QProcessEnvironment &environment,
        const QString &runnerExecutable);
};

} // namespace QindaQt::Session
