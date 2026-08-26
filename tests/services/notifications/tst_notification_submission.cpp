// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notifications/notification_limits.h"
#include "qindaqt/services/notifications/notification_service.h"

#include "support/notification_test_support.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Services::Notifications;
using namespace QindaQt::Services::Notifications::TestSupport;

class NotificationSubmissionTests final : public QObject {
    Q_OBJECT

private slots:
    void publishesBoundedTypedNotification();
    void replacementPreservesIdentityAndOldSnapshot();
    void replacementRequiresAuthenticatedOwner();
    void missingReplacementIdIsReusedAsProtocolIdentity();
    void invalidatedExplicitIdDoesNotAdvanceGeneratedSequence();
    void generatedIdsSkipActiveExplicitIdentities();
    void sparseExplicitHistoryCannotDenyFutureIdentities();
    void malformedPayloadsAreAtomicallyRejected();
    void capacityIsBoundedButReplacementRemainsAvailable();
    void perSourcePersistentQuotaPreservesCapacityForPeers();
    void perSourcePayloadQuotaIsAtomic();
    void aggregatePayloadBudgetIsAtomic();
    void invalidPolicyNeverBecomesReady();
    void invalidPerSourcePoliciesNeverBecomeReady();
    void timeoutPolicyIsDeterministicAndBounded();
};

