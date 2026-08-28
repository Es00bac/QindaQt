// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "session/terminal_session_types.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include "qindaqt/themes/theme_loader.h"

#include <QAction>
#include <QLabel>
#include <QMetaObject>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include <csignal>
#include <memory>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalLaunchRequest validRequest() {
  return TerminalLaunchRequest{
      .program = QStringLiteral("/bin/qindaqt-test-shell"),
      .arguments = {},
      .workingDirectory = {},
      .environment = {QStringLiteral("PATH=/usr/bin")},
      .title = {}};
}

TerminalViewAppearance testAppearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load test theme: %s", qPrintable(theme.error));
  }
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    qFatal("Could not derive appearance: %s",
           qPrintable(appearance.diagnostic));
  }
  return *appearance.appearance;
}

// Offscreen stand-in for the qtermwidget adapter: counts routed operations
// and never owns a real child, so the reap truth is "exited" (the ECHILD
// equivalent for a non-child PID).
class StubBackend final : public TerminalSessionBackend {
public:
  QWidget view;
  int copyCalls = 0;
  int pasteCalls = 0;
  int pasteSelectionCalls = 0;
  int selectAllCalls = 0;
  int clearCalls = 0;

  StartOutcome start(const TerminalLaunchRequest &) override {
    return {.ok = true, .diagnostic = {}};
  }
  void requestShutdown() override {}
  [[nodiscard]] ProcessId shellProcessId() const override { return 7777; }
  [[nodiscard]] QWidget *terminalWidget() override { return &view; }
  void copySelectionToClipboard() override { ++copyCalls; }
  void pasteClipboardToSession() override { ++pasteCalls; }
  void pastePrimarySelectionToSession() override {
    ++pasteSelectionCalls;
  }
  void selectAllInView() override { ++selectAllCalls; }
  void clearView() override { ++clearCalls; }
  [[nodiscard]] bool hasSelectedText() const override { return true; }
  void sendTextToSession(const QString &) override {}
};

class InstantExitMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] ProcessExitInfo reap(ProcessId) override {
    return {.state = ProcessState::Exited, .signaled = false, .code = 0};
  }
  [[nodiscard]] bool signalProcessGroup(ProcessId, int) override {
    return false;
  }
};

struct WindowHarness final {
  InstantExitMonitor monitor;
  StubBackend *stub = nullptr;

  std::unique_ptr<TerminalWindow> makeWindow() {
    // The stub needs no theme, so the factory captures nothing that could
    // dangle after makeWindow returns.
    StubBackend **created = &stub;
    TerminalSession::BackendFactory factory = [created]() {
      auto backend = std::make_unique<StubBackend>();
      *created = backend.get();
      return std::unique_ptr<TerminalSessionBackend>(std::move(backend));
    };
    auto session = std::make_unique<TerminalSession>(
        std::move(factory), &monitor, TeardownBounds{30, 30, 30, 1});
    return std::make_unique<TerminalWindow>(std::move(session),
                                            testAppearance());
  }
};

} // namespace

class TerminalWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Direct signal emission through QMetaObject requires a registered
    // parameter type.
    qRegisterMetaType<TerminalExitStatus>("TerminalExitStatus");
  }
  void windowEmbedsOnlyPublishedWidgets();
  void actionsCarryStableIdentityAndShiftModifiedShortcuts();
  void noWindowShortcutUsesPlainReadlineControlSequences();
  void clipboardAndSelectionActionsRouteThroughSession();
  void exitStatusIsReportedWithSeverityDistinction();
  void accessibilityMetadataIsPresentAndFocusIsOnTerminalView();
  void hostileResizeClampsEmbeddedView();
  void closeRequestsShutdownBeforeQuitSignal();
};

void TerminalWindowTest::windowEmbedsOnlyPublishedWidgets() {
  WindowHarness harness;
  auto window = harness.makeWindow();

  // The window is constructed before the first start; presentation must not
  // guess or create a terminal view by itself.
  QVERIFY(window->session()->terminalWidget() == nullptr);
  QVERIFY(window->session()->start(validRequest()));
  QCOMPARE(window->session()->state(), TerminalSession::State::Running);
  QVERIFY(window->session()->terminalWidget() != nullptr);
  QVERIFY(window->session()->terminalWidget() == &harness.stub->view);
}

void TerminalWindowTest::
    actionsCarryStableIdentityAndShiftModifiedShortcuts() {
  WindowHarness harness;
  auto window = harness.makeWindow();

  struct Expectation {
    const char *objectName;
    const char *shortcut;
  };
  const QVector<Expectation> expectations = {
      {"sessionRestartAction", "Ctrl+Shift+R"},
      {"editCopyAction", "Ctrl+Shift+C"},
      {"editPasteAction", "Ctrl+Shift+V"},
      {"editPasteSelectionAction", "Ctrl+Shift+Insert"},
      {"editSelectAllAction", "Ctrl+Shift+A"},
      {"viewClearAction", "Ctrl+Shift+K"},
      {"fileQuitAction", "Ctrl+Shift+Q"},
  };
  for (const auto &expectation : expectations) {
    const auto action = window->findChild<QAction *>(
        QLatin1String(expectation.objectName));
    QVERIFY2(action != nullptr, expectation.objectName);
    QCOMPARE(action->shortcut().toString(QKeySequence::NativeText),
             QLatin1String(expectation.shortcut));
    QCOMPARE(action->shortcutContext(), Qt::WindowShortcut);
    QVERIFY(!action->text().isEmpty());
    QVERIFY(!action->statusTip().isEmpty());
  }
}

