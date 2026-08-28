// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "session/terminal_session_backend.h"
#include "ui/terminal_appearance.h"

#include <QSocketNotifier>
#include <QString>

#include <memory>

class QTermWidget;

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT (ADR-0028): This is the only translation unit in the
// repository that includes qtermwidget6 headers or links qtermwidget6. The
// integration contract pinned by ADR-0028 at upstream tag 2.4.x is:
//  - startTerminalTeletype() opens the empty PTY and re-exposes keyboard
//    bytes on the sendData signal (master never sees emulator writes);
//  - getPtySlaveFd() yields the slave this class's child adopts;
//  - master reads and resize (TIOCSWINSZ) work without a widget-owned child.
// Bumping the dependency requires re-verifying those behaviors at the new tag
// before this adapter may change.
class TerminalWidgetAdapter final : public TerminalSessionBackend {
  Q_OBJECT

public:
  // Builds the widget, enters teletype mode, and prepares the slave channel.
  // Never forks here: start() owns child creation so failures stay typed.
  explicit TerminalWidgetAdapter(const TerminalViewAppearance &appearance,
                                 QObject *parent = nullptr);
  ~TerminalWidgetAdapter() override;

  [[nodiscard]] StartOutcome start(const TerminalLaunchRequest &request) override;
  void requestShutdown() override;
  [[nodiscard]] ProcessId shellProcessId() const override {
    return m_childPid;
  }
  [[nodiscard]] QWidget *terminalWidget() override { return m_widget; }

  void copySelectionToClipboard() override;
  void pasteClipboardToSession() override;
  void pastePrimarySelectionToSession() override;
  void selectAllInView() override;
  void clearView() override;
  [[nodiscard]] bool hasSelectedText() const override;
  void sendTextToSession(const QString &text) override;

private:
  void applyAppearance();
  void closeChildChannel();
  void forwardKeyboardBytes(const char *data, int length);
  void flushKeyboardBuffer();

  QTermWidget *m_widget = nullptr;
  TerminalViewAppearance m_appearance;
  QString m_schemePath;
  int m_slaveFd = -1;
  ProcessId m_childPid = 0;
  bool m_shutdownRequested = false;
  QByteArray m_keyboardBuffer;
  QSocketNotifier *m_slaveWriteNotifier = nullptr;
};

} // namespace QindaQt::Apps::Terminal
