// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_session.h"
#include "session/terminal_session_types.h"

#include <QHash>
#include <QPair>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include <csignal>
#include <memory>
#include <vector>

using QindaQt::Apps::Terminal::ProcessExitInfo;
using QindaQt::Apps::Terminal::ProcessId;
using QindaQt::Apps::Terminal::ProcessMonitor;
using QindaQt::Apps::Terminal::ProcessState;
using QindaQt::Apps::Terminal::TeardownBounds;
using QindaQt::Apps::Terminal::TerminalExitStatus;
using QindaQt::Apps::Terminal::TerminalLaunchRequest;
using QindaQt::Apps::Terminal::TerminalSession;
using QindaQt::Apps::Terminal::TerminalSessionBackend;

namespace {

constexpr ProcessId kFirstPid = 4242;
constexpr ProcessId kSecondPid = 4243;

TerminalLaunchRequest validRequest() {
  return TerminalLaunchRequest{
      .program = QStringLiteral("/bin/qindaqt-test-shell"),
      .arguments = {QStringLiteral("-l")},
      .workingDirectory = {},
      .environment = {QStringLiteral("PATH=/usr/bin")},
      .title = {}};
}

// Shared per-backend statistics that outlive the backend object itself, so
// assertions stay valid after the session disposes a generation.
struct BackendStats final {
  int startCalls = 0;
  int shutdownCalls = 0;
  QWidget *lastWidget = nullptr;
};

class FakeBackend;

// Test double for the rendering adapter: no PTY, no processes,
// deterministic child identity per generation.
class FakeBackend final : public TerminalSessionBackend {
public:
  FakeBackend(bool failStart, QString failureDiagnostic,
              ProcessId pid, QSharedPointer<BackendStats> stats,
              QObject *parent = nullptr)
      : TerminalSessionBackend(parent), m_failStart(failStart),
        m_failureDiagnostic(std::move(failureDiagnostic)), m_pid(pid),
        m_stats(std::move(stats)) {}

  ~FakeBackend() override { delete m_widget; }

  StartOutcome start(const TerminalLaunchRequest &request) override {
    ++m_stats->startCalls;
    if (m_failStart) {
      return {.ok = false, .diagnostic = m_failureDiagnostic};
    }
    m_widget = new QWidget();
    m_widget->setObjectName(QStringLiteral("fakeTerminalView%1").arg(qlonglong(m_pid)));
    m_stats->lastWidget = m_widget;
    return {.ok = true, .diagnostic = {}};
  }

  void requestShutdown() override {
    ++m_stats->shutdownCalls;
    m_events.append(QStringLiteral("shutdown"));
    delete m_widget;
    m_widget = nullptr;
  }

  [[nodiscard]] ProcessId shellProcessId() const override { return m_pid; }
  [[nodiscard]] QWidget *terminalWidget() override { return m_widget; }

  void copySelectionToClipboard() override { m_viewOperations.append(QStringLiteral("copy")); }
  void pasteClipboardToSession() override { m_viewOperations.append(QStringLiteral("paste")); }
  void pastePrimarySelectionToSession() override { m_viewOperations.append(QStringLiteral("paste-selection")); }
  void selectAllInView() override { m_viewOperations.append(QStringLiteral("select-all")); }
  void clearView() override { m_viewOperations.append(QStringLiteral("clear")); }
  [[nodiscard]] bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &text) override { Q_UNUSED(text); }

  [[nodiscard]] int shutdownCalls() const { return m_stats->shutdownCalls; }
  [[nodiscard]] QStringList events() const { return m_events; }
  [[nodiscard]] QStringList viewOperations() const {
    return m_viewOperations;
  }

private:
  bool m_failStart = false;
  QString m_failureDiagnostic;
  ProcessId m_pid = 0;
  QSharedPointer<BackendStats> m_stats;
  QWidget *m_widget = nullptr;
  QStringList m_events;
  QStringList m_viewOperations;
};

// Test double for process observation: each PID gets a script of how many
// reaps still report Running before the programmed exit disposition. PIDs
// without a script follow the default (running forever, or the programmed
// disposition immediately).
class FakeMonitor final : public ProcessMonitor {
public:
  [[nodiscard]] ProcessExitInfo reap(ProcessId pid) override {
    ++m_reapCalls;
    const auto scripted = m_runningReaps.constFind(pid);
    if (scripted != m_runningReaps.constEnd()) {
      if (scripted.value() > 0) {
        --scripted.value();
        return {.state = ProcessState::Running, .signaled = false, .code = 0};
      }
      return m_exitDisposition;
    }
    if (m_defaultRunning) {
      return {.state = ProcessState::Running, .signaled = false, .code = 0};
    }
    return m_exitDisposition;
  }

