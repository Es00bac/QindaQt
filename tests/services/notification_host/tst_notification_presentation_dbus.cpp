// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_host/resident_notification_host.h"
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation/wire_contract.h"

#include "support/notification_host_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

using namespace QindaQt::Services;
using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::NotificationHost::TestSupport;
using namespace QindaQt::Services::NotificationPresentation;

namespace {

class RevisionReceiver final : public QObject {
    Q_OBJECT

public slots:
    void snapshotChanged(const QString &epoch, quint64 revision)
    {
        emit received(epoch, revision);
    }

signals:
    void received(const QString &epoch, quint64 revision);
};

QDBusMessage method(const QString &service, const QString &name,
                    const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(
        service, QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName), name);
    message.setArguments(arguments);
    return message;
}

QDBusMessage completedCall(const QDBusConnection &connection,
                           const QDBusMessage &message)
{
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        finished.wait(5'000);
    }
    return QDBusPendingReply<>(watcher).reply();
}

QVariantMap successfulMapCall(const QDBusConnection &connection,
                              const QDBusMessage &message,
                              QString *error)
{
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        finished.wait(5'000);
    }
    const QDBusPendingReply<QVariantMap> reply(watcher);
    if (!reply.isValid()) {
        *error = reply.error().message();
        return {};
    }
    return reply.value();
}

} // namespace

class NotificationPresentationDBusTests final : public QObject {
    Q_OBJECT

private slots:
    void authenticatesOnePresenterAndDirectsState();
};