void NotificationSubmissionTests::publishesBoundedTypedNotification()
{
    ManualNotificationClock clock;
    clock.now = 50;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    const auto initial = service.snapshot();

    auto input = request(QStringLiteral(":1.10"));
    input.actions = {{QStringLiteral("default"), QStringLiteral("Open")}};
    input.hints.category = QStringLiteral("email.arrived");
    input.hints.urgency = Urgency::Low;
    const auto result = service.submit(input);

    QVERIFY(result.ok());
    QCOMPARE(result.notificationId, quint32(1));
    QCOMPARE(result.revisionBefore, quint64(0));
    QCOMPARE(result.revisionAfter, quint64(1));
    QVERIFY(!result.replaced);
    QCOMPARE(initial->revision, quint64(0));
    QVERIFY(initial->notifications.isEmpty());
    QCOMPARE(service.snapshot()->notifications.size(), 1);
    const auto &stored = service.snapshot()->notifications.first();
    QCOMPARE(stored.sourceService, QStringLiteral(":1.10"));
    QCOMPARE(stored.createdAtMs, qint64(50));
    QVERIFY(!stored.updatedAtMs.has_value());
    QCOMPARE(stored.expiresAtMs, std::optional<qint64>(5'050));
    QCOMPARE(stored.actions, input.actions);
    QCOMPARE(backend.publications.size(), 1);
    QCOMPARE(backend.publications.first(), service.snapshot());
}

void NotificationSubmissionTests::replacementPreservesIdentityAndOldSnapshot()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);

    clock.now = 10;
    const auto firstResult = service.submit(request(QStringLiteral(":1.11"),
                                                    QStringLiteral("First")));
    QVERIFY(firstResult.ok());
    const auto retained = service.snapshot();

    clock.now = 25;
    auto replacement = request(QStringLiteral(":1.11"), QStringLiteral("Replacement"));
    replacement.replacesId = firstResult.notificationId;
    replacement.expireTimeoutMs = 1'000;
    const auto result = service.submit(replacement);

    QVERIFY(result.ok());
    QVERIFY(result.replaced);
    QCOMPARE(result.notificationId, firstResult.notificationId);
    QCOMPARE(service.snapshot()->notifications.size(), 1);
    const auto &stored = service.snapshot()->notifications.first();
    QCOMPARE(stored.summary, QStringLiteral("Replacement"));
    QCOMPARE(stored.createdAtMs, qint64(10));
    QCOMPARE(stored.updatedAtMs, std::optional<qint64>(25));
    QCOMPARE(stored.expiresAtMs, std::optional<qint64>(1'025));
    QCOMPARE(retained->revision, quint64(1));
    QCOMPARE(retained->notifications.first().summary, QStringLiteral("First"));
}

void NotificationSubmissionTests::replacementRequiresAuthenticatedOwner()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    const auto created = service.submit(request(QStringLiteral(":1.12")));
    QVERIFY(created.ok());
    const auto before = service.snapshot();

    auto hostile = request(QStringLiteral(":1.13"), QStringLiteral("Hijacked"));
    hostile.replacesId = created.notificationId;
    const auto result = service.submit(hostile);

    QCOMPARE(result.status, OperationStatus::NotOwner);
    QCOMPARE(service.snapshot(), before);
    QCOMPARE(backend.publications.size(), 1);
}

void NotificationSubmissionTests::missingReplacementIdIsReusedAsProtocolIdentity()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto input = request(QStringLiteral(":1.14"));
    input.replacesId = 4'000;

    const auto result = service.submit(input);

    QVERIFY(result.ok());
    QVERIFY(!result.replaced);
    QCOMPARE(result.notificationId, quint32(4'000));
    QCOMPARE(service.snapshot()->notifications.first().id, quint32(4'000));

    const auto allocated = service.submit(request(QStringLiteral(":1.141")));
    QVERIFY(allocated.ok());
    QCOMPARE(allocated.notificationId, quint32(1));

    QVERIFY(service.dismiss(result.notificationId).ok());
    auto resurrected = request(QStringLiteral(":1.14"));
    resurrected.replacesId = 4'000;
    QVERIFY(service.submit(resurrected).ok());
    QVERIFY(service.dismiss(4'000).ok());

    const auto afterClosedExplicit =
        service.submit(request(QStringLiteral(":1.142")));
    QVERIFY(afterClosedExplicit.ok());
    QCOMPARE(afterClosedExplicit.notificationId, quint32(2));
}

void NotificationSubmissionTests::invalidatedExplicitIdDoesNotAdvanceGeneratedSequence()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto explicitIdentity = request(QStringLiteral(":1.1421"));
    explicitIdentity.replacesId = 1;

    const auto admitted = service.submit(explicitIdentity);
    QVERIFY(admitted.ok());
    QCOMPARE(admitted.notificationId, quint32(1));
    QVERIFY(service.dismiss(admitted.notificationId).ok());

    // AGENT-CONTRACT: The protocol invalidates a closed caller-supplied ID.
    // It therefore cannot advance the separate, non-reusable sequence that
    // QindaQt assigns only when replaces_id is zero.
    const auto firstGenerated = service.submit(request(QStringLiteral(":1.1422")));
    const auto secondGenerated = service.submit(request(QStringLiteral(":1.1423")));
    QVERIFY(firstGenerated.ok());
    QVERIFY(secondGenerated.ok());
    QCOMPARE(firstGenerated.notificationId, quint32(1));
    QCOMPARE(secondGenerated.notificationId, quint32(2));
}

void NotificationSubmissionTests::generatedIdsSkipActiveExplicitIdentities()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    auto explicitIdentity = request(QStringLiteral(":1.143"));
    explicitIdentity.replacesId = 1;

    const auto admitted = service.submit(explicitIdentity);
    QVERIFY(admitted.ok());
    QCOMPARE(admitted.notificationId, quint32(1));

    const auto generated = service.submit(request(QStringLiteral(":1.144")));
    QVERIFY(generated.ok());
    QCOMPARE(generated.notificationId, quint32(2));
    QVERIFY(service.dismiss(admitted.notificationId).ok());

    auto maximumIdentity = request(QStringLiteral(":1.145"));
    maximumIdentity.replacesId = std::numeric_limits<quint32>::max();
    QVERIFY(service.submit(maximumIdentity).ok());
    const auto afterMaximum = service.submit(request(QStringLiteral(":1.146")));
    QVERIFY(afterMaximum.ok());
    QCOMPARE(afterMaximum.notificationId, quint32(3));
}

void NotificationSubmissionTests::sparseExplicitHistoryCannotDenyFutureIdentities()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);
    constexpr quint32 firstExplicitId = 10'000;
    constexpr quint32 spacing = 3;
    constexpr int sparseIdentityCount = 2'048;

    for (int index = 0; index < sparseIdentityCount; ++index) {
        auto explicitIdentity = request(QStringLiteral(":1.147"));
        explicitIdentity.replacesId =
            firstExplicitId + quint32(index) * spacing;
        const auto admitted = service.submit(explicitIdentity);
        QVERIFY(admitted.ok());
        QVERIFY(service.dismiss(admitted.notificationId).ok());
    }

    auto anotherSparseIdentity = request(QStringLiteral(":1.147"));
    anotherSparseIdentity.replacesId = 4'000'000;
    const auto admittedSparseIdentity = service.submit(anotherSparseIdentity);
    QVERIFY(admittedSparseIdentity.ok());
    QCOMPARE(admittedSparseIdentity.notificationId, quint32(4'000'000));
    QVERIFY(service.dismiss(admittedSparseIdentity.notificationId).ok());

    const auto generated = service.submit(request(QStringLiteral(":1.148")));
    QVERIFY(generated.ok());
    QCOMPARE(generated.notificationId, quint32(1));
    for (int index = 0; index < sparseIdentityCount; ++index) {
        QVERIFY(generated.notificationId
                != firstExplicitId + quint32(index) * spacing);
    }
}

void NotificationSubmissionTests::malformedPayloadsAreAtomicallyRejected()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationService service(clock, backend);

    QVector<NotificationRequest> invalid;
    auto duplicateActions = request(QStringLiteral(":1.15"));
    duplicateActions.actions = {
        {QStringLiteral("same"), QStringLiteral("One")},
        {QStringLiteral("same"), QStringLiteral("Two")},
    };
    invalid.push_back(duplicateActions);

    auto oversized = request(QStringLiteral(":1.15"));
    oversized.body = QString(NotificationLimits::MaximumBodyBytes + 1, QLatin1Char('x'));
    invalid.push_back(oversized);

    auto malformedUtf16 = request(QStringLiteral(":1.15"));
    malformedUtf16.summary = QString(QChar(0xD800));
    invalid.push_back(malformedUtf16);

    auto malformedImage = request(QStringLiteral(":1.15"));
    malformedImage.hints.image = NotificationImage{
        .width = 2,
        .height = 2,
        .rowStride = 8,
        .hasAlpha = true,
        .bitsPerSample = 8,
        .channels = 4,
        .pixels = QByteArray(15, '\0'),
    };
    invalid.push_back(malformedImage);

    auto invalidTimeout = request(QStringLiteral(":1.15"));
    invalidTimeout.expireTimeoutMs = -2;
    invalid.push_back(invalidTimeout);

    auto invalidUrgency = request(QStringLiteral(":1.15"));
    invalidUrgency.hints.urgency = static_cast<Urgency>(3);
    invalid.push_back(invalidUrgency);

    const auto before = service.snapshot();
    for (const auto &input : invalid) {
        const auto result = service.submit(input);
        QCOMPARE(result.status, OperationStatus::InvalidRequest);
        QCOMPARE(service.snapshot(), before);
    }
    QVERIFY(backend.publications.isEmpty());

    QVERIFY(isValidUrgency(Urgency::Low));
    QVERIFY(isValidUrgency(Urgency::Normal));
    QVERIFY(isValidUrgency(Urgency::Critical));
    QVERIFY(!isValidUrgency(static_cast<Urgency>(3)));
}

void NotificationSubmissionTests::capacityIsBoundedButReplacementRemainsAvailable()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 1;
    NotificationService service(clock, backend, policy);

    const auto first = service.submit(request(QStringLiteral(":1.16")));
    QVERIFY(first.ok());
    QCOMPARE(service.submit(request(QStringLiteral(":1.17"))).status,
             OperationStatus::CapacityReached);

    auto replacement = request(QStringLiteral(":1.16"), QStringLiteral("Still allowed"));
    replacement.replacesId = first.notificationId;
    const auto replaced = service.submit(replacement);
    QVERIFY(replaced.ok());
    QVERIFY(replaced.replaced);
    QCOMPARE(service.snapshot()->notifications.size(), 1);
}

