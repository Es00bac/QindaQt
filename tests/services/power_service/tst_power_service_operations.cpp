// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"

#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

struct OperationHarness
{
    FakeBatteryCollaborator battery;
    FakeProfileCollaborator profiles;
    FakeSessionCollaborator session;
    std::unique_ptr<PowerServiceCoordinator> coordinator;

    OperationHarness()
    {
        coordinator = std::make_unique<PowerServiceCoordinator>(&battery, &profiles,
                                                                &session);
        coordinator->start();
        battery.publish(fixtureBatteryFacts());
        profiles.publish(fixtureProfileFacts());
        session.publish(fixtureSessionFacts());
    }

    [[nodiscard]] Handle keyboardHandle() const
    {
        return coordinator->snapshot().keyboardBacklights.first().handle;
    }
    [[nodiscard]] Handle holdHandle() const
    {
        return coordinator->snapshot().profiles.holds.first().handle;
    }
};

} // namespace

class PowerServiceOperationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void setProfileSucceedsExactlyOnce();
    void unknownProfileIsRejected();
    void acquireAndReleaseHoldsRoundTrip();
    void staleHoldHandleIsRejected();
    void keyboardBrightnessValidatesHandleAndRange();
    void internalBrightnessAdmitsOnlyTheSelectedWritablePanel();
    void internalBrightnessCompletesAfterObservedReadback();
    void authorityReplacementMakesDispatchedOperationUncertain();
    void duplicateUpstreamReplyIsDropped();
    void malformedOutcomeBecomesProtocolValidFailure();
    void busyCapRejectsExcessPendingOperations();
    void stopMakesPendingOperationsUncertain();
    void unavailableServiceRejectsOperations();
    void lateReplyAfterCompletionIsDropped();
    void successCarriesCurrentObservedLineage();
};

void PowerServiceOperationTests::setProfileSucceedsExactlyOnce()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("performance"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(submission.pending);
    QVERIFY(submission.operationId != 0);
    QCOMPARE(harness.profiles.profileOperations.size(), 1);
    QCOMPARE(harness.profiles.profileOperations.first().profileId,
             QStringLiteral("performance"));
    QCOMPARE(completed.size(), 0);

    harness.profiles.finish(submission.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.kind, OperationKind::SetProfile);
    QCOMPARE(result.initiatingEpoch, harness.coordinator->snapshot().epoch);
    QVERIFY(result.observedRevision >= result.initiatingRevision);
    QCOMPARE(result.reasonCode, QStringLiteral("applied"));
}

void PowerServiceOperationTests::unknownProfileIsRejected()
{
    OperationHarness harness;
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("turbo"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(!submission.pending);
    QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("unknown-profile"));
    QVERIFY(harness.profiles.profileOperations.isEmpty());
}

void PowerServiceOperationTests::acquireAndReleaseHoldsRoundTrip()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission acquire = harness.coordinator->submit(
        {.kind = OperationKind::AcquireProfileHold,
         .profileId = QStringLiteral("power-saver"),
         .applicationName = QStringLiteral("QindaQt Tests"),
         .reason = QStringLiteral("keep quiet"),
         .handle = {},
         .value = 0});
    QVERIFY(acquire.pending);
    QCOMPARE(harness.profiles.profileOperations.first().kind,
             OperationKind::AcquireProfileHold);
    QCOMPARE(harness.profiles.profileOperations.first().applicationName,
             QStringLiteral("QindaQt Tests"));
    harness.profiles.finish(acquire.operationId, succeededOutcome());
    QTRY_COMPARE(completed.size(), 1);

    const OperationSubmission release = harness.coordinator->submit(
        {.kind = OperationKind::ReleaseProfileHold,
         .profileId = {},
         .applicationName = {},
         .reason = {},
         .handle = harness.holdHandle(),
         .value = 0});
    QVERIFY(release.pending);
    QCOMPARE(harness.profiles.profileOperations.last().kind,
             OperationKind::ReleaseProfileHold);
    QCOMPARE(harness.profiles.profileOperations.last().hold, harness.holdHandle());
    harness.profiles.finish(release.operationId,
                            {.status = CollaboratorStatus::Failed,
                             .reasonCode = QStringLiteral("hold-not-found"),
                             .diagnostic = {}});
    QTRY_COMPARE(completed.size(), 2);
    const OperationResult failed = completed.last().at(1).value<OperationResult>();
    QCOMPARE(failed.status, OperationStatus::Failed);
    QCOMPARE(failed.reasonCode, QStringLiteral("hold-not-found"));
}

