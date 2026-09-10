// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "session/terminal_session_types.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include <csignal>
#include <memory>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalViewAppearance testAppearance() {
  return TerminalAppearanceAdapter::derive(
      QPalette(), TerminalContentScheme::Dark, false,
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
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
  TerminalViewAppearance appearance;
  void setAppearance(const TerminalViewAppearance &value) override { appearance = value; }

  StartOutcome start(const TerminalLaunchRequest &) override {
    return {.ok = true, .diagnostic = {}};
  }
  void requestShutdown() override {}
  [[nodiscard]] ProcessId shellProcessId() const override { return 7777; }
  [[nodiscard]] QWidget *terminalWidget() override { return &view; }
  void copySelectionToClipboard() override { ++copyCalls; }
  void pasteClipboardToSession() override { ++pasteCalls; }
  void pastePrimarySelectionToSession() override { ++pasteSelectionCalls; }
  void selectAllInView() override { ++selectAllCalls; }
  void clearView() override { ++clearCalls; }
  [[nodiscard]] bool hasSelectedText() const override { return true; }
  void sendTextToSession(const QString &) override {}
};

class InstantExitMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] ProcessExitInfo reap(ProcessId) override {
    return {.state = ProcessState::Exited,
            .signaled = false,
            .code = 0,
            .statusKnown = true};
  }
  [[nodiscard]] ProcessGroupState processGroupState(ProcessId) override {
    return ProcessGroupState::Empty;
  }
  [[nodiscard]] bool signalProcessGroup(ProcessId, int) override {
    return false;
  }
};

class NeverExitMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] ProcessExitInfo reap(ProcessId) override {
    return {.state = ProcessState::Running,
            .signaled = false,
            .code = 0,
            .statusKnown = true};
  }
  [[nodiscard]] ProcessGroupState processGroupState(ProcessId) override {
    return ProcessGroupState::NonEmpty;
  }
  [[nodiscard]] bool signalProcessGroup(ProcessId, int) override {
    return true;
  }
};

struct WindowHarness final {
  InstantExitMonitor instantExit;
  NeverExitMonitor neverExit;
  bool survivorScenario = false;
  StubBackend *stub = nullptr;
  int createdBackends = 0;

