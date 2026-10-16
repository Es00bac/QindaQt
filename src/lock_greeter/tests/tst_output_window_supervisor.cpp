// SPDX-License-Identifier: GPL-3.0-or-later
// Retry and backoff rules for lock windows that cannot be created (ADR-0382).
// The factory is scripted and the scheduler is manual: no QTimer, no sleeping.
#include "output_window_supervisor.h"
#include <QtTest>
#include <deque>
#include <functional>
#include <map>
#include <vector>
using namespace QindaQt::LockGreeter;
using std::chrono::milliseconds;
using Result = OutputWindowFactory::Result;

namespace {
class FakeFactory final : public OutputWindowFactory {
public:
  Result create(OutputKey output) override {
    ++creates[output];
    if (onCreate) onCreate(output);
    auto &queue = script[output];
    if (queue.empty()) return Result::Created;
    const auto next = queue.front();
    queue.pop_front();
    return next;
  }
  void destroy(OutputKey output) override { destroyed.push_back(output); }
  std::map<OutputKey, std::deque<Result>> script;
  std::map<OutputKey, int> creates;
  std::vector<OutputKey> destroyed;
  std::function<void(OutputKey)> onCreate;
};
class ManualScheduler final : public RetryScheduler {
public:
  void schedule(OutputKey output, milliseconds delay) override {
    pending[output] = delay;
    history[output].push_back(delay.count());
  }
  void cancel(OutputKey output) override {
    pending.erase(output);
    ++cancels;
  }
  std::map<OutputKey, milliseconds> pending;
  std::map<OutputKey, std::vector<long long>> history;
  int cancels = 0;
};
int first = 1, second = 2, third = 3;
OutputKey A = &first, B = &second, C = &third;
} // namespace

class OutputWindowSupervisorTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void backoffDoublesToThirtySecondsAndNeverStops() {
    RetryBackoff backoff;
    std::vector<long long> delays;
    for (int i = 0; i < 12; ++i) delays.push_back(backoff.next().count());
    const std::vector<long long> expected{250, 500, 1000, 2000, 4000, 8000, 16000, 30000, 30000, 30000, 30000, 30000};
    QCOMPARE(delays, expected);
    QCOMPARE(backoff.failures(), 12U);
    backoff.reset();
    QCOMPARE(backoff.failures(), 0U);
    QCOMPARE(backoff.next().count(), 250LL);
    RetryBackoff clamped(milliseconds(60000), milliseconds(5000)); // Initial above the cap.
    QCOMPARE(clamped.next().count(), 5000LL);
    QCOMPARE(clamped.next().count(), 5000LL);
  }
  void everyPresentOutputGetsAWindow() {
    FakeFactory factory;
    ManualScheduler scheduler;
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    supervisor.outputAdded(B);
    QCOMPARE(supervisor.windowCount(), std::size_t{2});
    QCOMPARE(supervisor.pendingRetries(), std::size_t{0});
    QVERIFY(supervisor.hasWindow(A) && supervisor.hasWindow(B));
    QVERIFY(scheduler.pending.empty());
  }
  void aFailedOutputRetriesWithBackoffWhileTheOthersStayLocked() {
    FakeFactory factory;
    ManualScheduler scheduler;
    factory.script[A] = {Result::Failed, Result::Failed, Result::Failed, Result::Created};
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    supervisor.outputAdded(B); // Created at once; A's failure must not touch it.
    QVERIFY(!supervisor.hasWindow(A));
    QVERIFY(supervisor.hasWindow(B));
    QVERIFY(supervisor.retryPending(A));
    QCOMPARE(scheduler.pending[A].count(), 250LL);
    supervisor.retryDue(A);
    QCOMPARE(scheduler.pending[A].count(), 500LL);
    supervisor.retryDue(A);
    QCOMPARE(scheduler.pending[A].count(), 1000LL);
    QCOMPARE(supervisor.failures(A), 3U);
    QVERIFY(supervisor.hasWindow(B));
    QVERIFY(factory.destroyed.empty()); // B kept its window throughout.
    supervisor.retryDue(A); // Fourth attempt succeeds.
    QVERIFY(supervisor.hasWindow(A));
    QVERIFY(!supervisor.retryPending(A));
    QCOMPARE(supervisor.failures(A), 0U); // Success resets the schedule.
    QCOMPARE(factory.creates[A], 4);
    QCOMPARE(factory.creates[B], 1);
    const std::vector<long long> expected{250, 500, 1000};
    QCOMPARE(scheduler.history[A], expected);
  }
  void retryNeverGivesUpAndCapsAtThirtySeconds() {
    FakeFactory factory;
    ManualScheduler scheduler;
    factory.script[A] = std::deque<Result>(40, Result::Failed);
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    for (int i = 0; i < 19; ++i) supervisor.retryDue(A);
    QCOMPARE(scheduler.history[A].size(), std::size_t{20});
    QVERIFY(supervisor.retryPending(A));
    for (std::size_t i = 8; i < scheduler.history[A].size(); ++i) QCOMPARE(scheduler.history[A][i], 30000LL);
  }
  void aClientOnlyPlaceholderIsNotSupervised() {
    FakeFactory factory;
    ManualScheduler scheduler;
    factory.script[A] = {Result::NotAnOutput};
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    QVERIFY(!supervisor.hasWindow(A));
    QVERIFY(!supervisor.retryPending(A));
    QVERIFY(scheduler.pending.empty());
    supervisor.retryDue(A); // A stale timer for something not tracked.
    QCOMPARE(factory.creates[A], 1);
  }
  void removalCancelsTheRetryAndDestroysTheWindow() {
    FakeFactory factory;
    ManualScheduler scheduler;
    factory.script[A] = {Result::Failed};
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    supervisor.outputAdded(B);
    supervisor.outputRemoved(A);
    QCOMPARE(scheduler.cancels, 1);
    QVERIFY(factory.destroyed.empty()); // A never had a window.
    supervisor.retryDue(A); // The cancelled timer may still deliver one callback.
    QCOMPARE(factory.creates[A], 1);
    supervisor.outputRemoved(B);
    QCOMPARE(factory.destroyed.size(), std::size_t{1});
    QVERIFY(factory.destroyed[0] == B);
    QCOMPARE(supervisor.windowCount(), std::size_t{0});
    supervisor.outputRemoved(C); // Unknown outputs are ignored.
    QCOMPARE(scheduler.cancels, 1);
  }
  void staleAndDuplicateNotificationsAreIgnored() {
    FakeFactory factory;
    ManualScheduler scheduler;
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    supervisor.outputAdded(A); // Duplicate screenAdded.
    supervisor.retryDue(A); // Timer for an output that already has its window.
    QCOMPARE(factory.creates[A], 1);
    factory.script[B] = {Result::Failed};
    supervisor.outputAdded(B);
    supervisor.outputAdded(B); // Duplicate while a retry is pending.
    QCOMPARE(factory.creates[B], 1);
    QCOMPARE(supervisor.pendingRetries(), std::size_t{1});
  }
  void anOutputRemovedWhileItsWindowIsBeingCreatedLeavesNothingBehind() {
    FakeFactory factory;
    ManualScheduler scheduler;
    OutputWindowSupervisor supervisor(factory, scheduler);
    factory.onCreate = [&](OutputKey output) { supervisor.outputRemoved(output); }; // Reentrant removal.
    supervisor.outputAdded(A);
    QCOMPARE(supervisor.windowCount(), std::size_t{0});
    QCOMPARE(factory.destroyed.size(), std::size_t{1}); // The window that raced the removal is destroyed.
    QVERIFY(factory.destroyed[0] == A);
    factory.script[B] = {Result::Failed};
    supervisor.outputAdded(B);
    QVERIFY(!supervisor.retryPending(B)); // A removed output is never retried.
    QVERIFY(scheduler.pending.find(B) == scheduler.pending.end());
  }
  void aReAddedOutputStartsAFreshSchedule() {
    FakeFactory factory;
    ManualScheduler scheduler;
    factory.script[A] = {Result::Failed, Result::Failed, Result::Failed};
    OutputWindowSupervisor supervisor(factory, scheduler);
    supervisor.outputAdded(A);
    supervisor.retryDue(A);
    supervisor.outputRemoved(A);
    supervisor.outputAdded(A); // Third scripted failure, but a brand new backoff.
    const std::vector<long long> expected{250, 500, 250};
    QCOMPARE(scheduler.history[A], expected);
  }
};
QTEST_GUILESS_MAIN(OutputWindowSupervisorTest)
#include "tst_output_window_supervisor.moc"