  [[nodiscard]] bool signalProcessGroup(ProcessId groupLeader,
                                        int signalNumber) override {
    m_signals.append({groupLeader, signalNumber});
    return !m_refuseSignals;
  }

  void scriptRunningThenExit(ProcessId pid, int runningReaps) {
    m_runningReaps.insert(pid, runningReaps);
  }
  void setExitDisposition(ProcessState state, bool signaled, int code) {
    m_exitDisposition = ProcessExitInfo{state, signaled, code};
  }
  void setDefaultRunning(bool running) { m_defaultRunning = running; }
  [[nodiscard]] const QVector<QPair<ProcessId, int>> &
  signalsSent() const {
    return m_signals;
  }

private:
  QHash<ProcessId, int> m_runningReaps;
  ProcessExitInfo m_exitDisposition{ProcessState::Exited, false, 0};
  QVector<QPair<ProcessId, int>> m_signals;
  bool m_defaultRunning = false;
  bool m_refuseSignals = false;
};

struct SessionHarness final {
  FakeMonitor monitor;
  std::vector<QSharedPointer<BackendStats>> backendStats;
  int createdBackends = 0;
  TeardownBounds bounds{30, 30, 30, 1};

  std::unique_ptr<TerminalSession> makeSession(bool failStart = false) {
    SessionHarness *self = this;
    TerminalSession::BackendFactory factory = [self, failStart]() {
      auto stats = QSharedPointer<BackendStats>::create();
      const ProcessId pid = kFirstPid + self->createdBackends;
      ++self->createdBackends;
      self->backendStats.push_back(stats);
      return std::unique_ptr<TerminalSessionBackend>(
          std::make_unique<FakeBackend>(failStart,
                                        QStringLiteral("cannot fork"), pid,
                                        stats));
    };
    return std::make_unique<TerminalSession>(std::move(factory), &monitor,
                                             bounds);
  }
};

} // namespace

class TerminalSessionTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // QSignalSpy stores arguments in QVariants; registration guarantees the
    // retrieval below works on every Qt build.
    qRegisterMetaType<TerminalExitStatus>("TerminalExitStatus");
  }
  void successfulStartPublishesWidgetAndRunningState();
  void emptyProgramPublishesTypedStartFailure();
  void backendStartFailurePublishesDiagnosticWithoutWidget();
  void exitCodesAndSignalsArePublishedDistinctly();
  void duplicateExitPublicationIsSuppressed();
  void shutdownCompletesWhenChildExitsAfterMasterClose();
  void shutdownEscalatesTermThenKillAndReportsFailureHonestly();
  void restartReplacesGenerationWithoutBackendReuse();
  void restartRejectedWhileShuttingDown();
  void idleShutdownCompletesCleanlyWithoutSignals();
  void disposalOrderIsViewThenBackend();
  void presentationOperationsRouteThroughBackend();

private:
  static void pump(int milliseconds) { QTest::qWait(milliseconds); }
};

void TerminalSessionTest::successfulStartPublishesWidgetAndRunningState() {
  SessionHarness harness;
  harness.monitor.setDefaultRunning(true);
  auto session = harness.makeSession();
  QSignalSpy widgetSpy(session.get(),
                       &TerminalSession::terminalWidgetChanged);
  QSignalSpy stateSpy(session.get(), &TerminalSession::stateChanged);

  QVERIFY(session->start(validRequest()));
  QCOMPARE(session->state(), TerminalSession::State::Running);
  QCOMPARE(widgetSpy.count(), 1);
  QVERIFY(session->terminalWidget() != nullptr);
  QCOMPARE(harness.createdBackends, 1);
  QCOMPARE(harness.backendStats.at(0)->startCalls, 1);
  QVERIFY(stateSpy.count() >= 1);
  // A running generation keeps polling; nothing exits underneath us.
  pump(50);
  QCOMPARE(session->state(), TerminalSession::State::Running);
}

void TerminalSessionTest::emptyProgramPublishesTypedStartFailure() {
  SessionHarness harness;
  auto session = harness.makeSession();
  QSignalSpy exitSpy(session.get(), &TerminalSession::sessionFinished);

  QVERIFY(!session->start(TerminalLaunchRequest{}));
  QCOMPARE(session->state(), TerminalSession::State::Exited);
  QCOMPARE(exitSpy.count(), 1);
  const auto status = exitSpy.first().first().value<TerminalExitStatus>();
  QCOMPARE(status.kind, TerminalExitStatus::Kind::StartFailed);
  QCOMPARE(harness.createdBackends, 0);
}

