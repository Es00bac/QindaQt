// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_tab_bar.h"

namespace QindaQt::Apps::Terminal {

TerminalTabBar::TerminalTabBar(QWidget *parent) : QTabBar(parent) {
  setObjectName(QStringLiteral("terminalTabBar"));
  setAccessibleName(QStringLiteral("Terminal tabs"));
  setAccessibleDescription(
      QStringLiteral("Terminal sessions in this window; use the Session "
                     "menu or Ctrl+Shift+T for a new tab"));
  setTabsClosable(true);
  setExpanding(false);
  setUsesScrollButtons(true);
  setFocusPolicy(Qt::TabFocus);
}

} // namespace QindaQt::Apps::Terminal