void NotificationSubmissionTests::perSourcePersistentQuotaPreservesCapacityForPeers()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    QCOMPARE(policy.maximumActiveNotificationsPerSource, qsizetype(64));
    QCOMPARE(policy.maximumRetainedPayloadBytesPerSource,
             qsizetype(32 * 1'024 * 1'024));
    policy.maximumActiveNotifications = 4;
    policy.maximumActiveNotificationsPerSource = 2;
    NotificationService service(clock, backend, policy);

    auto persistent = request(QStringLiteral(":1.50"));
    persistent.expireTimeoutMs = 0;
    const auto first = service.submit(persistent);
    const auto second = service.submit(persistent);
    QVERIFY(first.ok());
    QVERIFY(second.ok());
    const auto beforeRejection = service.snapshot();

    const auto rejected = service.submit(persistent);
    QCOMPARE(rejected.status, OperationStatus::CapacityReached);
    QCOMPARE(service.snapshot(), beforeRejection);
    QCOMPARE(backend.publications.size(), 2);

    auto peer = persistent;
    peer.sourceService = QStringLiteral(":1.51");
    const auto admittedPeer = service.submit(peer);
    QVERIFY(admittedPeer.ok());
    QCOMPARE(admittedPeer.notificationId, quint32(3));

    auto replacement = persistent;
    replacement.summary = QStringLiteral("Replacement within source quota");
    replacement.replacesId = first.notificationId;
    QVERIFY(service.submit(replacement).ok());

    QVERIFY(service.dismiss(second.notificationId).ok());
    const auto admittedAfterRelease = service.submit(persistent);
    QVERIFY(admittedAfterRelease.ok());
    QCOMPARE(admittedAfterRelease.notificationId, quint32(4));
}

