// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"

#include <QString>
#include <functional>

class QCommandLineParser;

namespace QindaQt::Apps::Terminal {

class TerminalProfileSettings;

// AGENT-CONTRACT: A new Terminal is a separate process because window
// topology belongs to QindaQt containers. The returned callback only reports
// dispatch failure; it never waits for the process or its shell to be ready.
[[nodiscard]] std::function<QString(const TerminalProfile &, const QString &)>
makeNewTerminalLauncher(const QCommandLineParser &parser);

// A settings client that has started but is still activating is not a fallback:
// wait for its baseline so a persisted default profile wins. Transport failure
// and degraded state deliberately use the built-in profile immediately.
[[nodiscard]] bool initialSessionReady(const TerminalProfileSettings &settings,
                                       bool definitiveFallback);

// Resolves the command-line profile against authoritative profile settings.
// A missing stored id leaves the valid default selected and supplies a concise
// diagnostic for main() to report.
[[nodiscard]] TerminalProfile
initialSessionProfile(const TerminalProfileSettings &settings,
                      const QString &requestedProfileId,
                      QString *unavailableProfileDiagnostic);

} // namespace QindaQt::Apps::Terminal
