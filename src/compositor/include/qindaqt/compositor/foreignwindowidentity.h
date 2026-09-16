// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace QindaQt::Compositor {

// AGENT-CONTRACT: raw identity repair for windows whose reported class is an
// opaque launcher artifact rather than the program the user recognises
// (ADR-0169). Pure: every input is passed in, nothing is read here, so the
// policy is unit-testable without KWin or a live process.
//
// The motivating case, measured on a live session: Battle.net launched through
// umu/Proton reports `WM_CLASS = ("steam_app_0", "steam_app_0")`, so the task
// list and dock showed "steam_app_0". Its command line names
// `C:\Program Files (x86)\Battle.net\Battle.net.exe`, and the host's own
// `battlenet.desktop` already declares `StartupWMClass=battle.net.exe` - so the
// executable basename is exactly the key that resolves the real name and icon
// through the shell's existing desktop-entry lookup.
//
// This stays RAW identity on purpose. It answers "which program is behind this
// window", never "what should the label say"; the shell keeps all naming and
// icon policy.

// True when a reported resource class carries no application identity a user
// could recognise: empty, a `steam_app_<digits>` launcher key, or one of Wine's
// own shim classes. Everything else is left strictly alone - a real class is
// always better identity than a guess.
[[nodiscard]] bool isOpaqueLauncherClass(const QString &resourceClass);

// Basename of the program behind a client, from its `/proc/<pid>/cmdline`
// bytes. A Wine or Proton client's command line names the WINDOWS executable,
// so the last `.exe` argument wins; otherwise argv[0]'s basename is used.
// Handles both `\` and `/` separators. Returns empty for absent, oversized,
// or hostile input rather than guessing.
[[nodiscard]] QString executableFromCommandLine(const QByteArray &cmdline);

// The application id to report for a window: the desktop file name when the
// client declared one, else the reported class when that carries identity,
// else the client executable, else the reported class unchanged (which may be
// empty, exactly as before).
[[nodiscard]] QString resolveApplicationId(const QString &desktopFileName,
                                           const QString &resourceClass,
                                           const QString &clientExecutable);

// The application name to report: the reported class when it carries identity,
// else the client executable, else the resolved id. Never invents text.
[[nodiscard]] QString resolveApplicationName(const QString &resourceClass,
                                             const QString &clientExecutable,
                                             const QString &applicationId);

} // namespace QindaQt::Compositor
