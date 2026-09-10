// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/reminder_scheduler.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

using namespace QindaQt::Apps::Calendar;

namespace {

// Test double for the ReminderTimer seam: records the arm and fires only
// when the test asks, so scheduling is asserted without real time passing.
class ManualReminderTimer final : public ReminderTimer {
public:
  void armAfter(qint64 delayMs, Callback callback) override {
    m_armed = true;
    m_delayMs = delayMs;
    m_callback = std::move(callback);
  }

  void cancel() noexcept override {
    m_armed = false;
    m_callback = nullptr;
  }

  [[nodiscard]] bool isArmed() const { return m_armed; }
  [[nodiscard]] qint64 armedDelayMs() const { return m_delayMs; }

  void fire() {
    m_armed = false;
    Callback callback = std::move(m_callback);
    m_callback = nullptr;
    if (callback) {
      callback();
    }
  }

private:
  bool m_armed = false;
  qint64 m_delayMs = -1;
  Callback m_callback;
};

[[nodiscard]] ReminderRequest makeRequest(const QString &uid,
                                          const QDateTime &occurrenceStart,
                                          const QDateTime &fireAt) {
  return {.eventUid = uid,
          .summary = uid + QStringLiteral(" summary"),
          .occurrenceStart = occurrenceStart,
          .fireAt = fireAt};
}

} // namespace

class TestReminderScheduler final : public QObject {
  Q_OBJECT

private slots:
  void schedulesAtFireTimeAndFiresOnce();
  void pastRemindersNeverFire();
  void identicalRequestsAreDeduplicated();
  void rescheduleReplacesThePendingSet();
  void malformedRequestsAreDropped();
};

void TestReminderScheduler::schedulesAtFireTimeAndFiresOnce() {
  QDateTime now(QDate(2026, 9, 7), QTime(8, 0), QTimeZone::UTC);
  auto *timer = new ManualReminderTimer;
  ReminderScheduler scheduler([&now] { return now; },
                              std::unique_ptr<ReminderTimer>(timer));
  QSignalSpy spy(&scheduler, &ReminderScheduler::reminderDue);

  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), QTimeZone::UTC);
  const ReminderRequest request =
      makeRequest(QStringLiteral("uid-1"), start, start.addSecs(-15 * 60));
  scheduler.reschedule({request});

  QVERIFY(timer->isArmed());
  QCOMPARE(timer->armedDelayMs(), qint64(45 * 60 * 1000));
  QCOMPARE(scheduler.pendingRequests(), QList<ReminderRequest>{request});

  // A timer fire before the clock reaches the deadline re-arms silently.
  timer->fire();
  QCOMPARE(spy.size(), 0);
  QVERIFY(timer->isArmed());
  QCOMPARE(scheduler.pendingRequests().size(), 1);

  now = start.addSecs(-15 * 60);
  timer->fire();
  QCOMPARE(spy.size(), 1);
  const QList<QVariant> delivered = spy.constFirst();
  QCOMPARE(delivered.at(0).toString(), QStringLiteral("uid-1"));
  QCOMPARE(delivered.at(1).toString(), QStringLiteral("uid-1 summary"));
  QCOMPARE(delivered.at(2).toDateTime(), start);
  QVERIFY(scheduler.pendingRequests().isEmpty());
  QVERIFY(!timer->isArmed());

  now = now.addSecs(60);
  timer->fire();
  QCOMPARE(spy.size(), 1);
}

void TestReminderScheduler::pastRemindersNeverFire() {
  QDateTime now(QDate(2026, 9, 7), QTime(8, 0), QTimeZone::UTC);
  auto *timer = new ManualReminderTimer;
  ReminderScheduler scheduler([&now] { return now; },
                              std::unique_ptr<ReminderTimer>(timer));
  QSignalSpy spy(&scheduler, &ReminderScheduler::reminderDue);

  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), QTimeZone::UTC);
  scheduler.reschedule(
      {makeRequest(QStringLiteral("past"), start, now.addSecs(-60)),
       makeRequest(QStringLiteral("exactly-now"), start, now)});

  QVERIFY2(scheduler.pendingRequests().isEmpty(),
           "past and already-due reminders are dropped, never replayed");
  QVERIFY(!timer->isArmed());
  QCOMPARE(spy.size(), 0);
}

void TestReminderScheduler::identicalRequestsAreDeduplicated() {
  QDateTime now(QDate(2026, 9, 7), QTime(8, 0), QTimeZone::UTC);
  auto *timer = new ManualReminderTimer;
  ReminderScheduler scheduler([&now] { return now; },
                              std::unique_ptr<ReminderTimer>(timer));
  QSignalSpy spy(&scheduler, &ReminderScheduler::reminderDue);

  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), QTimeZone::UTC);
  const ReminderRequest request =
      makeRequest(QStringLiteral("uid-1"), start, start.addSecs(-900));
  scheduler.reschedule({request, request});
  QCOMPARE(scheduler.pendingRequests().size(), 1);

  now = start.addSecs(-900);
  timer->fire();
  QCOMPARE(spy.size(), 1);
}

void TestReminderScheduler::rescheduleReplacesThePendingSet() {
  QDateTime now(QDate(2026, 9, 7), QTime(8, 0), QTimeZone::UTC);
  auto *timer = new ManualReminderTimer;
  ReminderScheduler scheduler([&now] { return now; },
                              std::unique_ptr<ReminderTimer>(timer));
  QSignalSpy spy(&scheduler, &ReminderScheduler::reminderDue);

  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), QTimeZone::UTC);
  const ReminderRequest first =
      makeRequest(QStringLiteral("first"), start, now.addSecs(600));
  const ReminderRequest second =
      makeRequest(QStringLiteral("second"), start, now.addSecs(1200));
  scheduler.reschedule({first});
  QCOMPARE(timer->armedDelayMs(), qint64(600 * 1000));

  scheduler.reschedule({second});
  QCOMPARE(scheduler.pendingRequests(), QList<ReminderRequest>{second});
  QCOMPARE(timer->armedDelayMs(), qint64(1200 * 1000));

  now = now.addSecs(1200);
  timer->fire();
  QCOMPARE(spy.size(), 1);
  QCOMPARE(spy.constFirst().at(0).toString(), QStringLiteral("second"));
}

void TestReminderScheduler::malformedRequestsAreDropped() {
  QDateTime now(QDate(2026, 9, 7), QTime(8, 0), QTimeZone::UTC);
  auto *timer = new ManualReminderTimer;
  ReminderScheduler scheduler([&now] { return now; },
                              std::unique_ptr<ReminderTimer>(timer));

  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), QTimeZone::UTC);
  scheduler.reschedule(
      {makeRequest(QString(), start, now.addSecs(60)),
       makeRequest(QStringLiteral("no-fire"), start, QDateTime()),
       makeRequest(QStringLiteral("no-start"), QDateTime(), now.addSecs(60))});
  QVERIFY(scheduler.pendingRequests().isEmpty());
  QVERIFY(!timer->isArmed());
}

QTEST_GUILESS_MAIN(TestReminderScheduler)
#include "tst_reminder_scheduler.moc"
