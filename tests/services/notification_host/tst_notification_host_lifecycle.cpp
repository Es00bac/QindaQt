// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notification_host/resident_notification_host.h"
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notification_presentation/wire_contract.h"

#include "support/notification_host_test_support.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QStandardPaths>
#include <QtTest>

using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::NotificationHost::TestSupport;
namespace NotificationPresentation =
    QindaQt::Services::NotificationPresentation;

namespace {

constexpr auto NotificationObjectPath = "/org/freedesktop/Notifications";

bool isServiceRegistered(const QDBusConnection &connection,
                         const QString &name) {
  if (connection.interface() == nullptr) {
    return false;
  }
  const QDBusReply<bool> reply =
      connection.interface()->isServiceRegistered(name);
  return reply.isValid() && reply.value();
}

void verifyServerAnswers(const QDBusConnection &connection,
                         const QString &name) {
  auto call = QDBusMessage::createMethodCall(
      name, QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("GetServerInformation"));
  QDBusPendingCallWatcher watcher(connection.asyncCall(call, 5'000));
  QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 5'000);
  const QDBusPendingReply<QString, QString, QString, QString> reply(watcher);
  QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
}

} // namespace

class NotificationHostLifecycleTests final : public QObject {
  Q_OBJECT

private slots:
  void ownershipConflictAndShutdownReleaseName();
  void startupSchedulerFailureRollsBackName();
  void presentationRegistrationFailureRollsBackName();
  void malformedServiceNameReturnsTypedFailure();
  void invalidPolicyReturnsTypedFailure();
  void disconnectedBusReturnsTypedFailure();
};

void NotificationHostLifecycleTests::ownershipConflictAndShutdownReleaseName() {
  if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
    QSKIP("dbus-daemon is unavailable");
  }
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  const QString firstName = connectionName(QStringLiteral("owner"));
  const QString secondName = connectionName(QStringLiteral("contender"));
  const QString probeName = connectionName(QStringLiteral("probe"));
  auto firstConnection =
      QDBusConnection::connectToBus(bus.address(), firstName);
  auto secondConnection =
      QDBusConnection::connectToBus(bus.address(), secondName);
  auto probe = QDBusConnection::connectToBus(bus.address(), probeName);
  QVERIFY(firstConnection.isConnected());
  QVERIFY(secondConnection.isConnected());
  QVERIFY(probe.isConnected());

  ManualNotificationClock firstClock;
  ManualNotificationClock secondClock;
  ManualDeadlineScheduler firstScheduler;
  ManualDeadlineScheduler secondScheduler;
  const QString ownedName = serviceName();
  {
    ResidentNotificationHost owner(firstConnection, firstClock, firstScheduler);
    ResidentNotificationHost contender(secondConnection, secondClock,
                                       secondScheduler);
    QVERIFY(owner.start(ownedName).ok());
    QVERIFY(isServiceRegistered(probe, ownedName));
    verifyServerAnswers(probe, ownedName);

    QObject privateEndpointSentinel;
    QVERIFY(firstConnection.registerObject(
        QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath),
        &privateEndpointSentinel, QDBusConnection::ExportAllSlots));
    firstConnection.unregisterObject(
        QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath));

    const auto conflict = contender.start(ownedName);
    QCOMPARE(conflict.status,
             NotificationHostStartStatus::NameOwnershipConflict);
    QVERIFY(!conflict.message.isEmpty());
    QVERIFY(!contender.isRunning());
    verifyServerAnswers(probe, ownedName);

    QObject sentinel;
    QVERIFY(secondConnection.registerObject(
        QStringLiteral("/org/freedesktop/Notifications"), &sentinel,
        QDBusConnection::ExportAllSlots));
    secondConnection.unregisterObject(
        QStringLiteral("/org/freedesktop/Notifications"));

    owner.stop();
    QTRY_VERIFY(!isServiceRegistered(probe, ownedName));
    QVERIFY(contender.start(ownedName).ok());
    QVERIFY(isServiceRegistered(probe, ownedName));
  }
  QTRY_VERIFY(!isServiceRegistered(probe, ownedName));
  QObject sentinel;
  QVERIFY(secondConnection.registerObject(
      QStringLiteral("/org/freedesktop/Notifications"), &sentinel,
      QDBusConnection::ExportAllSlots));
  secondConnection.unregisterObject(
      QStringLiteral("/org/freedesktop/Notifications"));

  QDBusConnection::disconnectFromBus(probeName);
  QDBusConnection::disconnectFromBus(secondName);
  QDBusConnection::disconnectFromBus(firstName);
  bus.stop();
  QVERIFY(bus.isStopped());
}

void NotificationHostLifecycleTests::invalidPolicyReturnsTypedFailure() {
  const QString invalidName = connectionName(QStringLiteral("invalid-policy"));
  auto connection = QDBusConnection::connectToBus(
      QStringLiteral("unix:path=/tmp/qindaqt-notification-host-no-such-bus"),
      invalidName);
  ManualNotificationClock clock;
  ManualDeadlineScheduler scheduler;
  QindaQt::Services::Notifications::NotificationPolicy policy;
  policy.maximumActiveNotifications = 0;
  ResidentNotificationHost host(connection, clock, scheduler, policy);

  const auto result = host.start(serviceName());

  QCOMPARE(result.status, NotificationHostStartStatus::InvalidPolicy);
  QVERIFY(!result.message.isEmpty());
  QDBusConnection::disconnectFromBus(invalidName);
}

