// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/application_catalog/launch_support.h"

#include "qindaqt/shell_launcher/launch_execution.h"

namespace QindaQt::ApplicationCatalog {

using QindaQt::Shell::Launcher::ExecExpansionValues;
using QindaQt::Shell::Launcher::LaunchExecutionParser;

LaunchPreparation planApplicationLaunch(const QString &documentText,
                                        const QString &actionId,
                                        const QString &displayName,
                                        const QString &desktopFilePath,
                                        const QStringList &localFiles)
{
    const auto keys = LaunchExecutionParser::parse(documentText, actionId);
    if (!keys.ok()) {
        return {LaunchSupport::Unsupported, {}, {}, keys.message};
    }
    if (keys.keys->dbusActivatable) {
        return {LaunchSupport::DbusActivatable, {}, {},
                QStringLiteral("application requests D-Bus activation")};
    }
    const QindaQt::Shell::Launcher::ExecExpansionValues values{
        displayName, {}, desktopFilePath, localFiles};
    const auto plan = QindaQt::Shell::Launcher::ExecFieldCodeExpander::expand(
        keys.keys->exec, values);
    if (!plan.ok()) {
        return {LaunchSupport::Unsupported, {}, {}, plan.message};
    }
    if (keys.keys->terminal) {
        return {LaunchSupport::TerminalRequired, plan.plan->program,
                plan.plan->arguments, {}, plan.plan->fileArguments};
    }
    return {LaunchSupport::ProcessSpawn, plan.plan->program,
            plan.plan->arguments, {}, plan.plan->fileArguments};
}

QStringList terminalCommandLine(const QStringList &terminalCommandPrefix,
                                const QString &program,
                                const QStringList &arguments)
{
    if (terminalCommandPrefix.isEmpty() || program.isEmpty()) {
        return {};
    }
    QStringList command = terminalCommandPrefix;
    // The terminal runs the planned argv as its command; "-e" style flags
    // (the prefix's last token) precede the program, matching the launcher's
    // own terminal routing.
    command.append(program);
    command.append(arguments);
    return command;
}

} // namespace QindaQt::ApplicationCatalog
