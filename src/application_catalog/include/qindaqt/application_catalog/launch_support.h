// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::ShellLauncher {
struct ExecPlan;
}

namespace QindaQt::ApplicationCatalog {

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
};

struct LaunchPreparation final
{
    LaunchSupport support = LaunchSupport::Unsupported;
    QString program;
    QStringList arguments;
    QString message;

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
// caller (the file manager owns a QProcess adapter).
[[nodiscard]] LaunchPreparation planApplicationLaunch(
    const QString &documentText, const QString &actionId,
    const QString &displayName, const QString &desktopFilePath);

// The single Exec argument vector for launching a terminal-required
// application through the supplied terminal emulator command prefix. Empty
// when the prefix is empty or the plan is not terminal-routed.
[[nodiscard]] QStringList terminalCommandLine(
    const QStringList &terminalCommandPrefix, const QString &program,
    const QStringList &arguments);

} // namespace QindaQt::ApplicationCatalog