  std::unique_ptr<TerminalWindow> makeWindow(bool startSession = true) {
    // The survivor scenario exercises the failed-escalation ownership path
    // (P1-2) with tiny injected bounds; the default proves clean close.
    ProcessMonitor *monitor = survivorScenario
                                  ? static_cast<ProcessMonitor *>(&neverExit)
                                  : static_cast<ProcessMonitor *>(&instantExit);
    StubBackend **created = &stub;
    int *backends = &createdBackends;
    TerminalSession::BackendFactory factory =
        [created, backends](const TerminalProfile &) {
          auto backend = std::make_unique<StubBackend>();
          *created = backend.get();
          ++*backends;
          return std::unique_ptr<TerminalSessionBackend>(std::move(backend));
        };
    TerminalSessionContext context{
        .baseEnvironment = {QStringLiteral("PATH=/usr/bin"),
                            QStringLiteral("LANG=C.UTF-8")},
        .fallbackProgram = QStringLiteral("/bin/true"),
        .fallbackArguments = {},
        .workingDirectory = {}};
    auto collection = std::make_unique<TerminalSessionCollection>(
        std::move(context), std::move(factory), monitor,
        TeardownBounds{30, 30, 30, 1});
    auto window = std::make_unique<TerminalWindow>(
        std::move(collection), testAppearance(), nullptr);
    if (startSession) {
      window->newSessionWithDefaultProfile();
    }
    return window;
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
  void startupAndRestartUseRetainedAppearance();
  void windowChromeFollowsThePlatformTheme();
  void monospaceContentFontSurvivesLiveFontChange();
  void actionsCarryStableIdentityAndShiftModifiedShortcuts();
  void noWindowShortcutUsesPlainReadlineControlSequences();
  void clipboardAndSelectionActionsRouteThroughSession();
  void exitStatusIsReportedWithSeverityDistinction();
  void accessibilityMetadataIsPresentAndFocusIsOnTerminalView();
  void hostileResizeClampsEmbeddedView();
  void closeRequestsShutdownBeforeQuitSignal();
  void viewActionStatesTrackSessionTruth();
  void quitIsRefusedWhileSurvivorRemains();
  void restartThenCloseSpawnsNothingBeforeQuit();
  void exitedStateDisablesPasteAndKeepsScrollbackOps();
};

void TerminalWindowTest::startupAndRestartUseRetainedAppearance() {
  WindowHarness harness;
  auto window = harness.makeWindow(false);
  auto appearance = testAppearance();
  appearance.terminalBackground = QColor("#291825");
  appearance.terminalFont.setPointSizeF(17);
  window->applyAppearance(appearance);
  window->newSessionWithDefaultProfile();
  QCOMPARE(harness.stub->appearance.terminalBackground, appearance.terminalBackground);
  QCOMPARE(harness.stub->appearance.terminalFont.pointSizeF(), 17.0);
  QVERIFY(window->session()->restart());
  QTRY_COMPARE(harness.createdBackends, 2);
  QCOMPARE(harness.stub->appearance.terminalBackground, appearance.terminalBackground);
  QCOMPARE(harness.stub->appearance.terminalFont.pointSizeF(), 17.0);
}

void TerminalWindowTest::windowEmbedsOnlyPublishedWidgets() {
  WindowHarness harness;
  auto window = harness.makeWindow(false);

  // The window is constructed before the first start; presentation must not
  // guess or create a terminal view by itself.
  QVERIFY(window->session() == nullptr);
  window->newSessionWithDefaultProfile();
  QVERIFY(window->session() != nullptr);
  QCOMPARE(window->session()->state(), TerminalSession::State::Running);
  QVERIFY(window->session()->terminalWidget() != nullptr);
  QVERIFY(window->session()->terminalWidget() == &harness.stub->view);
}

void TerminalWindowTest::actionsCarryStableIdentityAndShiftModifiedShortcuts() {
  WindowHarness harness;
  auto window = harness.makeWindow();

  struct Expectation {
    const char *objectName;
    const char *shortcut;
  };
  const QVector<Expectation> expectations = {
      {"fileNewTerminalAction", "Ctrl+Shift+T"},
      {"profileManageAction", "Ctrl+Shift+P"},
      {"sessionRestartAction", "Ctrl+Shift+R"},
      {"editCopyAction", "Ctrl+Shift+C"},
      {"editPasteAction", "Ctrl+Shift+V"},
      {"editPasteSelectionAction", "Ctrl+Shift+Insert"},
      {"editSelectAllAction", "Ctrl+Shift+A"},
      {"viewClearAction", "Ctrl+Shift+K"},
      {"viewZoomInAction", "Ctrl+Shift++"},
      {"viewZoomOutAction", "Ctrl+Shift+-"},
      {"viewZoomResetAction", "Ctrl+Shift+0"},
      {"fileQuitAction", "Ctrl+Shift+Q"},
  };
  for (const auto &expectation : expectations) {
    const auto action =
        window->findChild<QAction *>(QLatin1String(expectation.objectName));
    QVERIFY2(action != nullptr, expectation.objectName);
    // Sequence equality avoids platform string abbreviations
    // ("Ctrl+Shift+Ins") while still asserting the exact binding.
    QCOMPARE(action->shortcut(),
             QKeySequence(QLatin1String(expectation.shortcut)));
    QCOMPARE(action->shortcutContext(), Qt::WindowShortcut);
    QVERIFY(!action->text().isEmpty());
    QVERIFY(!action->statusTip().isEmpty());
  }
}

void TerminalWindowTest::noWindowShortcutUsesPlainReadlineControlSequences() {
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
    QVERIFY2(
        !forbidden.contains(sequence),
        qPrintable(
            QStringLiteral("%1 uses %2").arg(action->objectName(), sequence)));
  }
}