void NotificationSubmissionTests::perSourcePayloadQuotaIsAtomic()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 5;
    policy.maximumRetainedPayloadBytes = 500;
    policy.maximumRetainedPayloadBytesPerSource = 100;
    NotificationService service(clock, backend, policy);

    auto first = request(QStringLiteral(":1.52"));
    first.body = QString(60, QLatin1Char('x'));
    const auto admitted = service.submit(first);
    QVERIFY(admitted.ok());
    const auto beforeRejection = service.snapshot();

    auto sameSource = request(QStringLiteral(":1.52"));
    sameSource.body = QString(30, QLatin1Char('x'));
    QCOMPARE(service.submit(sameSource).status, OperationStatus::CapacityReached);
    QCOMPARE(service.snapshot(), beforeRejection);

    auto peer = first;
    peer.sourceService = QStringLiteral(":1.53");
    QVERIFY(service.submit(peer).ok());

    auto replacement = first;
    replacement.body = QString(70, QLatin1Char('x'));
    replacement.replacesId = admitted.notificationId;
    QVERIFY(service.submit(replacement).ok());
}

void NotificationSubmissionTests::aggregatePayloadBudgetIsAtomic()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 2;
    policy.maximumRetainedPayloadBytes = 60;
    NotificationService service(clock, backend, policy);

    auto large = request(QStringLiteral(":1.60"));
    large.body = QString(30, QLatin1Char('x'));
    const auto first = service.submit(large);
    QVERIFY(first.ok());
    const auto retained = service.snapshot();

    auto secondLarge = large;
    secondLarge.sourceService = QStringLiteral(":1.61");
    QCOMPARE(service.submit(secondLarge).status, OperationStatus::CapacityReached);
    QCOMPARE(service.snapshot(), retained);

    auto smallerReplacement = request(QStringLiteral(":1.60"), QStringLiteral("S"));
    smallerReplacement.body.clear();
    smallerReplacement.replacesId = first.notificationId;
    QVERIFY(service.submit(smallerReplacement).ok());
    auto secondSmall = request(QStringLiteral(":1.61"), QStringLiteral("S"));
    secondSmall.body.clear();
    QVERIFY(service.submit(secondSmall).ok());
    QCOMPARE(service.snapshot()->notifications.size(), 2);
}

void NotificationSubmissionTests::invalidPolicyNeverBecomesReady()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.maximumActiveNotifications = 0;
    NotificationService service(clock, backend, policy);

    QVERIFY(!service.isReady());
    QVERIFY(!service.initializationError().isEmpty());
    QCOMPARE(service.submit(request(QStringLiteral(":1.18"))).status,
             OperationStatus::InvalidPolicy);
    QCOMPARE(service.snapshot()->revision, quint64(0));
}

void NotificationSubmissionTests::invalidPerSourcePoliciesNeverBecomeReady()
{
    ManualNotificationClock clock;
    RecordingNotificationBackend countBackend;
    NotificationPolicy invalidCount;
    invalidCount.maximumActiveNotificationsPerSource = 0;
    NotificationService countService(clock, countBackend, invalidCount);
    QVERIFY(!countService.isReady());
    QCOMPARE(countService.submit(request(QStringLiteral(":1.181"))).status,
             OperationStatus::InvalidPolicy);

    RecordingNotificationBackend payloadBackend;
    NotificationPolicy invalidPayload;
    invalidPayload.maximumRetainedPayloadBytesPerSource = 0;
    NotificationService payloadService(clock, payloadBackend, invalidPayload);
    QVERIFY(!payloadService.isReady());
    QCOMPARE(payloadService.submit(request(QStringLiteral(":1.182"))).status,
             OperationStatus::InvalidPolicy);
}

void NotificationSubmissionTests::timeoutPolicyIsDeterministicAndBounded()
{
    ManualNotificationClock clock;
    clock.now = 100;
    RecordingNotificationBackend backend;
    NotificationPolicy policy;
    policy.defaultTimeoutMs = 200;
    policy.criticalDefaultTimeoutMs = 0;
    policy.maximumRequestedTimeoutMs = 500;
    NotificationService service(clock, backend, policy);

    auto normal = request(QStringLiteral(":1.19"));
    const auto normalResult = service.submit(normal);
    QVERIFY(normalResult.ok());
    QCOMPARE(service.snapshot()->notifications.first().expiresAtMs,
             std::optional<qint64>(300));

    auto critical = request(QStringLiteral(":1.20"));
    critical.hints.urgency = Urgency::Critical;
    const auto criticalResult = service.submit(critical);
    QVERIFY(criticalResult.ok());
    QVERIFY(!service.snapshot()->notifications.at(1).expiresAtMs.has_value());

    auto capped = request(QStringLiteral(":1.21"));
    capped.expireTimeoutMs = 2'000;
    const auto cappedResult = service.submit(capped);
    QVERIFY(cappedResult.ok());
    QCOMPARE(service.snapshot()->notifications.at(2).expiresAtMs,
             std::optional<qint64>(600));
}

QTEST_GUILESS_MAIN(NotificationSubmissionTests)
#include "tst_notification_submission.moc"
