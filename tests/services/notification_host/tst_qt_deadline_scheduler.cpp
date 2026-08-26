// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notification_host/qt_deadline_scheduler.h"

#include <QCoreApplication>
#include <QtTest>

using namespace QindaQt::Services::NotificationHost;

class QtDeadlineSchedulerTests final : public QObject {
  Q_OBJECT

private slots:
  void rejectsInvalidArmRequests();
  void rearmReplacesAndCancelSuppressesCallback();
  void destructionCancelsCallback();
};

void QtDeadlineSchedulerTests::rejectsInvalidArmRequests() {
  QtNotificationDeadlineScheduler scheduler;

  QCOMPARE(scheduler.armAfter(-1, [] {}).status,
           DeadlineArmStatus::InvalidRequest);
  QCOMPARE(scheduler.armAfter(0, {}).status, DeadlineArmStatus::InvalidRequest);
}

void QtDeadlineSchedulerTests::rearmReplacesAndCancelSuppressesCallback() {
  QtNotificationDeadlineScheduler scheduler;
  constexpr int oldDeadlineMs = 40;
  int oldCalls = 0;
  int replacementCalls = 0;
  QVERIFY(scheduler
              .armAfter(oldDeadlineMs, [&oldCalls] { ++oldCalls; })
              .ok());
  QVERIFY(
      scheduler.armAfter(0, [&replacementCalls] { ++replacementCalls; }).ok());

  QTRY_COMPARE(replacementCalls, 1);
  QCOMPARE(oldCalls, 0);
  QCoreApplication::processEvents();
  QCOMPARE(replacementCalls, 1);
  QTest::qWait(oldDeadlineMs + 20);
  QCOMPARE(oldCalls, 0);

  QVERIFY(scheduler.armAfter(0, [&oldCalls] { ++oldCalls; }).ok());
  scheduler.cancel();
  QCoreApplication::processEvents();
  QCOMPARE(oldCalls, 0);
}

void QtDeadlineSchedulerTests::destructionCancelsCallback() {
  int calls = 0;
  {
    QtNotificationDeadlineScheduler scheduler;
    QVERIFY(scheduler.armAfter(0, [&calls] { ++calls; }).ok());
  }

  QCoreApplication::processEvents();
  QCOMPARE(calls, 0);
}

QTEST_GUILESS_MAIN(QtDeadlineSchedulerTests)
#include "tst_qt_deadline_scheduler.moc"
