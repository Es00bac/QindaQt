// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindalutris_actions.h"

#include <QCoreApplication>

namespace QindaQt::QindaLutris {
namespace {

AppShell::ActionSpec action(const char *id, const char *menu, const char *menuLabel,
                            const char *label, const char *shortcut, int menuOrder,
                            int order) {
  AppShell::ActionSpec spec;
  spec.id = QString::fromLatin1(id);
  spec.menuId = QString::fromLatin1(menu);
  spec.menuLabel = QCoreApplication::translate("QindaLutrisActions", menuLabel);
  spec.label = QCoreApplication::translate("QindaLutrisActions", label);
  spec.shortcut = QKeySequence(QString::fromLatin1(shortcut));
  spec.menuOrder = menuOrder;
  spec.order = order;
  return spec;
}

} // namespace

QList<AppShell::ActionSpec> qindaLutrisActions() {
  // AGENT-GUARD: AppShell's registry rejects an action with no shortcut
  // ("invalid presentation state"), so every entry carries one.
  return {
      action("file.add-windows-game", "file", "File", "Add Windows game…",
             "Ctrl+N", 0, 0),
      action("file.refresh", "file", "File", "Refresh library", "F5", 0, 1),
      action("file.quit", "file", "File", "Quit", "Ctrl+Q", 0, 9),
      action("help.about", "help", "Help", "About QindaLutris", "F1", 2, 0),
  };
}

} // namespace QindaQt::QindaLutris
