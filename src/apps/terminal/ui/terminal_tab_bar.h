// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTabBar>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: TerminalTabBar is the window's presentation of the
// session list: one closable tab per session, in collection order. It owns
// no session state — the window maps tabs to TerminalSession* via tabData
// and routes every mutation through TerminalSessionCollection, so this
// strip stays a dumb, fully accessible view. Keyboard users drive tabs
// through the persistent window actions (tabNewAction, tabCloseAction,
// tabNextAction, tabPreviousAction, tabMoveLeftAction/Right) rather than
// pointer-only drag; the bar is intentionally not setMovable so tab order
// has exactly one authority (the collection).
class TerminalTabBar final : public QTabBar {
  Q_OBJECT

public:
  explicit TerminalTabBar(QWidget *parent = nullptr);
};

} // namespace QindaQt::Apps::Terminal
