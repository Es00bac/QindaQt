// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_bell.h"

#include <QApplication>
#include <QWidget>

namespace QindaQt::Apps::Terminal {

void dispatchTerminalBell(const TerminalProfile::BellPolicy policy,
                          QWidget *terminalView,
                          const TerminalBellActions &actions) {
  if (policy != TerminalProfile::BellPolicy::Audible) {
    return;
  }

  if (actions.beep) {
    actions.beep();
  }

  QWidget *window = terminalView != nullptr ? terminalView->window() : nullptr;
  // AGENT-GUARD: Target the owning top-level Terminal window, never the
  // qtermwidget child. Hidden windows have no presented task surface on which
  // a compositor can expose urgency; do not leave a stale alert for later.
  if (window != nullptr && window->isVisible() &&
      actions.requestWindowAttention) {
    actions.requestWindowAttention(window);
  }
}

TerminalBellActions productionTerminalBellActions() {
  return {
      .beep = [] { QApplication::beep(); },
      .requestWindowAttention =
          [](QWidget *window) { QApplication::alert(window); },
  };
}

} // namespace QindaQt::Apps::Terminal