void NotificationHostLifecycleTests::startupSchedulerFailureRollsBackName() {
  if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
    QSKIP("dbus-daemon is unavailable");
  }
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  const QString hostName = connectionName(QStringLiteral("rollback"));
  const QString probeName = connectionName(QStringLiteral("rollback-probe"));
  auto connection = QDBusConnection::connectToBus(bus.address(), hostName);
  auto probe = QDBusConnection::connectToBus(bus.address(), probeName);
  QVERIFY(connection.isConnected());
  QVERIFY(probe.isConnected());

  ManualNotificationClock clock;
  ManualDeadlineScheduler scheduler;
  ResidentNotificationHost host(connection, clock, scheduler);
  QVERIFY(host.service().submit(request(QStringLiteral(":1.90"), 10)).ok());
  scheduler.rejectNextArm = true;
  const QString rejectedName = serviceName();

  const auto result = host.start(rejectedName);

  QCOMPARE(result.status,
           NotificationHostStartStatus::DeadlineSchedulingFailed);
  QVERIFY(!result.message.isEmpty());
  QVERIFY(!host.isRunning());
  QVERIFY(!isServiceRegistered(probe, rejectedName));
  QVERIFY(!scheduler.isArmed());

  QObject sentinel;
  QVERIFY(connection.registerObject(
      QString::fromLatin1(NotificationObjectPath), &sentinel,
      QDBusConnection::ExportAllSlots));
  connection.unregisterObject(QString::fromLatin1(NotificationObjectPath));

  QVERIFY(host.start(rejectedName).ok());
  QVERIFY(isServiceRegistered(probe, rejectedName));
  host.stop();
  QTRY_VERIFY(!isServiceRegistered(probe, rejectedName));

  QDBusConnection::disconnectFromBus(probeName);
  QDBusConnection::disconnectFromBus(hostName);
  bus.stop();
  QVERIFY(bus.isStopped());
}

void NotificationHostLifecycleTests::presentationRegistrationFailureRollsBackName() {
  if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
    QSKIP("dbus-daemon is unavailable");
  }
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  const QString hostName = connectionName(QStringLiteral("presentation-rollback"));
  const QString probeName = connectionName(QStringLiteral("presentation-probe"));
  auto connection = QDBusConnection::connectToBus(bus.address(), hostName);
  auto probe = QDBusConnection::connectToBus(bus.address(), probeName);
  QVERIFY(connection.isConnected());
  QVERIFY(probe.isConnected());

  QObject collision;
  QVERIFY(connection.registerObject(
      QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath),
      &collision, QDBusConnection::ExportAllSlots));
  const auto token = NotificationPresentation::PresentationAccessToken::fromHex(
      QString(64, QLatin1Char('a')), &error);
  QVERIFY2(token.has_value(), qPrintable(error));
  ManualNotificationClock clock;
  ManualDeadlineScheduler scheduler;
  ResidentNotificationHost host(connection, clock, scheduler, {}, {}, nullptr,
                                token);
  const QString rejectedName = serviceName();

  const auto rejected = host.start(rejectedName);

  QCOMPARE(rejected.status,
           NotificationHostStartStatus::PresentationRegistrationFailed);
  QVERIFY(!rejected.message.isEmpty());
  QVERIFY(!host.isRunning());
  QVERIFY(!isServiceRegistered(probe, rejectedName));

  connection.unregisterObject(
      QString::fromLatin1(NotificationPresentation::WireContract::ObjectPath));
  QVERIFY(host.start(rejectedName).ok());
  QVERIFY(isServiceRegistered(probe, rejectedName));
  host.stop();
  QTRY_VERIFY(!isServiceRegistered(probe, rejectedName));

  QDBusConnection::disconnectFromBus(probeName);
  QDBusConnection::disconnectFromBus(hostName);
  bus.stop();
  QVERIFY(bus.isStopped());
}

void NotificationHostLifecycleTests::malformedServiceNameReturnsTypedFailure() {
  const QString connectionId =
      connectionName(QStringLiteral("invalid-service-name"));
  auto connection = QDBusConnection::connectToBus(
      QStringLiteral("unix:path=/tmp/qindaqt-notification-host-no-such-bus"),
      connectionId);
  ManualNotificationClock clock;
  ManualDeadlineScheduler scheduler;
  ResidentNotificationHost host(connection, clock, scheduler);
  const QStringList malformedNames = {
      QString{},
      QStringLiteral("org"),
      QStringLiteral("org..qindaqt"),
      QStringLiteral("1org.qindaqt"),
      QStringLiteral("org.2qindaqt"),
      QStringLiteral(":1.42"),
      QStringLiteral("org/qindaqt.Notifications"),
      QString(256, QLatin1Char('a')) + QStringLiteral(".name"),
  };

  for (const auto &name : malformedNames) {
    const auto result = host.start(name);
    QCOMPARE(result.status, NotificationHostStartStatus::InvalidServiceName);
    QVERIFY(!result.message.isEmpty());
    QVERIFY(!host.isRunning());
  }
  QDBusConnection::disconnectFromBus(connectionId);
}

void NotificationHostLifecycleTests::disconnectedBusReturnsTypedFailure() {
  const QString invalidName = connectionName(QStringLiteral("disconnected"));
  auto connection = QDBusConnection::connectToBus(
      QStringLiteral("unix:path=/tmp/qindaqt-notification-host-no-such-bus"),
      invalidName);
  QVERIFY(!connection.isConnected());
  ManualNotificationClock clock;
  ManualDeadlineScheduler scheduler;
  ResidentNotificationHost host(connection, clock, scheduler);

  const auto result = host.start(serviceName());

  QCOMPARE(result.status, NotificationHostStartStatus::BusUnavailable);
  QVERIFY(!result.ok());
  QVERIFY(!result.message.isEmpty());
  QDBusConnection::disconnectFromBus(invalidName);
}

QTEST_GUILESS_MAIN(NotificationHostLifecycleTests)
#include "tst_notification_host_lifecycle.moc"
