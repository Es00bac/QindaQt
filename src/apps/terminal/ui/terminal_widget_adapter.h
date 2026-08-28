// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_backend.h"
#include "ui/terminal_appearance.h"

#include <QByteArray>
#include <QEvent>
#include <QSocketNotifier>
#include <QString>

class QTermWidget;

namespace QindaQt::Apps::Terminal {

class TerminalPtyBridge;

// AGENT-CONTRACT (ADR-0040, superseding ADR-0030): This is the only
// translation unit in the repository that includes qtermwidget6 headers or
// links qtermwidget6. The pinned upstream teletype contract remains:
//  - startTerminalTeletype() opens the widget's PTY and re-exposes keyboard
//    bytes on the sendData signal;
//  - getPtySlaveFd() yields the slave used as the child-output channel;
//  - the widget's master reads and emulator rendering run without a
//    widget-owned child.
// The child itself runs on this adapter's TerminalPtyBridge PTY: keyboard
// master-writes are real child input, child output/echo is pumped into the
// teletype slave, and child winsize is programmed explicitly on bridge
// resize events. Bumping the dependency requires re-verifying those
// behaviors at the new tag before this adapter may change.
class TerminalWidgetAdapter final : public TerminalSessionBackend {
  Q_OBJECT

public:
  // Builds the widget, enters teletype mode, and opens the bridge PTY.
  // Never forks here: start() owns child creation so failures stay typed.
  explicit TerminalWidgetAdapter(const TerminalViewAppearance &appearance,
                                 QObject *parent = nullptr);
  ~TerminalWidgetAdapter() override;

  [[nodiscard]] StartOutcome start(const TerminalLaunchRequest &request) override;
  void requestShutdown() override;
  [[nodiscard]] ProcessId shellProcessId() const override {
    return m_childPid;
  }
  // Defined out of line in the .cpp: QTermWidget is only forward-declared
  // here (the private dependency must stay invisible to consumers), so the
  // derived-to-base conversion needs the complete type. AGENT-GUARD: never
  // give this override an inline body touching m_widget — the AUTOMOC unit
  // and main.cpp compile without <qtermwidget.h> and would fail the
  // derived-to-base conversion again (P1 strict-compile defect).
  [[nodiscard]] QWidget *terminalWidget() override;

  void copySelectionToClipboard() override;
  void pasteClipboardToSession() override;
  void pastePrimarySelectionToSession() override;
  void selectAllInView() override;
  void clearView() override;
  [[nodiscard]] bool hasSelectedText() const override;
  void sendTextToSession(const QString &text) override;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void applyAppearance();
  void makeWidgetTransportByteTransparent();
  void closeChildChannel();
  void forwardChildOutput(const char *data, int length);
  void flushChildOutputToWidget();

  QTermWidget *m_widget = nullptr;
  TerminalViewAppearance m_appearance;
  QString m_schemePath;
  TerminalPtyBridge *m_bridge = nullptr;
  QString m_slavePath;
  QString m_bridgeDiagnostic;
  int m_widgetSlaveFd = -1;
  QByteArray m_widgetOutputBuffer;
  QSocketNotifier *m_widgetOutputNotifier = nullptr;
  // Empty when the widget transport is proven byte-transparent; otherwise a
  // typed diagnostic that makes start() refuse (fail-closed, P2: the second
  // PTY must never apply a second line-discipline transformation).
  QString m_transportDiagnostic;
  ProcessId m_childPid = 0;
  bool m_shutdownRequested = false;
};

} // namespace QindaQt::Apps::Terminal