void TerminalWindowTest::clipboardAndSelectionActionsRouteThroughSession() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(harness.stub != nullptr);

  // Copy is enabled only by an actual selection event, mirroring the real
  // adapter's copyAvailable forwarding.
  QVERIFY(!window->findChild<QAction *>(QStringLiteral("editCopyAction"))
               ->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(window->session(), "selectionAvailable",
                                    Q_ARG(bool, true)));
  QVERIFY(window->findChild<QAction *>(QStringLiteral("editCopyAction"))
              ->isEnabled());

  auto *copy = window->findChild<QAction *>(QStringLiteral("editCopyAction"));
  QVERIFY(copy != nullptr);
  copy->trigger();
  QCOMPARE(harness.stub->copyCalls, 1);

  auto *paste = window->findChild<QAction *>(QStringLiteral("editPasteAction"));
  QVERIFY(paste != nullptr);
  paste->trigger();
  QCOMPARE(harness.stub->pasteCalls, 1);

  auto *pasteSelection =
      window->findChild<QAction *>(QStringLiteral("editPasteSelectionAction"));
  QVERIFY(pasteSelection != nullptr);
  pasteSelection->trigger();
  QCOMPARE(harness.stub->pasteSelectionCalls, 1);

  auto *selectAll =
      window->findChild<QAction *>(QStringLiteral("editSelectAllAction"));
  QVERIFY(selectAll != nullptr);
  selectAll->trigger();
  QCOMPARE(harness.stub->selectAllCalls, 1);

  auto *clear = window->findChild<QAction *>(QStringLiteral("viewClearAction"));
  QVERIFY(clear != nullptr);
  clear->trigger();
  QCOMPARE(harness.stub->clearCalls, 1);
}

void TerminalWindowTest::exitStatusIsReportedWithSeverityDistinction() {
  WindowHarness harness;
  auto window = harness.makeWindow();

  auto *status =
      window->findChild<QLabel *>(QStringLiteral("qindaqtTerminalStatus"));
  QVERIFY(status != nullptr);
  // ADR-0116: severity is text plus the accessible announcement, never a
  // recolored palette. The status label inherits the window palette in every
  // state below.
  const auto inheritedInk = [&window] {
    return window->palette().color(QPalette::WindowText);
  };

  // Exit events reach the window through the session signal; emitting the
  // signal directly is the moc-supported way to drive one subscriber.
  const TerminalExitStatus normal{TerminalExitStatus::Kind::Normal, 3, {}};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, normal)));
  QCOMPARE(status->text(), QStringLiteral("Session exited (code 3)"));
  QCOMPARE(status->palette().color(QPalette::WindowText), inheritedInk());

  const TerminalExitStatus crash{
      TerminalExitStatus::Kind::Signal, int{SIGKILL}, {}};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, crash)));
  QVERIFY(status->text().contains(QLatin1String("SIGKILL")));
  QVERIFY(status->accessibleName().contains(QLatin1String("SIGKILL")));
  QCOMPARE(status->palette().color(QPalette::WindowText), inheritedInk());

  const TerminalExitStatus failed{TerminalExitStatus::Kind::StartFailed, 0,
                                  QStringLiteral("channel unavailable")};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, failed)));
  QVERIFY(status->text().startsWith(QLatin1String("Error:")));
  QVERIFY(status->text().contains(QLatin1String("channel unavailable")));
  QCOMPARE(status->palette().color(QPalette::WindowText), inheritedInk());

  // P2-5: an unknown exit is surfaced as its own truth, never as success.
  const TerminalExitStatus unknown{
      TerminalExitStatus::Kind::UnknownExit, 0, {}};
  QVERIFY(QMetaObject::invokeMethod(window->session(), "sessionFinished",
                                    Q_ARG(TerminalExitStatus, unknown)));
  QCOMPARE(status->text(), QStringLiteral("Session exited (status unknown)"));
  QCOMPARE(status->palette().color(QPalette::WindowText), inheritedInk());
}

void TerminalWindowTest::windowChromeFollowsThePlatformTheme() {
  // ADR-0116: the window installs no palette, font, or QSS of its own, and a
  // live platform palette change reaches chrome without any app projection.
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(window->styleSheet().isEmpty());
  QVERIFY(window->testAttribute(Qt::WA_SetPalette) == false);
  QCOMPARE(window->palette(), QGuiApplication::palette());
  auto *status =
      window->findChild<QLabel *>(QStringLiteral("qindaqtTerminalStatus"));
  QVERIFY(status != nullptr);
  QVERIFY(status->styleSheet().isEmpty());

  const QPalette previous = QGuiApplication::palette();
  QPalette replacement = previous;
  const QColor marker = previous.color(QPalette::Window).lightnessF() > 0.5
                            ? QColor("#223344")
                            : QColor("#ddccbb");
  replacement.setColor(QPalette::Window, marker);
  QGuiApplication::setPalette(replacement);
  QTRY_COMPARE(window->palette().color(QPalette::Window), marker);
  // Content derivation follows the new palette through the changeEvent seam.
  QTRY_COMPARE(harness.stub->appearance.schemeId, QStringLiteral("system"));
  QGuiApplication::setPalette(previous);
  QTRY_COMPARE(window->palette().color(QPalette::Window),
               previous.color(QPalette::Window));
}

