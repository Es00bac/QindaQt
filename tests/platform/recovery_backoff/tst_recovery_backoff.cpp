// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/platform/recovery_backoff/recovery_backoff.h"
#include <QSignalSpy>
#include <QtTest>
using namespace QindaQt::Platform::Recovery;
using std::chrono::milliseconds;
namespace {
struct ManualTimers {
  QList<ManualRecoveryTimer *> created;
  RecoveryTimerFactory factory() {
    return [this] {
      auto timer = std::make_unique<ManualRecoveryTimer>();
      created.append(timer.get());
      return timer;
    };
  }
};
} // namespace
class RecoveryBackoffTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void defaultPolicyDoublesFromQuarterSecondToThirtySeconds() {
    const BackoffPolicy policy;
    QList<milliseconds> delays;
    for (int attempt = 0; attempt < 10; ++attempt)
      delays.append(policy.delayFor(attempt));
    QCOMPARE(delays, (QList<milliseconds>{milliseconds{250}, milliseconds{500},
                                          milliseconds{1000}, milliseconds{2000},
                                          milliseconds{4000}, milliseconds{8000},
                                          milliseconds{16000}, milliseconds{30000},
                                          milliseconds{30000}, milliseconds{30000}}));
    QCOMPARE(policy.delayFor(1'000'000), milliseconds{30000});
  }
  void degeneratePolicyNeverProducesATightLoop() {
    const BackoffPolicy zero{milliseconds{0}, milliseconds{0}};
    QCOMPARE(zero.delayFor(0), milliseconds{1});
    QCOMPARE(zero.delayFor(40), milliseconds{1});
    const BackoffPolicy inverted{milliseconds{500}, milliseconds{100}};
    QCOMPARE(inverted.delayFor(3), milliseconds{500});
  }
  void scheduleArmsOnceAndGrowsUntilReset() {
    ManualTimers timers;
    RecoveryBackoff backoff({}, timers.factory());
    QCOMPARE(timers.created.size(), 1);
    auto &timer = *timers.created.first();
    QSignalSpy retry(&backoff, &RecoveryBackoff::retry);
    QCOMPARE(backoff.schedule(), milliseconds{250});
    // A burst of failures while one retry is armed does not stack retries or
    // skip ahead in the schedule.
    QCOMPARE(backoff.schedule(), milliseconds{250});
    QCOMPARE(backoff.schedule(), milliseconds{250});
    QCOMPARE(timer.requested().size(), 1);
    QVERIFY(timer.fire());
    QCOMPARE(retry.size(), 1);
    QVERIFY(!timer.fire());
    QCOMPARE(backoff.schedule(), milliseconds{500});
    QVERIFY(timer.fire());
    QCOMPARE(backoff.schedule(), milliseconds{1000});
    QCOMPARE(backoff.attempts(), 3);
    backoff.reset();
    QVERIFY(!backoff.pending());
    QVERIFY(!timer.fire());
    QCOMPARE(retry.size(), 2);
    QCOMPARE(backoff.schedule(), milliseconds{250});
  }
  void cancelKeepsTheGrowthButStopsTheShot() {
    ManualTimers timers;
    RecoveryBackoff backoff({}, timers.factory());
    QSignalSpy retry(&backoff, &RecoveryBackoff::retry);
    backoff.schedule();
    backoff.cancel();
    QVERIFY(!timers.created.first()->fire());
    QCOMPARE(retry.size(), 0);
    QCOMPARE(backoff.schedule(), milliseconds{500});
  }
  void retryMayRescheduleReentrantly() {
    ManualTimers timers;
    RecoveryBackoff backoff({milliseconds{10}, milliseconds{40}},
                            timers.factory());
    QList<milliseconds> armed;
    connect(&backoff, &RecoveryBackoff::retry, this,
            [&] { armed.append(backoff.schedule()); });
    backoff.schedule();
    for (int i = 0; i < 4; ++i)
      QVERIFY(timers.created.first()->fire());
    QCOMPARE(armed, (QList<milliseconds>{milliseconds{20}, milliseconds{40},
                                         milliseconds{40}, milliseconds{40}}));
  }
  void productionTimerElapsesThroughTheEventLoop() {
    RecoveryBackoff backoff({milliseconds{1}, milliseconds{1}});
    QSignalSpy retry(&backoff, &RecoveryBackoff::retry);
    backoff.schedule();
    QVERIFY(backoff.pending());
    QVERIFY(retry.wait(2000));
    QVERIFY(!backoff.pending());
  }
};
QTEST_GUILESS_MAIN(RecoveryBackoffTests)
#include "tst_recovery_backoff.moc"