void NotificationPresentationDBusTests::authenticatesOnePresenterAndDirectsState()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));

    const QString hostConnectionName = connectionName(QStringLiteral("presentation-host"));
    const QString presenterConnectionName = connectionName(QStringLiteral("presenter"));
    const QString intruderConnectionName = connectionName(QStringLiteral("intruder"));
    auto hostConnection = QDBusConnection::connectToBus(bus.address(), hostConnectionName);
    auto presenter = QDBusConnection::connectToBus(bus.address(), presenterConnectionName);
    auto intruder = QDBusConnection::connectToBus(bus.address(), intruderConnectionName);
    QVERIFY(hostConnection.isConnected());
    QVERIFY(presenter.isConnected());
    QVERIFY(intruder.isConnected());

    const QString tokenText(64, QLatin1Char('a'));
    const auto token = PresentationAccessToken::fromHex(tokenText, &error);
    QVERIFY2(token.has_value(), qPrintable(error));
    ManualNotificationClock clock;
    ManualDeadlineScheduler scheduler;
    RecordingNotificationBackend backend;
    ResidentNotificationHost host(hostConnection, clock, scheduler, {}, {},
                                  &backend, token);
    const QString ownedName = serviceName();
    QVERIFY2(host.start(ownedName).ok(), "host failed to start");

    const QDBusMessage unauthorized = completedCall(
        intruder, method(ownedName, QStringLiteral("GetSnapshot")));
    QCOMPARE(unauthorized.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(unauthorized.errorName(),
             QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));

    const QDBusMessage wrongToken = completedCall(
        intruder, method(ownedName, QStringLiteral("RegisterPresenter"),
                         {QString(64, QLatin1Char('b'))}));
    QCOMPARE(wrongToken.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(wrongToken.errorName(),
             QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));

    QVariantMap initial = successfulMapCall(
        presenter, method(ownedName, QStringLiteral("RegisterPresenter"),
                          {tokenText}), &error);
    QVERIFY2(!initial.isEmpty(), qPrintable(error));
    const auto initialSnapshot = PresentationSnapshotCodec::decode(initial);
    QVERIFY2(initialSnapshot.ok(), qPrintable(initialSnapshot.error));
    QCOMPARE(initialSnapshot.snapshot->revision, quint64(0));
    QVERIFY(initialSnapshot.snapshot->notifications.isEmpty());

    const QDBusMessage secondPresenter = completedCall(
        intruder, method(ownedName, QStringLiteral("RegisterPresenter"),
                         {tokenText}));
    QCOMPARE(secondPresenter.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(secondPresenter.errorName(),
             QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));

    RevisionReceiver presenterReceiver;
    RevisionReceiver intruderReceiver;
    QSignalSpy presenterSignals(&presenterReceiver, &RevisionReceiver::received);
    QSignalSpy intruderSignals(&intruderReceiver, &RevisionReceiver::received);
    QVERIFY(presenter.connect(ownedName,
                              QString::fromLatin1(WireContract::ObjectPath),
                              QString::fromLatin1(WireContract::InterfaceName),
                              QStringLiteral("SnapshotChanged"),
                              &presenterReceiver,
                              SLOT(snapshotChanged(QString,quint64))));
    QVERIFY(intruder.connect(ownedName,
                             QString::fromLatin1(WireContract::ObjectPath),
                             QString::fromLatin1(WireContract::InterfaceName),
                             QStringLiteral("SnapshotChanged"),
                             &intruderReceiver,
                             SLOT(snapshotChanged(QString,quint64))));

    auto submitted = request(QStringLiteral(":1.900"), 0);
    submitted.summary = QStringLiteral("Private presentation");
    submitted.actions = {{QStringLiteral("default"), QStringLiteral("Open")}};
    const auto submission = host.service().submit(submitted);
    QVERIFY2(submission.ok(), qPrintable(submission.message));
    QTRY_COMPARE_WITH_TIMEOUT(presenterSignals.count(), 1, 5'000);
    QTest::qWait(20);
    QCOMPARE(intruderSignals.count(), 0);
    QCOMPARE(presenterSignals.constFirst().at(0).toString(),
             initialSnapshot.snapshot->epoch);
    QCOMPARE(presenterSignals.constFirst().at(1).toULongLong(), quint64(1));

    const QVariantMap populated = successfulMapCall(
        presenter, method(ownedName, QStringLiteral("GetSnapshot")), &error);
    const auto populatedSnapshot = PresentationSnapshotCodec::decode(populated);
    QVERIFY2(populatedSnapshot.ok(), qPrintable(populatedSnapshot.error));
    QCOMPARE(populatedSnapshot.snapshot->notifications.size(), 1);
    QCOMPARE(populatedSnapshot.snapshot->notifications.constFirst().summary,
             QStringLiteral("Private presentation"));
    QCOMPARE(populatedSnapshot.snapshot->notifications.constFirst().actions.size(), 1);

    const QDBusMessage rejectedDismiss = completedCall(
        intruder, method(ownedName, QStringLiteral("Dismiss"), {submission.notificationId}));
    QCOMPARE(rejectedDismiss.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(host.service().snapshot()->notifications.size(), 1);

    const QVariantMap actionResult = successfulMapCall(
        presenter, method(ownedName, QStringLiteral("InvokeAction"),
                          {submission.notificationId, QStringLiteral("default"),
                           QStringLiteral("activation-token")}), &error);
    QVERIFY2(!actionResult.isEmpty(), qPrintable(error));
    QCOMPARE(actionResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("applied"));
    QCOMPARE(host.service().snapshot()->notifications.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(presenterSignals.count(), 2, 5'000);
    QCOMPARE(backend.actions.size(), 1);
    QCOMPARE(backend.actions.constFirst().activationToken,
             QStringLiteral("activation-token"));

    const QDBusMessage released = completedCall(
        presenter, method(ownedName, QStringLiteral("ReleasePresenter")));
    QCOMPARE(released.type(), QDBusMessage::ReplyMessage);
    const QDBusMessage afterRelease = completedCall(
        presenter, method(ownedName, QStringLiteral("GetSnapshot")));
    QCOMPARE(afterRelease.type(), QDBusMessage::ErrorMessage);

    const QVariantMap takeover = successfulMapCall(
        intruder, method(ownedName, QStringLiteral("RegisterPresenter"),
                         {tokenText}), &error);
    QVERIFY2(!takeover.isEmpty(), qPrintable(error));
    QVERIFY(PresentationSnapshotCodec::decode(takeover).ok());

    intruder = QDBusConnection(QStringLiteral("qindaqt-released-presenter"));
    QDBusConnection::disconnectFromBus(intruderConnectionName);
    const QString replacementConnectionName =
        connectionName(QStringLiteral("replacement-presenter"));
    auto replacement = QDBusConnection::connectToBus(bus.address(),
                                                       replacementConnectionName);
    QVERIFY(replacement.isConnected());
    QVariantMap replacementSnapshot;
    QTRY_VERIFY_WITH_TIMEOUT(
        !(replacementSnapshot = successfulMapCall(
              replacement,
              method(ownedName, QStringLiteral("RegisterPresenter"), {tokenText}),
              &error)).isEmpty(),
        5'000);
    QVERIFY(PresentationSnapshotCodec::decode(replacementSnapshot).ok());

    host.stop();
    QDBusConnection::disconnectFromBus(replacementConnectionName);
    QDBusConnection::disconnectFromBus(presenterConnectionName);
    QDBusConnection::disconnectFromBus(hostConnectionName);
    bus.stop();
    QVERIFY(bus.isStopped());
}

QTEST_GUILESS_MAIN(NotificationPresentationDBusTests)
#include "tst_notification_presentation_dbus.moc"
