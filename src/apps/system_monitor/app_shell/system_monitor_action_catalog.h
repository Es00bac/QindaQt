// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

namespace QindaQt::Apps::SystemMonitor {
namespace AppShellActionIds {
inline constexpr const char *ViewOpen = "view.open-window";
inline constexpr const char *ViewPause = "view.pause";
inline constexpr const char *ViewInterval250 = "view.interval-250";
inline constexpr const char *ViewInterval500 = "view.interval-500";
inline constexpr const char *ViewInterval1000 = "view.interval-1000";
inline constexpr const char *ViewInterval2000 = "view.interval-2000";
inline constexpr const char *ViewInterval5000 = "view.interval-5000";
inline constexpr const char *ViewDuration30 = "view.duration-30";
inline constexpr const char *ViewDuration60 = "view.duration-60";
inline constexpr const char *ViewDuration120 = "view.duration-120";
inline constexpr const char *ViewDuration300 = "view.duration-300";
inline constexpr const char *ProcessTerminate = "process.terminate";
inline constexpr const char *ProcessKill = "process.kill";
inline constexpr const char *ProcessPause = "process.pause";
inline constexpr const char *ProcessResume = "process.resume";
inline constexpr const char *ProcessNice = "process.nice";
} // namespace AppShellActionIds

[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> systemMonitorActionCatalog();
} // namespace QindaQt::Apps::SystemMonitor