void TerminalWindowTest::monospaceContentFontSurvivesLiveFontChange() {
  // ADR-0116/ADR-0115: the terminal content font is the platform FixedFont;
  // an interface-font change re-derives content without breaking the
  // monospace contract, and the window never pins an interface font itself.
  WindowHarness harness;
  auto window = harness.makeWindow();
  QVERIFY(harness.stub != nullptr);
  QVERIFY(harness.stub->appearance.terminalFont.fixedPitch());
  QVERIFY(!window->testAttribute(Qt::WA_SetFont));

  const QFont previous = QGuiApplication::font();
  // Seed a sentinel so the live-change assertion cannot pass vacuously.
  auto marker = harness.stub->appearance;
  marker.terminalFont.setPointSizeF(marker.terminalFont.pointSizeF() + 7.0);
  window->applyAppearance(marker);
  QCOMPARE(harness.stub->appearance.terminalFont.pointSizeF(),
           marker.terminalFont.pointSizeF());

  QFont interfaceFont(previous);
  interfaceFont.setPointSizeF(previous.pointSizeF() + 3);
  interfaceFont.setFixedPitch(false);
  QGuiApplication::setFont(interfaceFont);
  QTRY_COMPARE(harness.stub->appearance.terminalFont.pointSizeF(),
               QFontDatabase::systemFont(QFontDatabase::FixedFont)
                   .pointSizeF());
  QCOMPARE(harness.stub->appearance.terminalFont.styleHint(), QFont::Monospace);
  QVERIFY(harness.stub->appearance.terminalFont.fixedPitch());
  QVERIFY(!window->testAttribute(Qt::WA_SetFont));
  QGuiApplication::setFont(previous);
}

void TerminalWindowTest::
    accessibilityMetadataIsPresentAndFocusIsOnTerminalView() {
  WindowHarness harness;
  auto window = harness.makeWindow();
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));

  auto *view = window->session()->terminalWidget();
  QVERIFY(view != nullptr);
  QCOMPARE(view->accessibleName(), QStringLiteral("Terminal session"));
  QVERIFY(!view->accessibleDescription().isEmpty());
  QCOMPARE(view->focusPolicy(), Qt::StrongFocus);
  QCOMPARE(window->accessibleName(), QStringLiteral("QindaQt Terminal"));

  auto *find = window->findChild<QAction *>(QStringLiteral("viewFindAction"));
  auto *editor =
      window->findChild<QLineEdit *>(QStringLiteral("terminalFindText"));
  QVERIFY(find != nullptr && editor != nullptr);
  find->trigger();
  QTRY_VERIFY(editor->hasFocus());
  QTest::keyClick(editor, Qt::Key_Escape);
  QTRY_VERIFY(view->hasFocus());
}

