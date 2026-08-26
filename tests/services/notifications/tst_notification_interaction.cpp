// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notifications/notification_limits.h"
#include "qindaqt/services/notifications/notification_service.h"

#include "support/notification_test_support.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Services::Notifications;
using namespace QindaQt::Services::Notifications::TestSupport;

class NotificationInteractionTests final : public QObject {
    Q_OBJECT

private slots:
    void applicationCloseChecksOwnerAndReason();
    void dismissUsesUserReason();
    void expirationSweepIsAtomicAtExactDeadline();
    void nonResidentActionInvokesThenCloses();
    void residentActionRemainsWithoutAdvancingRevision();
    void unknownAndOversizedActionsAreRejected();
    void backendReentrancyCannotMutateModel();
    void clockRegressionAndOverflowAreAtomicFailures();
    void revisionExhaustionIsAtomic();
    void everyRemovalPathReleasesSourceQuota();
    void everyRemovalPathReleasesPayloadQuota();
};

void NotificationInteractionTests::applicationCloseChecksOwnerAndReason()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    const auto created = service.submit(request(QStringLiteral(":1.30")));
    QVERIFY(created.ok());
    const auto before = service.snapshot();

    QCOMPARE(service.closeFromApplication(QStringLiteral(":1.31"), created.notificationId).status,
             OperationStatus::NotOwner);
    QCOMPARE(service.snapshot(), before);

    const auto closed = service.closeFromApplication(QStringLiteral(":1.30"),
                                                     created.notificationId);
    QVERIFY(closed.ok());
    QCOMPARE(closed.revisionAfter, quint64(2));
    QVERIFY(service.snapshot()->notifications.isEmpty());
    QCOMPARE(backend.closures.size(), 1);
    QCOMPARE(backend.closures.first().reason, CloseReason::ClosedByApplication);
    QCOMPARE(backend.closures.first().sourceService, QStringLiteral(":1.30"));
    QCOMPARE(backend.eventOrder,
             QStringList({QStringLiteral("model"),
                          QStringLiteral("model"),
                          QStringLiteral("closed")}));
}

void NotificationInteractionTests::dismissUsesUserReason()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    const auto created = service.submit(request(QStringLiteral(":1.32")));

    const auto result = service.dismiss(created.notificationId);

    QVERIFY(result.ok());
    QCOMPARE(backend.closures.last().reason, CloseReason::DismissedByUser);
    const auto missing = service.dismiss(created.notificationId);
    QCOMPARE(missing.status, OperationStatus::NotFound);
    QVERIFY(!missing.message.isEmpty());
}

void NotificationInteractionTests::expirationSweepIsAtomicAtExactDeadline()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);

    auto first = request(QStringLiteral(":1.33"));
    first.expireTimeoutMs = 10;
    auto second = request(QStringLiteral(":1.34"));
    second.expireTimeoutMs = 10;
    auto persistent = request(QStringLiteral(":1.35"));
    persistent.expireTimeoutMs = 0;
    QVERIFY(service.submit(first).ok());
    QVERIFY(service.submit(second).ok());
    QVERIFY(service.submit(persistent).ok());
    QCOMPARE(service.nextExpiryDeadlineMs(), std::optional<qint64>(10));

    clock.now = 9;
    const auto early = service.expireDue();
    QVERIFY(early.ok());
    QCOMPARE(early.revisionBefore, early.revisionAfter);
    QCOMPARE(service.snapshot()->notifications.size(), 3);

    clock.now = 10;
    const auto result = service.expireDue();
    QVERIFY(result.ok());
    QCOMPARE(result.revisionBefore, quint64(3));
    QCOMPARE(result.revisionAfter, quint64(4));
    QCOMPARE(result.affectedIds, QVector<quint32>({1, 2}));
    QCOMPARE(service.snapshot()->notifications.size(), 1);
    QCOMPARE(service.snapshot()->notifications.first().id, quint32(3));
    QVERIFY(!service.nextExpiryDeadlineMs().has_value());
    QCOMPARE(backend.closures.size(), 2);
    QCOMPARE(backend.closures.at(0).reason, CloseReason::Expired);
    QCOMPARE(backend.closures.at(1).revision, quint64(4));
}

