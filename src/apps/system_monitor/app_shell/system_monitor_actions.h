// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/app_shell/action_registry.h>

namespace QindaQt::SystemMonitor {

/// The application's menus as stable command identities. One catalog serves
/// both the desktop's global menu and the in-window bar, so the two can never
/// offer different commands.
[[nodiscard]] QList<AppShell::ActionSpec> systemMonitorActions(bool singlePanel);

/// Interval actions are a radio set; this is the one whose id matches.
[[nodiscard]] QString intervalActionId(int milliseconds);
[[nodiscard]] int intervalForActionId(const QString &actionId);

} // namespace QindaQt::SystemMonitor