void TerminalWindowTest::hostileResizeClampsEmbeddedView() {
  WindowHarness harness;
  auto window = harness.makeWindow();
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
  // P1 regression, part 1 — the production wiring seam. Qt quits when the
  // last window closes by default, which would end the event loop during
  // the bounded teardown escalation; main() must flip it before show().
  // quitOnLastWindowClosed is a QGuiApplication property, hence the cast.
  auto *guiApplication =
      qobject_cast<QGuiApplication *>(QCoreApplication::instance());
  QVERIFY(guiApplication != nullptr);
  QVERIFY(guiApplication->quitOnLastWindowClosed());
  TerminalWindow::prepareApplicationQuitFlow(*guiApplication);
  QVERIFY(!guiApplication->quitOnLastWindowClosed());
  TerminalWindow::prepareApplicationQuitFlow(*guiApplication); // Idempotent.
  QVERIFY(!guiApplication->quitOnLastWindowClosed());

  // P1 regression, part 2 — behavior: a shown window's close must complete
  // the session teardown without any quit leaving the application early.
  WindowHarness harness;
  auto window = harness.makeWindow(false);
  window->newSessionWithDefaultProfile();
  QSignalSpy shutdownSpy(window->session(), &TerminalSession::shutdownFinished);
  QSignalSpy quitSpy(window.get(), &TerminalWindow::closeShutdownFinished);
  QSignalSpy aboutToQuitSpy(guiApplication, &QCoreApplication::aboutToQuit);

  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  window->close();
  QTest::qWait(200);
  QCOMPARE(shutdownSpy.count(), 1);
  QCOMPARE(quitSpy.count(), 1);
  QCOMPARE(window->sessions()->count(), 0);
  QVERIFY(window->session() == nullptr);
  QCOMPARE(aboutToQuitSpy.count(), 0);

  // P1 regression, part 3 — source binding: main() must wire the seam and
  // the queued quit before the first show, so the wiring cannot silently
  // regress while this harness cannot execute main() itself.
  QFile mainSource(
      QStringLiteral(QINDAQT_SOURCE_DIR "/src/apps/terminal/main.cpp"));
  QVERIFY(mainSource.open(QIODevice::ReadOnly));
  const QString source = QString::fromUtf8(mainSource.readAll());
  const qsizetype wiring =
      source.indexOf(QStringLiteral("prepareApplicationQuitFlow"));
  const qsizetype quitConnect =
      source.indexOf(QStringLiteral("connectQuitAfterCloseShutdown"));
  const qsizetype show = source.indexOf(QStringLiteral("window.show()"));
  QVERIFY(wiring >= 0);
  QVERIFY(quitConnect >= 0);
  QVERIFY(show > wiring);
  QVERIFY(show > quitConnect);
}

void TerminalWindowTest::viewActionStatesTrackSessionTruth() {
  WindowHarness harness;
  auto window = harness.makeWindow(false);

  auto *copy = window->findChild<QAction *>(QStringLiteral("editCopyAction"));
  auto *paste = window->findChild<QAction *>(QStringLiteral("editPasteAction"));
  auto *pasteSelection =
      window->findChild<QAction *>(QStringLiteral("editPasteSelectionAction"));
  auto *selectAll =
      window->findChild<QAction *>(QStringLiteral("editSelectAllAction"));
  auto *clear = window->findChild<QAction *>(QStringLiteral("viewClearAction"));
  auto *restart =
      window->findChild<QAction *>(QStringLiteral("sessionRestartAction"));
  QVERIFY(copy && paste && pasteSelection && selectAll && clear && restart);

  // P2-4: without a live view the view operations are disabled, exactly as
  // the accessibility prose claims.
  QVERIFY(!paste->isEnabled());
  QVERIFY(!pasteSelection->isEnabled());
  QVERIFY(!selectAll->isEnabled());
  QVERIFY(!clear->isEnabled());
  QVERIFY(!copy->isEnabled());

  window->newSessionWithDefaultProfile();
  QVERIFY(paste->isEnabled());
  QVERIFY(pasteSelection->isEnabled());
  QVERIFY(selectAll->isEnabled());
  QVERIFY(clear->isEnabled());
  QVERIFY(!copy->isEnabled()); // No selection event yet.

  QVERIFY(QMetaObject::invokeMethod(window->session(), "selectionAvailable",
                                    Q_ARG(bool, true)));
  QVERIFY(copy->isEnabled());

  // View disposal clears selection and deactivates view operations; Restart
  // is refused only while the escalation runs.
  QVERIFY(window->session()->restart());
  QCOMPARE(window->session()->state(), TerminalSession::State::ShuttingDown);
  QVERIFY(!paste->isEnabled());
  QVERIFY(!copy->isEnabled());
  QVERIFY(!restart->isEnabled());
}

