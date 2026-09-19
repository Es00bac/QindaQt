// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_actions.h"

#include <QCoreApplication>

namespace QindaQt::SystemMonitor {

namespace {

AppShell::ActionSpec action(const char *id, const char *menu, const char *menuLabel,
                            const char *label, const char *shortcut, int menuOrder,
                            int order) {
  AppShell::ActionSpec spec;
  spec.id = QString::fromLatin1(id);
  spec.menuId = QString::fromLatin1(menu);
  spec.menuLabel = QCoreApplication::translate("SystemMonitorActions", menuLabel);
  spec.label = QCoreApplication::translate("SystemMonitorActions", label);
  spec.shortcut = QKeySequence(QString::fromLatin1(shortcut));
  spec.menuOrder = menuOrder;
  spec.order = order;
  return spec;
}

AppShell::ActionSpec checkable(AppShell::ActionSpec spec, bool checked) {
  spec.checkable = true;
  spec.checked = checked;
  return spec;
}

struct IntervalChoice final {
  const char *id;
  int milliseconds;
};

// AGENT-CONTRACT: the sampling rates offered, and the only place they are
// listed. QML reads them back through intervalForActionId so a rate cannot be
// added to the menu without the application understanding it.
constexpr IntervalChoice kIntervals[] = {
    {"view.interval-fast", 500},
    {"view.interval-normal", 1000},
    {"view.interval-relaxed", 3000},
};

} // namespace

QString intervalActionId(int milliseconds) {
  for (const IntervalChoice &choice : kIntervals) {
    if (choice.milliseconds == milliseconds) {
      return QString::fromLatin1(choice.id);
    }
  }
  return {};
}

int intervalForActionId(const QString &actionId) {
  for (const IntervalChoice &choice : kIntervals) {
    if (actionId == QLatin1String(choice.id)) {
      return choice.milliseconds;
    }
  }
  return 0;
}

QList<AppShell::ActionSpec> systemMonitorActions(bool singlePanel) {
  QList<AppShell::ActionSpec> actions{
      action("view.pause", "view", "View", "Pause updates", "Ctrl+Alt+P", 0, 0),
      action("view.refresh", "view", "View", "Refresh now", "F5", 0, 1),
      // AGENT-GUARD: AppShell's registry rejects an action with no shortcut
      // ("invalid presentation state"), so every entry here carries one --
      // including the ones a menu would ordinarily leave bare.
      action("view.interval-fast", "view", "View", "Fast (500 ms)", "Ctrl+1", 0, 2),
      action("view.interval-normal", "view", "View", "Normal (1 s)", "Ctrl+2", 0, 3),
      action("view.interval-relaxed", "view", "View", "Relaxed (3 s)", "Ctrl+3", 0, 4),
      action("help.about", "help", "Help", "About System Monitor", "F1", 2, 0),
  };
  actions[0] = checkable(actions.at(0), false);
  for (int i = 2; i <= 4; ++i) {
    // Normal is the engine's own default, so it starts checked.
    actions[i] = checkable(actions.at(i), actions.at(i).id
                                              == QLatin1String("view.interval-normal"));
  }

  // A window showing one panel has no arrangement to reset, and offering the
  // commands anyway would be offering commands that do nothing.
  if (!singlePanel) {
    actions.append(action("layout.reset", "layout", "Layout", "Reset arrangement",
                          "Ctrl+Shift+R", 1, 0));
    actions.append(action("layout.show-all", "layout", "Layout", "Show every panel",
                          "Ctrl+Shift+A", 1, 1));
  }
  return actions;
}

} // namespace QindaQt::SystemMonitor
