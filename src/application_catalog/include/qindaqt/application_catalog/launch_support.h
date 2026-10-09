// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/managed_applications.h"

#include <optional>

#include <QString>
#include <QStringList>

namespace QindaQt::ShellLauncher {
struct ExecPlan;
}

namespace QindaQt::ApplicationCatalog {

struct ScannedApplication;

// How an application's validated document can be started. ProcessSpawn means
// the caller may execute the planned argv directly; DbusActivatable requires
// a D-Bus activation the caller must provide; TerminalRequired means the
// planned argv belongs inside a terminal emulator command line; Unsupported
// carries the reason in the message.
enum class LaunchSupport {
    ProcessSpawn,
    DbusActivatable,
    TerminalRequired,
    Unsupported,
    ManagedRuntime,
};

struct LaunchPreparation final
{
    LaunchSupport support = LaunchSupport::Unsupported;
    QString program;
    QStringList arguments;
    QString message;
    // ADR-0269: how many of the planned localFiles the argv carries (see
    // ExecPlan::fileArguments): 0, 1 (%f/%u) or all of them (%F/%U).
    qsizetype fileArguments = 0;
    // ManagedRuntime supplies only this binding. Program/argv remain empty;
    // the runtime owner rechecks revision/digest and resolves its own recipe.
    std::optional<QindaQt::ShellLauncher::ManagedApplicationBinding> managedBinding = std::nullopt;

    [[nodiscard]] bool spawnable() const noexcept
    {
        return support == LaunchSupport::ProcessSpawn
            || support == LaunchSupport::TerminalRequired;
    }
    friend bool operator==(const LaunchPreparation &,
                           const LaunchPreparation &) = default;
};

// Extracts the execution keys of one entry/action from the exact document
// text a scan retained and plans the argv without any shell interpolation.
// Pure text work over caller-supplied input; process spawning stays with the
// caller (the file manager owns a QProcess adapter). localFiles (ADR-0269,
// File Manager's Open With) are already-validated absolute paths that the
// Exec file codes receive as whole arguments; leave it empty to plan a plain
// application start.
[[nodiscard]] LaunchPreparation planApplicationLaunch(
    const QString &documentText, const QString &actionId,
    const QString &displayName, const QString &desktopFilePath,
    const QStringList &localFiles = {});

// Plans a complete catalog row without confusing managed routing with native
// Exec execution. ManagedRuntime is not spawnable; until a qualified file/
// action handoff exists, managed localFiles/actions are truthfully refused.
// This function is pure and grants no runtime/process/window authority.
[[nodiscard]] LaunchPreparation planApplicationLaunch(
    const ScannedApplication &application, const QString &actionId = {},
    const QStringList &localFiles = {});

// The single Exec argument vector for launching a terminal-required
// application through the supplied terminal emulator command prefix. Empty
// when the prefix is empty or the plan is not terminal-routed.
[[nodiscard]] QStringList terminalCommandLine(
    const QStringList &terminalCommandPrefix, const QString &program,
    const QStringList &arguments);

} // namespace QindaQt::ApplicationCatalog
