// SPDX-License-Identifier: GPL-3.0-or-later
// Deterministic state-machine tests for the lock greeter's authentication
// lifecycle (ADR-0382). The controller runs against fake ports: no Wayland, no
// PAM child, no timers. A worker "timeout" is simply the runner reporting
// Outcome::TimedOut, which is exactly what WorkerProcess does at its deadline.
#include "authentication_controller.h"
#include "fake_authentication_ports.h"
#include <QSignalSpy>
#include <QtTest>
using namespace QindaQt::LockGreeter;
using namespace QindaQt::LockAuthentication;

namespace {
using namespace TestSupport;
struct Harness {
  FakeSession session;
  FakeRunner runner;
  AuthenticationController controller{session, runner, QStringLiteral("tester")};
  QSignalSpy cleared{&controller, &AuthenticationController::clearFields};
  QSignalSpy finished{&controller, &AuthenticationController::finished};
  QSignalSpy changed{&controller, &AuthenticationController::changed};
  // Locked, one attempt started by input, first secret prompt on screen.
  void promptReady() {
    session.lock();
    controller.noteInput();
    runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
  }
};
} // namespace

class AuthenticationControllerTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void init() {
    // CTest runs this binary with QT_FATAL_WARNINGS=0 because two rows trigger
    // the greeter's exit-reason warnings on purpose and verify them with
    // ignoreMessage. Any OTHER warning still fails the row.
    QTest::failOnWarning();
  }
  void noAttemptWithoutInput() {
    Harness h;
    QVERIFY(!h.controller.locked());
    h.session.lock();
    // The screen is locked and the field is usable, but nothing talks to PAM.
    QVERIFY(h.controller.locked());
    QVERIFY(!h.controller.busy());
    QVERIFY(!h.controller.waiting());
    QCOMPARE(h.runner.starts, 0);
    QCoreApplication::processEvents();
    QCOMPARE(h.runner.starts, 0);
    // Things that are not input start nothing either.
    h.controller.respond(QStringLiteral("stray"));
    h.controller.cancel();
    h.session.lock(); // duplicate lock notification
    QCOMPARE(h.runner.starts, 0);
    QVERIFY(h.runner.responses.empty());
    QVERIFY(h.controller.status().isEmpty());
  }
  void inputBeforeLockWaitsForTheLock() {
    Harness h;
    h.controller.noteInput();
    h.controller.submit(QStringLiteral("early")); // not locked: dropped
    QCOMPARE(h.runner.starts, 0);
    h.session.lock();
    QCOMPARE(h.runner.starts, 1); // The earlier input counts as the first input.
    QVERIFY(h.runner.responses.empty());
  }
  void firstInputStartsExactlyOneAttempt() {
    Harness h;
    h.session.lock();
    for (int i = 0; i < 5; ++i) h.controller.noteInput();
    QCOMPARE(h.runner.starts, 1);
    QVERIFY(h.controller.busy());
    QVERIFY(!h.controller.waiting());
    QVERIFY(h.runner.current.epoch != 0 && h.runner.current.request != 0);
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QVERIFY(h.controller.waiting());
    QVERIFY(h.controller.secret());
    QCOMPARE(h.controller.promptText(), QStringLiteral("Password: "));
    h.controller.noteInput(); // More typing never starts a second attempt.
    QCOMPARE(h.runner.starts, 1);
  }
  void timeoutReturnsToReadyAndEnterSubmits() {
    // The acceptance flow: idle -> input -> attempt -> timeout -> ready again.
    Harness h;
    h.session.lock();
    QCOMPARE(h.runner.starts, 0);
    h.controller.noteInput();
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QVERIFY(h.controller.waiting());
    const auto first = h.runner.current;
    const auto clearedBefore = h.cleared.size();
    const auto changedBefore = h.changed.size();
    h.runner.finish(Outcome::TimedOut); // WorkerProcess's whole-attempt deadline
    QVERIFY(h.changed.size() > changedBefore); // QML refocuses the field on changed().
    QVERIFY(!h.controller.busy());
    QVERIFY(!h.controller.waiting());
    QVERIFY(h.controller.locked()); // Still locked; the field is simply ready again.
    QVERIFY(h.controller.status().contains(QStringLiteral("Timed out")));
    QVERIFY(h.cleared.size() > clearedBefore); // An abandoned attempt keeps no partial password.
    QCOMPARE(h.runner.starts, 1); // No automatic restart: nothing without input.
    // Keyboard-only: the next key starts a fresh attempt ...
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 2);
    QVERIFY(h.runner.current.request > first.request);
    QCOMPARE(h.runner.current.epoch, first.epoch);
    QVERIFY(h.controller.status().isEmpty());
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    // ... and Enter submits to it.
    h.controller.submit(QStringLiteral("correct horse"));
    QCOMPARE(h.runner.responses.size(), std::size_t{1});
    QCOMPARE(h.runner.responses[0], QStringLiteral("correct horse"));
    QVERIFY(h.controller.verifying());
    h.runner.finish(Outcome::Authenticated);
    QCOMPARE(h.session.unlocks, 1);
    QCOMPARE(h.finished.size(), 0); // finished only after the server processed the unlock
    // Every way the locker ends says why (the journal is the only record).
    QTest::ignoreMessage(QtInfoMsg, "unlock processed by the compositor; the locker is finished");
    h.session.processUnlock();
    QCOMPARE(h.finished.size(), 1);
  }
  void enterFromIdleStartsAnAttemptAndHoldsTheText() {
    Harness h;
    h.session.lock();
    const auto clearedBefore = h.cleared.size();
    h.controller.submit(QStringLiteral("pw-from-idle"));
    QCOMPARE(h.runner.starts, 1);
    QVERIFY(h.cleared.size() > clearedBefore); // The text moved into the controller.
    QVERIFY(h.runner.responses.empty()); // Nothing can be sent before PAM asks.
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(h.runner.responses.size(), std::size_t{1});
    QCOMPARE(h.runner.responses[0], QStringLiteral("pw-from-idle"));
    QVERIFY(h.controller.verifying());
    QVERIFY(!h.controller.waiting());
  }
  void enterWhileStartingHoldsTheTextForTheFirstPrompt() {
    Harness h;
    h.session.lock();
    h.controller.noteInput();
    h.controller.submit(QStringLiteral("typed fast"));
    QVERIFY(h.runner.responses.empty());
    h.runner.offer(MessageKind::Information, QStringLiteral("Place your finger on the reader"));
    QVERIFY(h.runner.responses.empty()); // Information is not a prompt.
    QCOMPARE(h.controller.status(), QStringLiteral("Place your finger on the reader"));
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(h.runner.responses.size(), std::size_t{1});
    QCOMPARE(h.runner.responses[0], QStringLiteral("typed fast"));
  }
  void enterAtThePromptSendsImmediately() {
    Harness h;
    h.promptReady();
    h.controller.submit(QStringLiteral("now"));
    QCOMPARE(h.runner.responses.size(), std::size_t{1});
    QCOMPARE(h.runner.responses[0], QStringLiteral("now"));
    QVERIFY(h.controller.verifying());
    QCOMPARE(h.controller.status(), QStringLiteral("Checking…"));
  }
  void typeAheadSurvivesAttemptStartAndFirstPrompt() {
    Harness h;
    h.session.lock();
    const auto before = h.cleared.size();
    h.controller.noteInput(); // The first key press, whose character is already in the field.
    QCOMPARE(h.cleared.size(), before);
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(h.cleared.size(), before); // The typed password is not wiped by the prompt.
    QVERIFY(h.controller.waiting());
  }
  void laterOrVisiblePromptsDiscardStaleText() {
    {
      Harness h; // A visible first prompt cannot be assumed to want the password.
      h.session.lock();
      h.controller.submit(QStringLiteral("password-text"));
      const auto before = h.cleared.size();
      h.runner.offer(MessageKind::Visible, QStringLiteral("Code: "));
      QVERIFY(h.cleared.size() > before);
      QVERIFY(h.runner.responses.empty()); // Never sent to the wrong prompt.
      QVERIFY(!h.controller.secret());
      QVERIFY(h.controller.waiting());
    }
    {
      Harness h; // A second prompt after a response starts clean.
      h.promptReady();
      h.controller.submit(QStringLiteral("first"));
      const auto before = h.cleared.size();
      h.runner.offer(MessageKind::Visible, QStringLiteral("Second factor: "));
      QVERIFY(h.cleared.size() > before);
      QVERIFY(h.controller.waiting());
      QVERIFY(!h.controller.verifying());
      QVERIFY(h.controller.status().isEmpty()); // "Checking..." does not outlive the answer.
      QCOMPARE(h.runner.responses.size(), std::size_t{1});
    }
  }
  void emptyPasswordIsNeverSent() {
    Harness h;
    h.promptReady();
    h.controller.submit(QString());
    QVERIFY(h.runner.responses.empty()); // It could never authenticate; it would only count as a failure.
    QVERIFY(h.controller.waiting());
    QVERIFY(!h.controller.status().isEmpty()); // But Enter visibly does something.
    h.controller.submit(QStringLiteral("pw"));
    QCOMPARE(h.runner.responses.size(), std::size_t{1});
    // An empty answer to a visible prompt is legitimate ("press Enter to continue").
    Harness v;
    v.session.lock();
    v.controller.noteInput();
    v.runner.offer(MessageKind::Visible, QStringLiteral("Press Enter: "));
    v.controller.submit(QString());
    QCOMPARE(v.runner.responses.size(), std::size_t{1});
    // Enter on an empty field from Idle starts the attempt but queues nothing.
    Harness i;
    i.session.lock();
    i.controller.submit(QString());
    QCOMPARE(i.runner.starts, 1);
    i.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QVERIFY(i.runner.responses.empty());
    QVERIFY(i.controller.waiting());
  }
  void enterWhileVerifyingKeepsTheTypedAheadText() {
    Harness h;
    h.promptReady();
    h.controller.submit(QStringLiteral("wrong"));
    QVERIFY(h.controller.verifying());
    const auto before = h.cleared.size();
    h.controller.submit(QStringLiteral("second try")); // Typed ahead during pam_faildelay.
    QCOMPARE(h.runner.responses.size(), std::size_t{1}); // Not a second answer to the same prompt.
    QCOMPARE(h.cleared.size(), before); // So the field keeps it.
    h.runner.finish(Outcome::Denied);
    QVERIFY(!h.controller.busy());
    // Enter now works from the ready state with the kept text.
    h.controller.submit(QStringLiteral("second try"));
    QCOMPARE(h.runner.starts, 2);
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(h.runner.responses.size(), std::size_t{2});
    QCOMPARE(h.runner.responses[1], QStringLiteral("second try"));
  }
  void failureReturnsToReadyWithoutRestarting_data() {
    QTest::addColumn<int>("outcome");
    QTest::addColumn<QString>("expected");
    QTest::newRow("wrong-password") << int(Outcome::Denied) << QStringLiteral("Authentication failed");
    QTest::newRow("account-denied") << int(Outcome::AccountDenied) << QStringLiteral("cannot unlock");
    QTest::newRow("unavailable") << int(Outcome::Unavailable) << QStringLiteral("unavailable");
    QTest::newRow("timed-out") << int(Outcome::TimedOut) << QStringLiteral("Timed out");
  }
  void failureReturnsToReadyWithoutRestarting() {
    QFETCH(int, outcome);
    QFETCH(QString, expected);
    Harness h;
    h.promptReady();
    h.controller.submit(QStringLiteral("pw"));
    h.runner.finish(static_cast<Outcome>(outcome));
    QVERIFY(!h.controller.busy());
    QVERIFY(!h.controller.waiting());
    QVERIFY(h.controller.locked());
    QVERIFY2(h.controller.status().contains(expected), qPrintable(h.controller.status()));
    QCOMPARE(h.runner.starts, 1); // PAM is never contacted again without input.
    QCOMPARE(h.session.unlocks, 0);
    h.controller.noteInput(); // A prompt was shown, so plain input retries.
    QCOMPARE(h.runner.starts, 2);
    QVERIFY(h.controller.status().isEmpty());
  }
  void attemptThatDiesBeforeAnyPromptStopsRespawning() {
    Harness h;
    h.session.lock();
    h.controller.noteInput();
    h.runner.finish(Outcome::Denied); // e.g. a faillock-ed account: refused before asking.
    QVERIFY(h.controller.status().contains(QStringLiteral("refused")));
    QVERIFY(h.controller.status().contains(QStringLiteral("locked")));
    for (int i = 0; i < 20; ++i) h.controller.noteInput(); // Mashing keys must not spawn workers.
    QCOMPARE(h.runner.starts, 1);
    h.controller.submit(QStringLiteral("pw")); // Enter is explicit intent.
    QCOMPARE(h.runner.starts, 2);
    // Once a conversation really prompts, plain input is trusted again.
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    h.runner.finish(Outcome::Denied);
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 3);
  }
  void workerThatCannotStartIsReportedAndNotRetriedPerKey() {
    Harness h;
    h.runner.startFails = true;
    h.session.lock();
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 1);
    QVERIFY(!h.controller.busy());
    QVERIFY(h.controller.status().contains(QStringLiteral("unavailable")));
    h.controller.noteInput();
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 1);
    h.runner.startFails = false;
    h.controller.begin(); // Explicit retry works once the worker can start.
    QCOMPARE(h.runner.starts, 2);
    QVERIFY(h.controller.busy());
  }
  void queuedTextDiesWithAnAttemptThatNeverPrompted() {
    Harness h;
    h.session.lock();
    h.controller.submit(QStringLiteral("must-not-leak"));
    h.runner.finish(Outcome::Denied); // Died before asking anything.
    h.controller.noteInput();
    h.controller.begin();
    h.runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QVERIFY(h.runner.responses.empty()); // The old text was wiped, not replayed.
  }
  void cancelClearsAndRearmsInput() {
    Harness h;
    h.promptReady();
    const auto stale = h.runner.current;
    const auto before = h.cleared.size();
    h.controller.cancel();
    QCOMPARE(h.runner.cancels, 1);
    QVERIFY(h.cleared.size() > before);
    QVERIFY(!h.controller.waiting());
    QVERIFY(h.controller.status().isEmpty());
    // The cancelled child is reaped later; its completion is stale and harmless.
    h.runner.finish(Outcome::Authenticated, stale);
    QCOMPARE(h.session.unlocks, 0);
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 2);
  }
  void inputWhileTheCancelledChildIsStillBeingReapedStartsAfterwards() {
    Harness h;
    h.promptReady();
    const auto stale = h.runner.current;
    h.controller.cancel();
    QVERIFY(h.runner.active()); // SIGKILL sent, child not yet reaped.
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 1); // Cannot start on top of the old child ...
    h.runner.finish(Outcome::Cancelled, stale);
    QCOMPARE(h.runner.starts, 2); // ... but never loses the user's input.
    QVERIFY(h.controller.busy());
  }
  void lateFramesOfACancelledAttemptAreIgnored() {
    Harness h;
    h.promptReady();
    h.controller.cancel();
    h.runner.offer(MessageKind::Secret, QStringLiteral("late prompt"));
    QVERIFY(!h.controller.waiting());
    h.controller.respond(QStringLiteral("late response"));
    QVERIFY(h.runner.responses.empty());
  }
  void approvalIsConsumedOnceAndOnlyWhenCurrent() {
    Harness h;
    h.promptReady();
    h.controller.submit(QStringLiteral("pw"));
    const auto token = h.runner.current;
    h.runner.finish(Outcome::Authenticated, token);
    QCOMPARE(h.session.unlocks, 1);
    h.runner.finish(Outcome::Authenticated, token); // Duplicate approval.
    QCOMPARE(h.session.unlocks, 1);
    h.controller.noteInput(); // The epoch is spent: nothing can start again.
    QCOMPARE(h.runner.starts, 1);
  }
  void refusedUnlockIsReported() {
    Harness h;
    h.session.unlockAccepted = false;
    h.promptReady();
    h.controller.submit(QStringLiteral("pw"));
    QTest::ignoreMessage(QtWarningMsg, "the compositor refused the standard unlock request");
    h.runner.finish(Outcome::Authenticated);
    QCOMPARE(h.session.unlocks, 1);
    QCOMPARE(h.controller.status(), QStringLiteral("Unlock unavailable."));
  }
  void transportFailureIsNeverApproval() {
    Harness h;
    h.runner.respondAccepted = false;
    h.promptReady();
    h.controller.submit(QStringLiteral("pw"));
    QCOMPARE(h.runner.cancels, 1);
    QCOMPARE(h.session.unlocks, 0);
  }
  void compositorFinishingTheLockEndsTheLocker() {
    Harness h;
    h.promptReady();
    QTest::ignoreMessage(QtWarningMsg, "the compositor finished the session lock; the locker exits without unlocking");
    h.session.reject();
    QCOMPARE(h.finished.size(), 1);
    QVERIFY(h.runner.cancels >= 1);
    h.controller.noteInput();
    QCOMPARE(h.runner.starts, 1); // The finished lock cannot start another attempt.
  }
  void aLockFinishedBeforeAnyoneListenedEndsTheLocker() {
    // The compositor can finish the lock while Qt is still starting its platform,
    // before the controller exists. That is an exit (a lingering locker looks
    // alive to compositor supervision), delivered once the event loop runs.
    FakeSession session;
    session.available = false;
    FakeRunner runner;
    QTest::ignoreMessage(QtWarningMsg, "the compositor finished the session lock; the locker exits without unlocking");
    AuthenticationController controller(session, runner, QStringLiteral("tester"));
    QSignalSpy finished(&controller, &AuthenticationController::finished);
    QTRY_COMPARE(finished.size(), 1);
    controller.noteInput();
    QCOMPARE(runner.starts, 0);
  }
  void layoutIsForwarded() {
    Harness h;
    QCOMPARE(h.controller.layout(), QStringLiteral("English (US)"));
    QCOMPARE(h.controller.user(), QStringLiteral("tester"));
  }
};
QTEST_GUILESS_MAIN(AuthenticationControllerTest)
#include "tst_authentication_controller.moc"
