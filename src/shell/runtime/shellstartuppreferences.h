// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "shellpreferencevalues.h"

#include <QString>

#include <optional>

class QDBusConnection;

namespace QindaQt::Shell {

// AGENT-CONTRACT: The shell's startup preference read goes through the public
// Settings1 service API only. The shell never opens settings storage files;
// the service remains the sole authority over layers and validation.
//
// Reads one confirmed scoped snapshot before the initial surface plan with a
// bounded wait. An unavailable service, a lost owner, or a deadline returns
// nullopt and the caller falls back to built-in defaults; the wait never
// blocks startup longer than timeoutMilliseconds in total.
[[nodiscard]] std::optional<ShellPreferenceValues>
readConfirmedShellPreferences(const QDBusConnection &bus, int timeoutMilliseconds,
                              QString *error = nullptr);

} // namespace QindaQt::Shell
