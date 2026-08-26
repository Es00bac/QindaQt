// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notification_host/resident_notification_host.h"

#include "support/notification_host_test_support.h"

#include <QDBusConnection>
#include <QStandardPaths>
#include <QtTest>

using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::NotificationHost::TestSupport;

class NotificationDeadlineTests final : public QObject {
  Q_OBJECT

private slots:
  void expiryRearmsToEarliestDeadlineAndCancels();
  void runtimeFailuresDisarmAndPublishTypedState();
};

void NotificationDeadlineTests::expiryRearmsToEarliestDeadlineAndCancels() {
  if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
    QSKIP("dbus-daemon is unavailable");
  }
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  const QString hostConnectionName = connectionName(QStringLiteral("deadline"));
  auto connection =
      QDBusConnection::connectToBus(bus.address(), hostConnectionName);
  QVERIFY(connection.isConnected());

  ManualNotificationClock clock;
  clock.now = 100;
  ManualDeadlineScheduler scheduler;
  RecordingNotificationBackend presentation;
  ResidentNotificationHost host(connection, clock, scheduler, {}, {},
                                &presentation);
  QVERIFY(host.start(serviceName()).ok());

  const auto later =
      host.service().submit(request(QStringLiteral(":1.80"), 20));
  QVERIFY(later.ok());
  QCOMPARE(scheduler.armDelays, QVector<qint64>({20}));
  const auto earlier =
      host.service().submit(request(QStringLiteral(":1.81"), 5));
  QVERIFY(earlier.ok());
  QCOMPARE(scheduler.armDelays, QVector<qint64>({20, 5}));
  QVERIFY(scheduler.isArmed());

  clock.now = 105;
  QVERIFY(scheduler.fire());
  QCOMPARE(host.service().snapshot()->notifications.size(), 1);
  QCOMPARE(host.service().snapshot()->notifications.first().id,
           later.notificationId);
  QCOMPARE(scheduler.armDelays, QVector<qint64>({20, 5, 15}));
  QCOMPARE(presentation.closures.last().reason,
           QindaQt::Services::Notifications::CloseReason::Expired);

  clock.now = 110;
  QVERIFY(scheduler.fire());
  QCOMPARE(host.service().snapshot()->notifications.size(), 1);
  QCOMPARE(scheduler.armDelays, QVector<qint64>({20, 5, 15, 10}));
  QVERIFY(host.runtimeState().healthy());

  QVERIFY(host.service().dismiss(later.notificationId).ok());
  QVERIFY(!scheduler.isArmed());
  QCOMPARE(scheduler.cancelCount, 1);

  const auto persistent =
      host.service().submit(request(QStringLiteral(":1.82"), 0));
  QVERIFY(persistent.ok());
  QCOMPARE(scheduler.armDelays.size(), 4);
  host.stop();
  QCOMPARE(scheduler.cancelCount, 2);
  QVERIFY(!scheduler.fire());

  QDBusConnection::disconnectFromBus(hostConnectionName);
  bus.stop();
  QVERIFY(bus.isStopped());
}

void NotificationDeadlineTests::runtimeFailuresDisarmAndPublishTypedState() {
  if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
    QSKIP("dbus-daemon is unavailable");
  }
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  const QString hostConnectionName = connectionName(QStringLiteral("runtime-failure"));
  auto connection =
      QDBusConnection::connectToBus(bus.address(), hostConnectionName);
  QVERIFY(connection.isConnected());

  ManualNotificationClock clock;
  clock.now = 100;
  ManualDeadlineScheduler scheduler;
  ResidentNotificationHost host(connection, clock, scheduler);
  QVERIFY(host.start(serviceName()).ok());

  scheduler.rejectNextArm = true;
  const auto admittedAfterRejectedArm =
      host.service().submit(request(QStringLiteral(":1.83"), 10));
  QVERIFY(admittedAfterRejectedArm.ok());
  QCOMPARE(host.runtimeState().status,
           NotificationHostRuntimeStatus::DeadlineSchedulingFailed);
  QVERIFY(!host.runtimeState().message.isEmpty());
  QVERIFY(!scheduler.isArmed());

  // A later publication retries the earliest absolute deadline and may
  // restore healthy operation without restarting the D-Bus owner.
  const auto retry =
      host.service().submit(request(QStringLiteral(":1.84"), 20));
  QVERIFY(retry.ok());
  QVERIFY(host.runtimeState().healthy());
  QVERIFY(scheduler.isArmed());

  clock.now = 99;
  QVERIFY(scheduler.fire());
  QCOMPARE(host.runtimeState().status,
           NotificationHostRuntimeStatus::ExpirationFailed);
  QVERIFY(!host.runtimeState().message.isEmpty());
  QVERIFY(!scheduler.isArmed());
  QVERIFY(host.isRunning());

  host.stop();
  QDBusConnection::disconnectFromBus(hostConnectionName);
  bus.stop();
  QVERIFY(bus.isStopped());
}

QTEST_GUILESS_MAIN(NotificationDeadlineTests)
#include "tst_notification_deadlines.moc"
