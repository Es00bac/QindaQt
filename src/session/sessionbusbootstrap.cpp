// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionbusbootstrap.h"

namespace QindaQt::Session {

SessionBusBootstrapCommand SessionBusBootstrap::command(
    const QStringList &launcherArguments,
    const QProcessEnvironment &environment,
    const QString &runnerExecutable)
{
    if (!environment.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).trimmed().isEmpty()) {
        return {};
    }
    if (runnerExecutable.trimmed().isEmpty() || launcherArguments.isEmpty()) {
        return {};
    }

    SessionBusBootstrapCommand result;
    result.executable = runnerExecutable;
    result.arguments = {QStringLiteral("--")};
    result.arguments.append(launcherArguments);
    return result;
}

} // namespace QindaQt::Session