void PowerServiceOperationTests::staleHoldHandleIsRejected()
{
    OperationHarness harness;
    Handle stale = harness.holdHandle();
    stale.epoch -= 1;
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::ReleaseProfileHold,
         .profileId = {},
         .applicationName = {},
         .reason = {},
         .handle = stale,
         .value = 0});
    QVERIFY(!submission.pending);
    QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("stale-handle"));
}

void PowerServiceOperationTests::keyboardBrightnessValidatesHandleAndRange()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);

    const OperationSubmission overflow = harness.coordinator->submit(
        {.kind = OperationKind::SetKeyboardBrightness,
         .profileId = {},
         .applicationName = {},
         .reason = {},
         .handle = harness.keyboardHandle(),
         .value = 256});
    QVERIFY(!overflow.pending);
    QCOMPARE(overflow.immediateResult.status, OperationStatus::Unsupported);

    Handle stale = harness.keyboardHandle();
    stale.epoch += 1;
    const OperationSubmission foreign = harness.coordinator->submit(
        {.kind = OperationKind::SetKeyboardBrightness,
         .profileId = {},
         .applicationName = {},
         .reason = {},
         .handle = stale,
         .value = 10});
    QVERIFY(!foreign.pending);
    QCOMPARE(foreign.immediateResult.reasonCode, QStringLiteral("stale-handle"));

    const OperationSubmission accepted = harness.coordinator->submit(
        {.kind = OperationKind::SetKeyboardBrightness,
         .profileId = {},
         .applicationName = {},
         .reason = {},
         .handle = harness.keyboardHandle(),
         .value = 200});
    QVERIFY(accepted.pending);
    QCOMPARE(harness.battery.keyboardOperations.size(), 1);
    QCOMPARE(harness.battery.keyboardOperations.first().value, quint32(200));
    harness.battery.finish(accepted.operationId, succeededOutcome());
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
}

namespace {

PowerServiceRequest internalRequest(const Handle &handle, const quint32 value)
{
    return {.kind = OperationKind::SetInternalBrightness,
            .profileId = {},
            .applicationName = {},
            .reason = {},
            .handle = handle,
            .value = value};
}

} // namespace