void TerminalSessionTest::
    backendStartFailurePublishesDiagnosticWithoutWidget() {
  SessionHarness harness;
  auto session = harness.makeSession(/*failStart=*/true);
  QSignalSpy exitSpy(session.get(), &TerminalSession::sessionFinished);
  QSignalSpy widgetSpy(session.get(),
                       &TerminalSession::terminalWidgetChanged);

  QVERIFY(!session->start(validRequest()));
  QCOMPARE(session->state(), TerminalSession::State::Exited);
  QCOMPARE(exitSpy.count(), 1);
  const auto status = exitSpy.first().first().value<TerminalExitStatus>();
  QCOMPARE(status.kind, TerminalExitStatus::Kind::StartFailed);
  QCOMPARE(status.diagnostic, QStringLiteral("cannot fork"));
  QCOMPARE(widgetSpy.count(), 0);
  QVERIFY(session->terminalWidget() == nullptr);
}

void TerminalSessionTest::exitCodesAndSignalsArePublishedDistinctly() {
  SessionHarness harness;
  harness.monitor.scriptRunningThenExit(kFirstPid, 3);
  harness.monitor.setExitDisposition(ProcessState::Exited, false, 3);
  auto session = harness.makeSession();
  QSignalSpy exitSpy(session.get(), &TerminalSession::sessionFinished);
  QVERIFY(session->start(validRequest()));
  pump(200);
  QCOMPARE(exitSpy.count(), 1);
  const auto status = exitSpy.first().first().value<TerminalExitStatus>();
  QCOMPARE(status.kind, TerminalExitStatus::Kind::Normal);
  QCOMPARE(status.code, 3);
  QCOMPARE(session->state(), TerminalSession::State::Exited);

  // A signal death reports the signal number, not a bogus exit code.
  harness.monitor.setExitDisposition(ProcessState::Exited, true, SIGKILL);
  auto crashSession = harness.makeSession();
  QSignalSpy crashSpy(crashSession.get(), &TerminalSession::sessionFinished);
  harness.monitor.scriptRunningThenExit(kSecondPid, 2);
  QVERIFY(crashSession->start(validRequest()));
  pump(200);
  QCOMPARE(crashSpy.count(), 1);
  const auto crashStatus =
      crashSpy.first().first().value<TerminalExitStatus>();
  QCOMPARE(crashStatus.kind, TerminalExitStatus::Kind::Signal);
  QCOMPARE(crashStatus.code, int{SIGKILL});
}

void TerminalSessionTest::duplicateExitPublicationIsSuppressed() {
  SessionHarness harness;
  harness.monitor.scriptRunningThenExit(kFirstPid, 1);
  auto session = harness.makeSession();
  QSignalSpy exitSpy(session.get(), &TerminalSession::sessionFinished);
  QVERIFY(session->start(validRequest()));
  // The script runs out quickly; many polls must yield exactly one exit.
  pump(300);
  pump(100);
  QCOMPARE(exitSpy.count(), 1);
  QCOMPARE(session->state(), TerminalSession::State::Exited);
}

void TerminalSessionTest::
    shutdownCompletesWhenChildExitsAfterMasterClose() {
  SessionHarness harness;
  harness.monitor.scriptRunningThenExit(kFirstPid, 50);
  auto session = harness.makeSession();
  QSignalSpy shutdownSpy(session.get(), &TerminalSession::shutdownFinished);
  QVERIFY(session->start(validRequest()));

  session->beginShutdown();
  QCOMPARE(session->state(), TerminalSession::State::ShuttingDown);
  pump(400);
  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(shutdownSpy.first().first().toBool());
  QCOMPARE(session->state(), TerminalSession::State::ShutdownComplete);
  QVERIFY(session->terminalWidget() == nullptr);
  // SIGHUP via master close sufficed; no escalation signal was needed.
  QVERIFY(harness.monitor.signalsSent().isEmpty());
}

void TerminalSessionTest::
    shutdownEscalatesTermThenKillAndReportsFailureHonestly() {
  SessionHarness harness;
  harness.monitor.setDefaultRunning(true);
  auto session = harness.makeSession();
  QSignalSpy shutdownSpy(session.get(), &TerminalSession::shutdownFinished);
  QVERIFY(session->start(validRequest()));

  session->beginShutdown();
  pump(600);
  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(!shutdownSpy.first().first().toBool());
  QCOMPARE(session->state(), TerminalSession::State::ShutdownFailed);
  const auto signalsSent = harness.monitor.signalsSent();
  QCOMPARE(signalsSent.size(), 2);
  QCOMPARE(signalsSent.at(0).first, kFirstPid);
  QCOMPARE(signalsSent.at(0).second, int{SIGTERM});
  QCOMPARE(signalsSent.at(1).second, int{SIGKILL});
  // A failed shutdown stays failed: start() may not silently replace a
  // generation whose child may still be alive.
  QVERIFY(!session->start(validRequest()));
}