void NotificationInteractionTests::nonResidentActionInvokesThenCloses()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto input = request(QStringLiteral(":1.36"));
    input.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    const auto created = service.submit(input);

    const auto result = service.invokeAction(created.notificationId,
                                             QStringLiteral("open"),
                                             QStringLiteral("activation-token"));

    QVERIFY(result.ok());
    QCOMPARE(result.revisionAfter, quint64(2));
    QVERIFY(service.snapshot()->notifications.isEmpty());
    QCOMPARE(backend.actions.size(), 1);
    QCOMPARE(backend.actions.first().activationToken, QStringLiteral("activation-token"));
    QCOMPARE(backend.closures.last().reason, CloseReason::DismissedByUser);
    QCOMPARE(backend.eventOrder,
             QStringList({QStringLiteral("model"),
                          QStringLiteral("action"),
                          QStringLiteral("model"),
                          QStringLiteral("closed")}));
}

void NotificationInteractionTests::residentActionRemainsWithoutAdvancingRevision()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto input = request(QStringLiteral(":1.37"));
    input.actions = {{QStringLiteral("reply"), QStringLiteral("Reply")}};
    input.hints.resident = true;
    const auto created = service.submit(input);

    const auto result = service.invokeAction(created.notificationId,
                                             QStringLiteral("reply"));

    QVERIFY(result.ok());
    QCOMPARE(result.revisionBefore, quint64(1));
    QCOMPARE(result.revisionAfter, quint64(1));
    QCOMPARE(service.snapshot()->notifications.size(), 1);
    QCOMPARE(backend.actions.size(), 1);
    QVERIFY(backend.closures.isEmpty());
    QCOMPARE(backend.publications.size(), 1);
}

void NotificationInteractionTests::unknownAndOversizedActionsAreRejected()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto input = request(QStringLiteral(":1.38"));
    input.actions = {{QStringLiteral("known"), QStringLiteral("Known")}};
    const auto created = service.submit(input);
    const auto before = service.snapshot();

    QCOMPARE(service.invokeAction(created.notificationId, QStringLiteral("unknown")).status,
             OperationStatus::UnknownAction);
    QCOMPARE(service.invokeAction(
                 created.notificationId,
                 QStringLiteral("known"),
                 QString(NotificationLimits::MaximumActivationTokenBytes + 1,
                         QLatin1Char('x')))
                 .status,
             OperationStatus::InvalidRequest);
    QCOMPARE(service.snapshot(), before);
    QVERIFY(backend.actions.isEmpty());
}

void NotificationInteractionTests::backendReentrancyCannotMutateModel()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    OperationStatus nestedStatus = OperationStatus::Applied;
    backend.onPublication = [&service, &nestedStatus]() {
        nestedStatus = service.submit(request(QStringLiteral(":1.40"))).status;
    };

    const auto result = service.submit(request(QStringLiteral(":1.39")));

    QVERIFY(result.ok());
    QCOMPARE(nestedStatus, OperationStatus::ReentrantOperation);
    QCOMPARE(service.snapshot()->notifications.size(), 1);
    QCOMPARE(service.snapshot()->revision, quint64(1));
}

void NotificationInteractionTests::clockRegressionAndOverflowAreAtomicFailures()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    clock.now = 10;
    QVERIFY(service.submit(request(QStringLiteral(":1.41"))).ok());
    const auto retained = service.snapshot();

    clock.now = 9;
    QCOMPARE(service.submit(request(QStringLiteral(":1.42"))).status,
             OperationStatus::ClockFailure);
    QCOMPARE(service.snapshot(), retained);

    ManualNotificationClock overflowClock;
    overflowClock.now = std::numeric_limits<qint64>::max();
    RecordingNotificationBackend overflowBackend;
    NotificationService overflowService(overflowClock, overflowBackend);
    auto expiring = request(QStringLiteral(":1.43"));
    expiring.expireTimeoutMs = 1;
    QCOMPARE(overflowService.submit(expiring).status, OperationStatus::ClockFailure);
    QCOMPARE(overflowService.snapshot()->revision, quint64(0));
    QVERIFY(overflowBackend.publications.isEmpty());
}