void TerminalWindowTest::
    noWindowShortcutUsesPlainReadlineControlSequences() {
  WindowHarness harness;
  auto window = harness.makeWindow();

  // Ctrl+C/S/Q/A/Z/X/V/R/K/W are readline and shell-job sequences inside the
  // child; a window binding on any plain form would steal them.
  const QStringList forbidden = {
      QStringLiteral("Ctrl+C"), QStringLiteral("Ctrl+S"),
      QStringLiteral("Ctrl+Q"), QStringLiteral("Ctrl+A"),
      QStringLiteral("Ctrl+Z"), QStringLiteral("Ctrl+X"),
      QStringLiteral("Ctrl+V"), QStringLiteral("Ctrl+R"),
      QStringLiteral("Ctrl+K"), QStringLiteral("Ctrl+W"),
  };
  const auto actions = window->findChildren<QAction *>();
  QVERIFY(!actions.isEmpty());
  for (const QAction *action : actions) {
    if (action->objectName().isEmpty()) {
      continue; // Menu-title actions created by QMenuBar are not commands.
    }
    const QString sequence = action->shortcut().toString();
    QVERIFY2(!forbidden.contains(sequence),
             qPrintable(QStringLiteral("%1 uses %2")
                            .arg(action->objectName(), sequence)));
  }
}

void TerminalWindowTest::
    clipboardAndSelectionActionsRouteThroughSession() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->session()->start(validRequest()));
  QVERIFY(harness.stub != nullptr);

  auto *copy =
      window->findChild<QAction *>(QStringLiteral("editCopyAction"));
  QVERIFY(copy != nullptr);
  copy->trigger();
  QCOMPARE(harness.stub->copyCalls, 1);

  auto *paste =
      window->findChild<QAction *>(QStringLiteral("editPasteAction"));
  QVERIFY(paste != nullptr);
  paste->trigger();
  QCOMPARE(harness.stub->pasteCalls, 1);

  auto *pasteSelection = window->findChild<QAction *>(
      QStringLiteral("editPasteSelectionAction"));
  QVERIFY(pasteSelection != nullptr);
  pasteSelection->trigger();
  QCOMPARE(harness.stub->pasteSelectionCalls, 1);

  auto *selectAll = window->findChild<QAction *>(
      QStringLiteral("editSelectAllAction"));
  QVERIFY(selectAll != nullptr);
  selectAll->trigger();
  QCOMPARE(harness.stub->selectAllCalls, 1);

  auto *clear =
      window->findChild<QAction *>(QStringLiteral("viewClearAction"));
  QVERIFY(clear != nullptr);
  clear->trigger();
  QCOMPARE(harness.stub->clearCalls, 1);
}

void TerminalWindowTest::exitStatusIsReportedWithSeverityDistinction() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->session()->start(validRequest()));

  auto *status =
      window->findChild<QLabel *>(QStringLiteral("qindaqtTerminalStatus"));
  QVERIFY(status != nullptr);

  // Exit events reach the window through the session signal; emitting the
  // signal directly is the moc-supported way to drive one subscriber.
  const TerminalExitStatus normal{TerminalExitStatus::Kind::Normal, 3, {}};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, normal)));
  QCOMPARE(status->text(), QStringLiteral("Session exited (code 3)"));
  QCOMPARE(status->palette().color(QPalette::WindowText),
           testAppearance().windowPalette.color(QPalette::WindowText));

  const TerminalExitStatus crash{TerminalExitStatus::Kind::Signal,
                                 int{SIGKILL}, {}};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, crash)));
  QVERIFY(status->text().contains(QLatin1String("SIGKILL")));
  QCOMPARE(status->palette().color(QPalette::WindowText),
           testAppearance().statusDangerForeground);

  const TerminalExitStatus failed{
      TerminalExitStatus::Kind::StartFailed, 0,
      QStringLiteral("channel unavailable")};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, failed)));
  QVERIFY(status->text().startsWith(QLatin1String("Error:")));
  QVERIFY(status->text().contains(QLatin1String("channel unavailable")));
}

void TerminalWindowTest::
    accessibilityMetadataIsPresentAndFocusIsOnTerminalView() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->session()->start(validRequest()));
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));

  auto *view = window->session()->terminalWidget();
  QVERIFY(view != nullptr);
  QCOMPARE(view->accessibleName(), QStringLiteral("Terminal session"));
  QVERIFY(!view->accessibleDescription().isEmpty());
  QCOMPARE(view->focusPolicy(), Qt::StrongFocus);
  QCOMPARE(window->accessibleName(), QStringLiteral("QindaQt Terminal"));
}

void TerminalWindowTest::hostileResizeClampsEmbeddedView() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->session()->start(validRequest()));
  auto *view = window->session()->terminalWidget();
  QVERIFY(view != nullptr);

  window->resize(800, 500);
  QTest::qWait(20);
  QVERIFY(view->width() >= TerminalLaunchPolicy::kMinViewWidth);
  QVERIFY(view->height() >= TerminalLaunchPolicy::kMinViewHeight);
  QVERIFY(view->width() <= TerminalLaunchPolicy::kMaxViewWidth);
  QVERIFY(view->height() <= TerminalLaunchPolicy::kMaxViewHeight);
}

void TerminalWindowTest::closeRequestsShutdownBeforeQuitSignal() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->session()->start(validRequest()));
  QSignalSpy shutdownSpy(window->session(),
                         &TerminalSession::shutdownFinished);
  QSignalSpy quitSpy(window.get(), &TerminalWindow::closeShutdownFinished);

  window->close();
  QTest::qWait(200);
  QCOMPARE(shutdownSpy.count(), 1);
  QCOMPARE(quitSpy.count(), 1);
  QCOMPARE(window->session()->state(),
           TerminalSession::State::ShutdownComplete);
}

QTEST_MAIN(TerminalWindowTest)
#include "tst_terminal_window.moc"
