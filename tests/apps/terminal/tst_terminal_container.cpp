// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include <QFontDatabase>

#include <QAction>
#include <QTabBar>
#include <QWidget>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalViewAppearance testAppearance() {
  return TerminalAppearanceAdapter::derive(
      QPalette(), TerminalContentScheme::Dark, false,
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

class RunningMonitor final : public ProcessMonitor {
public:
  ProcessExitInfo reap(ProcessId) override {
    return {.state = ProcessState::Running, .signaled = false, .code = 0,
            .statusKnown = true};
  }
  ProcessGroupState processGroupState(ProcessId) override {
    return ProcessGroupState::NonEmpty;
  }
  bool signalProcessGroup(ProcessId, int) override { return true; }
};

class Backend final : public TerminalSessionBackend {
public:
  StartOutcome start(const TerminalLaunchRequest &) override {
    m_view = new QWidget;
    return {.ok = true, .diagnostic = {}};
  }
  ~Backend() override { delete m_view; }
  void requestShutdown() override {}
  ProcessId shellProcessId() const override { return 9000; }
  QWidget *terminalWidget() override { return m_view; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}

private:
  QWidget *m_view = nullptr;
};

} // namespace

class TerminalContainerTest final : public QObject {
  Q_OBJECT

private slots:
  void newTerminalDispatchesASeparateProcessWithoutInternalTabs();
};

void TerminalContainerTest::
    newTerminalDispatchesASeparateProcessWithoutInternalTabs() {
  RunningMonitor monitor;
  TerminalSession::BackendFactory factory = [](const TerminalProfile &) {
    return std::make_unique<Backend>();
  };
  TerminalSessionContext context{{}, QStringLiteral("/bin/true"), {},
                                 QStringLiteral("/tmp")};
  auto sessions = std::make_unique<TerminalSessionCollection>(
      context, std::move(factory), &monitor, TeardownBounds{2, 2, 2, 1});
  TerminalProfile launchedProfile;
  QString launchedDirectory;
  int launchCount = 0;
  TerminalWindow window(
      std::move(sessions), testAppearance(), nullptr, nullptr,
      [&launchedProfile, &launchedDirectory, &launchCount](
          const TerminalProfile &profile, const QString &directory) {
        launchedProfile = profile;
        launchedDirectory = directory;
        ++launchCount;
        return QString{};
      });
  window.newSessionWithDefaultProfile();
  QCOMPARE(window.sessions()->count(), 1);

  QVERIFY(window.findChild<QTabBar *>() == nullptr);
  for (const char *obsolete : {"tabNewAction", "tabCloseAction",
                               "tabNextAction", "tabPreviousAction",
                               "tabMoveLeftAction", "tabMoveRightAction"}) {
    QVERIFY2(window.findChild<QAction *>(QLatin1String(obsolete)) == nullptr,
             obsolete);
  }

  QAction *newTerminal =
      window.findChild<QAction *>(QStringLiteral("fileNewTerminalAction"));
  QVERIFY(newTerminal != nullptr);
  QCOMPARE(newTerminal->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+T")));
  newTerminal->trigger();
  QCOMPARE(launchCount, 1);
  QCOMPARE(launchedProfile, builtinDefaultProfile());
  QCOMPARE(launchedDirectory, QStringLiteral("/tmp"));
  QCOMPARE(window.sessions()->count(), 1);
}

QTEST_MAIN(TerminalContainerTest)
#include "tst_terminal_container.moc"
