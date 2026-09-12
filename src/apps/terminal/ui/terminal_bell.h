// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"

#include <functional>

class QWidget;

namespace QindaQt::Apps::Terminal {

struct TerminalBellActions final {
  std::function<void()> beep;
  std::function<void(QWidget *)> requestWindowAttention;
};

// Handles one already-parsed qtermwidget bell on the GUI thread. The view is
// borrowed only for this synchronous call; actions are injected by value so
// tests never need to ring the host bell or involve a live terminal session.
void dispatchTerminalBell(TerminalProfile::BellPolicy policy,
                          QWidget *terminalView,
                          const TerminalBellActions &actions);

// Production actions use Qt's platform abstraction. QApplication::alert is
// the application-side request which the compositor exposes as window
// demands-attention state; Terminal owns no compositor or Task List channel.
[[nodiscard]] TerminalBellActions productionTerminalBellActions();

} // namespace QindaQt::Apps::Terminal