void PowerServiceOperationTests::internalBrightnessAdmitsOnlyTheSelectedWritablePanel()
{
    OperationHarness harness;
    const quint64 epoch = harness.coordinator->snapshot().epoch;
    const Handle panel{.epoch = epoch, .opaqueId = QStringLiteral("backlight-panel")};
    const Handle raw{.epoch = epoch, .opaqueId = QStringLiteral("backlight-raw")};

    // Without internal inventory the capability itself is absent.
    OperationSubmission refused = harness.coordinator->submit(internalRequest(panel, 10));
    QVERIFY(!refused.pending);
    QCOMPARE(refused.immediateResult.kind, OperationKind::SetInternalBrightness);
    QCOMPARE(refused.immediateResult.status, OperationStatus::Unsupported);

    BatteryFacts facts = fixtureBatteryFacts();
    facts.internalBacklights = {
        fixtureInternalBacklight(raw.opaqueId, BacklightKind::Raw),
        fixtureInternalBacklight(panel.opaqueId)};
    harness.battery.publish(facts);
    QVERIFY(harness.coordinator->snapshot().capabilities.testFlag(
        Capability::InternalBacklight));

    refused = harness.coordinator->submit(
        internalRequest({.epoch = epoch + 1, .opaqueId = panel.opaqueId}, 10));
    QCOMPARE(refused.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(refused.immediateResult.reasonCode, QStringLiteral("stale-handle"));
    refused = harness.coordinator->submit(
        internalRequest({.epoch = epoch, .opaqueId = QStringLiteral("backlight-gone")}, 10));
    QCOMPARE(refused.immediateResult.reasonCode, QStringLiteral("stale-handle"));
    // A lower kernel type preference is never the target.
    refused = harness.coordinator->submit(internalRequest(raw, 10));
    QCOMPARE(refused.immediateResult.status, OperationStatus::Unsupported);
    refused = harness.coordinator->submit(internalRequest(panel, 256));
    QCOMPARE(refused.immediateResult.status, OperationStatus::Unsupported);

    facts.internalBacklights = {fixtureInternalBacklight(panel.opaqueId),
                                fixtureInternalBacklight(QStringLiteral("backlight-dsi"))};
    harness.battery.publish(facts);
    refused = harness.coordinator->submit(internalRequest(panel, 10));
    QCOMPARE(refused.immediateResult.status, OperationStatus::Unsupported);

    // A read-only selected panel is refused and never replaced by another device.
    facts.internalBacklights = {
        fixtureInternalBacklight(panel.opaqueId, BacklightKind::Firmware,
                                 BacklightStatus::Unavailable),
        fixtureInternalBacklight(raw.opaqueId, BacklightKind::Raw)};
    harness.battery.publish(facts);
    QCOMPARE(harness.coordinator->submit(internalRequest(panel, 10)).immediateResult.status,
             OperationStatus::Unsupported);
    QCOMPARE(harness.coordinator->submit(internalRequest(raw, 10)).immediateResult.status,
             OperationStatus::Unsupported);

    QVERIFY(harness.battery.internalOperations.isEmpty());
    QVERIFY(harness.battery.keyboardOperations.isEmpty());
}

void PowerServiceOperationTests::internalBrightnessCompletesAfterObservedReadback()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    BatteryFacts facts = fixtureBatteryFacts();
    facts.internalBacklights = {fixtureInternalBacklight()};
    harness.battery.publish(facts);
    const Handle panel = harness.coordinator->snapshot().internalBacklights.first().handle;
    const quint64 initiatingRevision = harness.coordinator->snapshot().revision;

    const OperationSubmission accepted =
        harness.coordinator->submit(internalRequest(panel, 200));
    QVERIFY(accepted.pending);
    QCOMPARE(harness.battery.internalOperations.size(), 1);
    QCOMPARE(harness.battery.internalOperations.first().operationId, accepted.operationId);
    QCOMPARE(harness.battery.internalOperations.first().device, panel);
    QCOMPARE(harness.battery.internalOperations.first().value, quint32(200));
    QCOMPARE(completed.size(), 0);

    // The production adapter publishes its readback before completing.
    facts.internalBacklights.first().observed = 200;
    harness.battery.publish(facts);
    harness.battery.finish(accepted.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.kind, OperationKind::SetInternalBrightness);
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.initiatingRevision, initiatingRevision);
    QCOMPARE(result.observedRevision, harness.coordinator->snapshot().revision);
    QVERIFY(result.observedRevision > initiatingRevision);
    QCOMPARE(harness.coordinator->snapshot().internalBacklights.first().observed,
             quint32(200));

    harness.battery.finish(accepted.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);
}

void PowerServiceOperationTests::authorityReplacementMakesDispatchedOperationUncertain()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const quint64 initiatingEpoch = harness.coordinator->snapshot().epoch;
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(submission.pending);

    harness.battery.replaceAuthority();
    QTRY_COMPARE(completed.size(), 1);
    OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("authority-replaced"));
    QCOMPARE(result.initiatingEpoch, initiatingEpoch);

    // A delayed success from the old authority can never restore success.
    harness.profiles.finishForGeneration(harness.profiles.generation,
                                         submission.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);

    // A second operation under the new epoch completes normally.
    const OperationSubmission fresh = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(fresh.pending);
    harness.profiles.finish(fresh.operationId, succeededOutcome());
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.last().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
}