void TerminalWindowTest::quitIsRefusedWhileSurvivorRemains() {
  // P1-2: a SIGKILL survivor stays owned. The failed escalation must not
  // emit the quit signal, and further close attempts are refused.
  WindowHarness harness;
  harness.survivorScenario = true;
  auto window = harness.makeWindow();
  QSignalSpy shutdownSpy(window->session(), &TerminalSession::shutdownFinished);
  QSignalSpy quitSpy(window.get(), &TerminalWindow::closeShutdownFinished);

  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  window->close();
  QTest::qWait(600);
  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(!shutdownSpy.first().first().toBool());
  QCOMPARE(quitSpy.count(), 0);
  QCOMPARE(window->session()->state(), TerminalSession::State::ShutdownFailed);
  // The window is re-shown with the failure visible; close stays refused.
  QVERIFY(window->isVisible());
  window->close();
  QVERIFY(window->isVisible());
  QTest::qWait(50);
  QCOMPARE(quitSpy.count(), 0);
}

void TerminalWindowTest::restartThenCloseSpawnsNothingBeforeQuit() {
  // P1 (Dijkstra P1-2 / Church P1-1 / Astra P1-1): the production close
  // route during a pending Restart must reach
  // TerminalSession::beginShutdown() — which cancels the restart in
  // ShuttingDown — so generation 2 is never spawned in front of the queued
  // application quit. The pre-fix parent short-circuited the ShuttingDown
  // close in closeEvent, kept the restart flag true, and this row failed
  // with createdBackends == 2 and a second widget publication.
  WindowHarness harness; // InstantExitMonitor: generation 1 reaps on tick 1.
  auto window = harness.makeWindow();
  QSignalSpy shutdownSpy(window->session(), &TerminalSession::shutdownFinished);
  QSignalSpy quitSpy(window.get(), &TerminalWindow::closeShutdownFinished);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));

  auto *restart =
      window->findChild<QAction *>(QStringLiteral("sessionRestartAction"));
  auto *quit = window->findChild<QAction *>(QStringLiteral("fileQuitAction"));
  QVERIFY(restart != nullptr && quit != nullptr);
  restart->trigger(); // Real action route: enterShutdownSequence(true).
  QCOMPARE(window->session()->state(), TerminalSession::State::ShuttingDown);
  quit->trigger(); // Real close route: closeEvent while ShuttingDown.
  QVERIFY(!window->isVisible());
  window->close(); // Double close while ShuttingDown stays idempotent.
  QTest::qWait(200);

  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(shutdownSpy.first().first().toBool());
  QCOMPARE(window->sessions()->count(), 0);
  QCOMPARE(harness.createdBackends, 1); // No generation 2, ever.
  QVERIFY(window->session() == nullptr);
  QCOMPARE(quitSpy.count(), 1); // Quit exactly once, after generation 1.
}

void TerminalWindowTest::exitedStateDisablesPasteAndKeepsScrollbackOps() {
  // P2 (Dijkstra P2-1 / Astra P2-1): a normal exit deliberately retains the
  // widget for scrollback. Paste needs a live child and must disable on
  // Exited; retained-view operations (select all, clear) and copy with a
  // real selection stay available for the dead generation's buffer.
  WindowHarness harness;
  auto window = harness.makeWindow();
  auto *copy = window->findChild<QAction *>(QStringLiteral("editCopyAction"));
  auto *paste = window->findChild<QAction *>(QStringLiteral("editPasteAction"));
  auto *pasteSelection =
      window->findChild<QAction *>(QStringLiteral("editPasteSelectionAction"));
  auto *selectAll =
      window->findChild<QAction *>(QStringLiteral("editSelectAllAction"));
  auto *clear = window->findChild<QAction *>(QStringLiteral("viewClearAction"));
  QVERIFY(copy && paste && pasteSelection && selectAll && clear);

  QVERIFY(QMetaObject::invokeMethod(window->session(), "selectionAvailable",
                                    Q_ARG(bool, true)));
  QVERIFY(paste->isEnabled());
  QVERIFY(copy->isEnabled());

  QTRY_COMPARE_WITH_TIMEOUT(window->session()->state(),
                            TerminalSession::State::Exited, 5000);
  QVERIFY(window->session()->terminalWidget() != nullptr); // Retained view.
  QVERIFY(!paste->isEnabled());
  QVERIFY(!pasteSelection->isEnabled());
  QVERIFY(selectAll->isEnabled());
  QVERIFY(clear->isEnabled());
  QVERIFY(copy->isEnabled()); // Selection survives on retained scrollback.
}

QTEST_MAIN(TerminalWindowTest)
#include "tst_terminal_window.moc"
