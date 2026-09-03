// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_session_backend.h"

#if __has_include(<qtermwidget.h>)
#error "qtermwidget include surface leaked through terminal support"
#endif

#include <QtTest>

using namespace QindaQt::Apps::Terminal;

class BoundaryBackend final : public TerminalSessionBackend {
public:
  StartOutcome start(const TerminalLaunchRequest &) override { return {}; }
  void requestShutdown() override {}
  ProcessId shellProcessId() const override { return 0; }
  QWidget *terminalWidget() override { return nullptr; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}
};

class TerminalAdapterBoundaryTest final : public QObject {
  Q_OBJECT

private slots:
  void additiveDefaultsFailClosedWithoutRendererTypes() {
    BoundaryBackend backend;
    const auto search = backend.searchScrollback(
        {.pattern = QStringLiteral("text")}, TerminalSearchDirection::Initial);
    QVERIFY(!search.accepted);
    QVERIFY(!backend.currentVisibleLink().found);
  }
};

QTEST_MAIN(TerminalAdapterBoundaryTest)
#include "tst_terminal_adapter_boundary.moc"