void PowerServiceOperationTests::duplicateUpstreamReplyIsDropped()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    harness.profiles.finish(submission.operationId, succeededOutcome());
    harness.profiles.finish(submission.operationId, succeededOutcome());
    harness.profiles.finish(submission.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);
}

void PowerServiceOperationTests::malformedOutcomeBecomesProtocolValidFailure()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});

    harness.profiles.finish(
        submission.operationId,
        {.status = CollaboratorStatus::Succeeded,
         .reasonCode = QStringLiteral("Not A Token!"),
         .diagnostic = QStringLiteral("raw \x01 upstream text")});
    QTRY_COMPARE(completed.size(), 1);
    OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Failed);
    QCOMPARE(result.reasonCode, QStringLiteral("upstream-malformed"));
    QVERIFY(result.diagnostic.isEmpty());
}

void PowerServiceOperationTests::busyCapRejectsExcessPendingOperations()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    for (int i = 0; i < kMaxServicePendingOperations; ++i) {
        const OperationSubmission submission = harness.coordinator->submit(
            {.kind = OperationKind::SetProfile,
             .profileId = QStringLiteral("balanced"),
             .applicationName = {},
             .reason = {},
             .handle = {},
             .value = 0});
        QVERIFY(submission.pending);
    }
    const OperationSubmission excess = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(!excess.pending);
    QCOMPARE(excess.immediateResult.status, OperationStatus::Busy);
    QCOMPARE(excess.immediateResult.reasonCode, QStringLiteral("too-many-operations"));
    QCOMPARE(harness.profiles.profileOperations.size(), kMaxServicePendingOperations);
}

void PowerServiceOperationTests::stopMakesPendingOperationsUncertain()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(submission.pending);
    harness.coordinator->stop();
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed.first().at(1).value<OperationResult>().reasonCode,
             QStringLiteral("service-stopped"));
    // The stopped coordinator no longer accepts operations.
    const OperationSubmission rejected = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(!rejected.pending);
    QCOMPARE(rejected.immediateResult.reasonCode, QStringLiteral("unavailable"));
}

void PowerServiceOperationTests::unavailableServiceRejectsOperations()
{
    FakeBatteryCollaborator battery;
    FakeProfileCollaborator profiles;
    FakeSessionCollaborator session;
    PowerServiceCoordinator coordinator(&battery, &profiles, &session);
    // Not started: every operation is rejected as unavailable.
    const OperationSubmission submission = coordinator.submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    QVERIFY(!submission.pending);
    QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("unavailable"));
}

void PowerServiceOperationTests::lateReplyAfterCompletionIsDropped()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    harness.profiles.finish(submission.operationId,
                            {.status = CollaboratorStatus::Failed,
                             .reasonCode = QStringLiteral("upstream-busy"),
                             .diagnostic = {}});
    QCOMPARE(completed.size(), 1);
    // A late different-status reply for the same ID is dropped, not merged.
    harness.profiles.finish(submission.operationId, succeededOutcome());
    QCOMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Failed);
}

void PowerServiceOperationTests::successCarriesCurrentObservedLineage()
{
    OperationHarness harness;
    QSignalSpy completed(harness.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    const OperationSubmission submission = harness.coordinator->submit(
        {.kind = OperationKind::SetProfile,
         .profileId = QStringLiteral("balanced"),
         .applicationName = {},
         .reason = {},
         .handle = {},
         .value = 0});
    // Facts arrive between dispatch and completion; the success must observe
    // the current lineage, never an earlier revision.
    harness.session.publish(fixtureSessionFacts());
    harness.profiles.finish(submission.operationId, succeededOutcome());
    QTRY_COMPARE(completed.size(), 1);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.observedEpoch, harness.coordinator->snapshot().epoch);
    QCOMPARE(result.observedRevision, harness.coordinator->snapshot().revision);
    QVERIFY(result.observedRevision > result.initiatingRevision);
}

QTEST_GUILESS_MAIN(PowerServiceOperationTests)
#include "tst_power_service_operations.moc"
