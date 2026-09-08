// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"
#include "session/terminal_session_backend.h"
#include "ui/terminal_appearance.h"

#include <QByteArray>
#include <QEvent>
#include <QList>
#include <QSocketNotifier>
#include <QString>

class QTermWidget;
class QAction;
class QLineEdit;

namespace QindaQt::Apps::Terminal {

class TerminalPtyBridge;

// AGENT-CONTRACT (ADR-0040, superseding ADR-0030): This adapter's private
// implementation units are the only Terminal sources that include
// qtermwidget6 headers; only its target links qtermwidget6. The pinned
// upstream teletype contract remains:
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
  // Builds only the presentation widget. start() enters teletype mode and
  // opens the bridge after the caller has attached the widget to its final
  // visible layout, then owns child creation so failures stay typed.
  // The profile bounds scrollback, applies font family/size overrides on
  // top of the QST-derived appearance, selects the bell policy, and names
  // the theme whose projection produced the appearance.
  explicit TerminalWidgetAdapter(const TerminalViewAppearance &appearance,
                                 const TerminalProfile &profile,
                                 QObject *parent = nullptr);
  ~TerminalWidgetAdapter() override;

  [[nodiscard]] StartOutcome
  start(const TerminalLaunchRequest &request) override;
  void requestShutdown() override;
  [[nodiscard]] ProcessId shellProcessId() const override { return m_childPid; }
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
  void setAppearance(const TerminalViewAppearance &appearance) override;
  [[nodiscard]] TerminalSearchResult
  searchScrollback(const TerminalSearchQuery &query,
                   TerminalSearchDirection direction) override;
  void clearScrollbackSearch() override;
  [[nodiscard]] TerminalLinkSelection selectVisibleLink(int delta) override;
  [[nodiscard]] TerminalLinkSelection currentVisibleLink() override;

  void setZoomSteps(int steps) override {
    m_zoomSteps = steps;
    applyFont();
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void applyAppearance();
  void applyFont();
  [[nodiscard]] bool initializeChannels();
  void makeWidgetTransportByteTransparent();
  void closeChildChannel();
  void forwardChildOutput(const char *data, int length);
  void flushChildOutputToWidget();
  void primeWidgetTransport();
  void initializeSearchSurface();
  [[nodiscard]] QString captureHistory(qsizetype byteLimit, bool retainTail,
                                       bool *overflow) const;
  [[nodiscard]] QList<TerminalLink> refreshVisibleLinks();

  QTermWidget *m_widget = nullptr;
  TerminalViewAppearance m_appearance;
  const TerminalViewAppearance m_profileAppearance;
  int m_zoomSteps = 0;
  TerminalProfile m_profile;
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
  QLineEdit *m_searchEditor = nullptr;
  QAction *m_searchMatchCase = nullptr;
  QAction *m_searchRegex = nullptr;
  QAction *m_searchHighlightAll = nullptr;
  TerminalSearchQuery m_searchQuery;
  int m_searchMatchIndex = -1;
  bool m_searchRendererActive = false;
  QList<TerminalLink> m_visibleLinks;
  int m_visibleLinkIndex = -1;
};

} // namespace QindaQt::Apps::Terminal
