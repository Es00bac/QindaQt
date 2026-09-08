// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_action_catalog.h"

#include <QKeySequence>

namespace QindaQt::Apps::SystemMonitor {
namespace {

QindaQt::AppShell::ActionSpec action(const char *id, const char *menu,
                                     const char *menuLabel, const char *label,
                                     const char *description,
                                     const char *shortcut, int menuOrder,
                                     int order, bool destructive = false,
                                     bool checkable = false) {
  return {.id = QString::fromLatin1(id),
          .menuId = QString::fromLatin1(menu),
          .menuLabel = QString::fromLatin1(menuLabel),
          .label = QString::fromLatin1(label),
          .accessibleDescription = QString::fromLatin1(description),
          .shortcut = QKeySequence(QLatin1String(shortcut)),
          .menuOrder = menuOrder,
          .order = order,
          .enabled = true,
          .checkable = checkable,
          .checked = false,
          .destructive = destructive};
}

} // namespace

QList<QindaQt::AppShell::ActionSpec> systemMonitorActionCatalog() {
  using namespace AppShellActionIds;
  return {
      action(ViewOpen, "view", "View", "Open view in new window",
             "Open this resource view in another System Monitor window", "Ctrl+N",
             0, 0),
      action(ViewPause, "view", "View", "Pause updates",
             "Pause live resource updates", "Ctrl+Alt+P", 0, 1, false, true),
      action(ViewInterval250, "view", "View", "Update every 250 ms",
             "Set the live update interval to 250 milliseconds", "Ctrl+Alt+1", 0,
             2, false, true),
      action(ViewInterval500, "view", "View", "Update every 500 ms",
             "Set the live update interval to 500 milliseconds", "Ctrl+Alt+2", 0,
             3, false, true),
      action(ViewInterval1000, "view", "View", "Update every second",
             "Set the live update interval to one second", "Ctrl+Alt+3", 0, 4,
             false, true),
      action(ViewInterval2000, "view", "View", "Update every 2 seconds",
             "Set the live update interval to two seconds", "Ctrl+Alt+4", 0, 5,
             false, true),
      action(ViewInterval5000, "view", "View", "Update every 5 seconds",
             "Set the live update interval to five seconds", "Ctrl+Alt+5", 0, 6,
             false, true),
      action(ViewDuration30, "view", "View", "Show 30 seconds",
             "Set graph history to 30 seconds", "Ctrl+Shift+1", 0, 7, false,
             true),
      action(ViewDuration60, "view", "View", "Show 60 seconds",
             "Set graph history to 60 seconds", "Ctrl+Shift+2", 0, 8, false,
             true),
      action(ViewDuration120, "view", "View", "Show 2 minutes",
             "Set graph history to two minutes", "Ctrl+Shift+3", 0, 9, false,
             true),
      action(ViewDuration300, "view", "View", "Show 5 minutes",
             "Set graph history to five minutes", "Ctrl+Shift+4", 0, 10, false,
             true),
      action(ProcessTerminate, "process", "Process", "Terminate",
             "Request orderly process termination", "Ctrl+Shift+T", 1, 0, true),
      action(ProcessKill, "process", "Process", "Kill",
             "Immediately kill the selected process", "Ctrl+Shift+K", 1, 1, true),
      action(ProcessPause, "process", "Process", "Pause",
             "Suspend the selected process", "Ctrl+Shift+P", 1, 2),
      action(ProcessResume, "process", "Process", "Resume",
             "Resume the selected process", "Ctrl+Shift+R", 1, 3),
      action(ProcessNice, "process", "Process", "Set priority",
             "Set the selected process priority", "Ctrl+Alt+N", 1, 4)};
}

} // namespace QindaQt::Apps::SystemMonitor