void TerminalSessionTest::restartReplacesGenerationWithoutBackendReuse() {
  SessionHarness harness;
  harness.monitor.setDefaultRunning(true);
  harness.monitor.scriptRunningThenExit(kFirstPid, 10);
  auto session = harness.makeSession();
  QSignalSpy widgetSpy(session.get(),
                       &TerminalSession::terminalWidgetChanged);
  QSignalSpy shutdownSpy(session.get(), &TerminalSession::shutdownFinished);
  QVERIFY(session->start(validRequest()));

  QVERIFY(session->restart());
  pump(500);
  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(shutdownSpy.first().first().toBool());
  QCOMPARE(session->state(), TerminalSession::State::Running);
  QCOMPARE(widgetSpy.count(), 2);
  QCOMPARE(harness.createdBackends, 2);
  QCOMPARE(harness.backendStats.at(0)->startCalls, 1);
  QCOMPARE(harness.backendStats.at(0)->shutdownCalls, 1);
  QCOMPARE(harness.backendStats.at(1)->startCalls, 1);
  // Each generation is single-use and owns its own view.
  QVERIFY(harness.backendStats.at(0)->lastWidget !=
          harness.backendStats.at(1)->lastWidget);
  QVERIFY(session->terminalWidget() != nullptr);
}

void TerminalSessionTest::restartRejectedWhileShuttingDown() {
  SessionHarness harness;
  harness.monitor.setDefaultRunning(true);
  auto session = harness.makeSession();
  QVERIFY(session->start(validRequest()));
  session->beginShutdown();
  QVERIFY(!session->restart());
  QVERIFY(!session->start(validRequest()));
  pump(600);
  QCOMPARE(session->state(), TerminalSession::State::ShutdownFailed);
}

void TerminalSessionTest::idleShutdownCompletesCleanlyWithoutSignals() {
  SessionHarness harness;
  auto session = harness.makeSession();
  QSignalSpy shutdownSpy(session.get(), &TerminalSession::shutdownFinished);

  session->beginShutdown();
  pump(200);
  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(shutdownSpy.first().first().toBool());
  QCOMPARE(session->state(), TerminalSession::State::ShutdownComplete);
  QVERIFY(harness.monitor.signalsSent().isEmpty());
  QCOMPARE(harness.createdBackends, 0);
}

void TerminalSessionTest::disposalOrderIsViewThenBackend() {
  SessionHarness harness;
  harness.monitor.scriptRunningThenExit(kFirstPid, 50);
  auto session = harness.makeSession();
  QVERIFY(session->start(validRequest()));
  auto *backendStats = harness.backendStats.at(0).data();
  QStringList disposalOrder;
  connect(session.get(), &TerminalSession::viewDisposalRequested,
          session.get(), [&disposalOrder] {
            disposalOrder.append(QStringLiteral("view"));
          });
  connect(session.get(), &TerminalSession::viewDisposalRequested,
          session.get(), [backendStats, &disposalOrder] {
            // The backend's own shutdown has not run yet at this point.
            disposalOrder.append(backendStats->shutdownCalls == 0
                                     ? QStringLiteral("before-backend")
                                     : QStringLiteral("after-backend"));
          });

  session->beginShutdown();
  pump(400);
  QCOMPARE(disposalOrder, QStringList{QStringLiteral("view"),
                                      QStringLiteral("before-backend")});
  QCOMPARE(backendStats->shutdownCalls, 1);
}

void TerminalSessionTest::presentationOperationsRouteThroughBackend() {
  SessionHarness harness;
  harness.monitor.setDefaultRunning(true);
  auto session = harness.makeSession();
  // Before any backend exists every operation must be a safe no-op.
  session->copySelectionToClipboard();
  session->pasteClipboardToSession();
  session->pastePrimarySelectionToSession();
  session->selectAllInView();
  session->clearView();
  QCOMPARE(harness.createdBackends, 0);

  QVERIFY(session->start(validRequest()));
  session->copySelectionToClipboard();
  session->pasteClipboardToSession();
  session->pastePrimarySelectionToSession();
  session->selectAllInView();
  session->clearView();
  QCOMPARE(harness.createdBackends, 1);
  // No crash: routing went through exactly the started generation.
  QVERIFY(session->terminalWidget() != nullptr);
  QVERIFY(session->terminalWidget() ==
          harness.backendStats.at(0)->lastWidget);
}

QTEST_MAIN(TerminalSessionTest)
#include "tst_terminal_session.moc"
