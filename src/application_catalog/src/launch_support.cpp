// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/application_catalog/launch_support.h"

#include "qindaqt/shell_launcher/launch_execution.h"
#include "qindaqt/application_catalog/application_directory_scan.h"

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

LaunchPreparation planApplicationLaunch(const ScannedApplication &application,
                                        const QString &actionId,
                                        const QStringList &localFiles)
{
    if (!application.entry.managedBinding) {
        return planApplicationLaunch(application.documentText, actionId,
                                     application.entry.name, application.desktopFilePath,
                                     localFiles);
    }
    const auto &binding = *application.entry.managedBinding;
    // AGENT-GUARD: Managed rows never gain an Exec escape hatch, including a
    // stale/raw document accidentally retained by a later merge consumer.
    if (!QindaQt::ShellLauncher::isValidManagedApplicationBinding(binding)
        || QindaQt::ShellLauncher::managedApplicationEntryId(binding) != application.entry.id
        || !application.documentText.isEmpty() || !application.desktopFilePath.isEmpty()) {
        return {LaunchSupport::Unsupported, {}, {},
                QStringLiteral("managed application catalog binding is unusable")};
    }
    if (!actionId.isEmpty() || !localFiles.isEmpty()) {
        return {LaunchSupport::Unsupported, {}, {},
                QStringLiteral("managed application file or action handoff is unavailable")};
    }
    LaunchPreparation preparation;
    preparation.support = LaunchSupport::ManagedRuntime;
    preparation.managedBinding = binding;
    return preparation;
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