void NotificationInteractionTests::revisionExhaustionIsAtomic()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(
        clock,
        backend,
        {},
        NotificationRevisionSeed{std::numeric_limits<quint64>::max() - 1});
    QCOMPARE(service.snapshot()->revision,
             std::numeric_limits<quint64>::max() - 1);

    auto persistent = request(QStringLiteral(":1.44"));
    persistent.expireTimeoutMs = 0;
    const auto admitted = service.submit(persistent);
    QVERIFY(admitted.ok());
    QCOMPARE(admitted.revisionAfter, std::numeric_limits<quint64>::max());
    const auto retained = service.snapshot();

    const auto dismissed = service.dismiss(admitted.notificationId);
    QCOMPARE(dismissed.status, OperationStatus::RevisionExhausted);
    QCOMPARE(dismissed.revisionBefore, std::numeric_limits<quint64>::max());
    QCOMPARE(dismissed.revisionAfter, dismissed.revisionBefore);
    QCOMPARE(service.snapshot(), retained);
    QCOMPARE(backend.publications.size(), 1);
    QVERIFY(backend.closures.isEmpty());

    auto replacement = persistent;
    replacement.replacesId = admitted.notificationId;
    QCOMPARE(service.submit(replacement).status,
             OperationStatus::RevisionExhausted);
    QCOMPARE(service.snapshot(), retained);
}

void NotificationInteractionTests::everyRemovalPathReleasesSourceQuota()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 2;
    policy.maximumActiveNotificationsPerSource = 1;
    NotificationService service(clock, backend, policy);
    const QString source = QStringLiteral(":1.45");

    auto persistent = request(source);
    persistent.expireTimeoutMs = 0;
    const auto applicationClosed = service.submit(persistent);
    QVERIFY(applicationClosed.ok());
    QCOMPARE(service.submit(persistent).status, OperationStatus::CapacityReached);
    QVERIFY(service.closeFromApplication(source, applicationClosed.notificationId).ok());

    auto actionable = persistent;
    actionable.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    const auto actionClosed = service.submit(actionable);
    QVERIFY(actionClosed.ok());
    QVERIFY(service.invokeAction(actionClosed.notificationId,
                                 QStringLiteral("open")).ok());

    auto expiring = persistent;
    expiring.expireTimeoutMs = 5;
    const auto expired = service.submit(expiring);
    QVERIFY(expired.ok());
    clock.now = 5;
    const auto expiration = service.expireDue();
    QVERIFY(expiration.ok());
    QCOMPARE(expiration.affectedIds, QVector<quint32>({expired.notificationId}));

    const auto afterEveryRelease = service.submit(persistent);
    QVERIFY(afterEveryRelease.ok());
    QCOMPARE(service.snapshot()->notifications.size(), 1);
}

void NotificationInteractionTests::everyRemovalPathReleasesPayloadQuota()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 8;
    policy.maximumActiveNotificationsPerSource = 8;
    policy.maximumRetainedPayloadBytes = 50;
    policy.maximumRetainedPayloadBytesPerSource = 50;
    NotificationService service(clock, backend, policy);
    const QString source = QStringLiteral(":1.46");

    auto payload = request(source);
    payload.body = QString(20, QLatin1Char('x'));
    payload.expireTimeoutMs = 0;
    const auto payloadBudgetIsFull = [&service, &payload]() {
        QCOMPARE(service.submit(payload).status, OperationStatus::CapacityReached);
    };

    const auto applicationClosed = service.submit(payload);
    QVERIFY(applicationClosed.ok());
    payloadBudgetIsFull();
    QVERIFY(service.closeFromApplication(source, applicationClosed.notificationId).ok());

    auto actionable = payload;
    actionable.actions = {{QStringLiteral("go"), QStringLiteral("Go")}};
    const auto actionClosed = service.submit(actionable);
    QVERIFY(actionClosed.ok());
    payloadBudgetIsFull();
    QVERIFY(service.invokeAction(actionClosed.notificationId,
                                 QStringLiteral("go")).ok());

    auto expiring = payload;
    expiring.expireTimeoutMs = 5;
    const auto expired = service.submit(expiring);
    QVERIFY(expired.ok());
    payloadBudgetIsFull();
    clock.now = 5;
    const auto expiration = service.expireDue();
    QVERIFY(expiration.ok());
    QCOMPARE(expiration.affectedIds, QVector<quint32>({expired.notificationId}));

    auto small = request(source, QStringLiteral("S"));
    small.body.clear();
    small.expireTimeoutMs = 0;
    const auto replaced = service.submit(small);
    QVERIFY(replaced.ok());
    auto largerReplacement = payload;
    largerReplacement.replacesId = replaced.notificationId;
    QVERIFY(service.submit(largerReplacement).ok());
    payloadBudgetIsFull();
    QVERIFY(service.dismiss(replaced.notificationId).ok());

    QVERIFY(service.submit(payload).ok());
    QCOMPARE(service.snapshot()->notifications.size(), 1);
}

QTEST_GUILESS_MAIN(NotificationInteractionTests)
#include "tst_notification_interaction.moc"
