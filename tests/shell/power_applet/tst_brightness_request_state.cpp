// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/power_applet/brightness_request_state.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <QtTest>

using namespace QindaQt::Shell::PowerApplet;

namespace {

constexpr quint64 kEpoch = 7;
constexpr quint64 kRevision = 42;
constexpr quint64 kObservedRevision = 43;

Power::Snapshot keyboardSnapshot() {
  Power::Snapshot snapshot;
  snapshot.epoch = kEpoch;
  snapshot.revision = kRevision;
  snapshot.availability = Power::Availability::Ready;
  snapshot.capabilities.setFlag(Power::Capability::KeyboardBacklight);
  Power::KeyboardBacklight device;
  device.handle = {.epoch = kEpoch, .opaqueId = QStringLiteral("kb0")};
  device.name = QStringLiteral("Lenovo kbd");
  device.valueKnown = true;
  device.value = 2;
  device.maximum = 4;
  device.normalized = 5000;
  device.canSet = true;
  snapshot.keyboardBacklights.append(device);
  return snapshot;
}

Power::Handle keyboardHandle() {
  return {.epoch = kEpoch, .opaqueId = QStringLiteral("kb0")};
}

Power::OperationResult successResult() {
  Power::OperationResult result;
  result.kind = Power::OperationKind::SetKeyboardBrightness;
  result.status = Power::OperationStatus::Succeeded;
  result.initiatingEpoch = kEpoch;
  result.initiatingRevision = kRevision;
  result.observedEpoch = kEpoch;
  result.observedRevision = kObservedRevision;
  result.reasonCode = QStringLiteral("ok");
  return result;
}

BrightnessRequest pendingRequest() {
  return beginKeyboardBrightnessRequest(keyboardSnapshot(),
                                        keyboardHandle());
}

bool sameState(const BrightnessRequest &left, const BrightnessRequest &right) {
  return left == right;
}

} // namespace

class BrightnessRequestStateTests final : public QObject {
    Q_OBJECT

private slots:
    void beginRequiresAdjustableDeviceAndLegalGeneration();
    void successCompletesOnlyInMatchingLineage();
    void staleRepliesNeverCompletePendingRequests();
    void failureStatusesProduceTypedFeedback();
    void hostileRepliesBecomeUncertain();
    void ownerLossAndEpochReplacementInvalidatePending();
    void terminalRequestsNeverMutateAgain();
};

void BrightnessRequestStateTests::
    beginRequiresAdjustableDeviceAndLegalGeneration()
{
    const Power::Snapshot snapshot = keyboardSnapshot();

    const BrightnessRequest invalidDevice = beginKeyboardBrightnessRequest(
        snapshot, Power::Handle{});
    QCOMPARE(invalidDevice.phase, RequestPhase::Failed);
    QCOMPARE(invalidDevice.failure, FailureKind::Unsupported);
    QVERIFY(!invalidDevice.feedback.isEmpty());

    const BrightnessRequest unknownDevice =
        beginKeyboardBrightnessRequest(
            snapshot, Power::Handle{kEpoch, QStringLiteral("kb-missing")});
    QCOMPARE(unknownDevice.phase, RequestPhase::Failed);
    QCOMPARE(unknownDevice.failure, FailureKind::Unsupported);

    Power::Snapshot readOnly = keyboardSnapshot();
    readOnly.keyboardBacklights.first().canSet = false;
    const BrightnessRequest notAdjustable = beginKeyboardBrightnessRequest(
        readOnly, keyboardHandle());
    QCOMPARE(notAdjustable.phase, RequestPhase::Failed);
    QCOMPARE(notAdjustable.failure, FailureKind::Unsupported);

    Power::Snapshot uncapped = keyboardSnapshot();
    uncapped.capabilities = Power::Capabilities{};
    const BrightnessRequest noCapability = beginKeyboardBrightnessRequest(
        uncapped, keyboardHandle());
    QCOMPARE(noCapability.phase, RequestPhase::Failed);

    Power::Snapshot zeroEpoch = keyboardSnapshot();
    zeroEpoch.epoch = 0;
    const BrightnessRequest illegal = beginKeyboardBrightnessRequest(
        zeroEpoch, keyboardHandle());
    QCOMPARE(illegal.phase, RequestPhase::Failed);

    const BrightnessRequest request = pendingRequest();
    QCOMPARE(request.phase, RequestPhase::Pending);
    QCOMPARE(request.initiatingEpoch, kEpoch);
    QCOMPARE(request.initiatingRevision, kRevision);
    QCOMPARE(request.device, keyboardHandle());
    QCOMPARE(request.failure, FailureKind::None);
    QVERIFY(request.feedback.isEmpty());
}

void BrightnessRequestStateTests::successCompletesOnlyInMatchingLineage()
{
    const BrightnessRequest completed =
        applyOperationResult(pendingRequest(), successResult());
    QCOMPARE(completed.phase, RequestPhase::Succeeded);
    QCOMPARE(completed.failure, FailureKind::None);
    QVERIFY(!completed.feedback.isEmpty());

    Power::OperationResult earlyObservation = successResult();
    earlyObservation.observedRevision = kRevision - 1;
    const BrightnessRequest uncertain =
        applyOperationResult(pendingRequest(), earlyObservation);
    QCOMPARE(uncertain.phase, RequestPhase::Uncertain);
    QCOMPARE(uncertain.failure, FailureKind::LineageMismatch);

    Power::OperationResult foreignEpoch = successResult();
    foreignEpoch.observedEpoch = kEpoch + 1;
    const BrightnessRequest moved =
        applyOperationResult(pendingRequest(), foreignEpoch);
    QCOMPARE(moved.phase, RequestPhase::Uncertain);
    QCOMPARE(moved.failure, FailureKind::LineageMismatch);
}

void BrightnessRequestStateTests::staleRepliesNeverCompletePendingRequests()
{
    const BrightnessRequest request = pendingRequest();

    Power::OperationResult otherRevision = successResult();
    otherRevision.initiatingRevision = kRevision + 5;
    QVERIFY(sameState(applyOperationResult(request, otherRevision), request));

    Power::OperationResult otherEpoch = successResult();
    otherEpoch.initiatingEpoch = kEpoch + 1;
    QVERIFY(sameState(applyOperationResult(request, otherEpoch), request));

    Power::OperationResult otherKind = successResult();
    otherKind.kind = Power::OperationKind::SetProfile;
    QVERIFY(sameState(applyOperationResult(request, otherKind), request));

    // The live answer still completes the untouched pending request.
    QVERIFY(applyOperationResult(request, successResult()).phase ==
            RequestPhase::Succeeded);
}

void BrightnessRequestStateTests::failureStatusesProduceTypedFeedback()
{
    const struct {
        Power::OperationStatus status;
        FailureKind expected;
    } rows[] = {
        {Power::OperationStatus::Rejected, FailureKind::Rejected},
        {Power::OperationStatus::Unsupported, FailureKind::Unsupported},
        {Power::OperationStatus::Failed, FailureKind::Failed},
        {Power::OperationStatus::Busy, FailureKind::Busy},
        {Power::OperationStatus::AuthenticationRequired,
         FailureKind::AuthenticationRequired},
        {Power::OperationStatus::Inhibited, FailureKind::Inhibited},
        {Power::OperationStatus::Uncertain, FailureKind::ServiceUncertain},
    };
    for (const auto &row : rows) {
        Power::OperationResult result = successResult();
        result.status = row.status;
        result.reasonCode = QStringLiteral("why");
        result.diagnostic = QStringLiteral("detail");
        const BrightnessRequest request =
            applyOperationResult(pendingRequest(), result);
        QCOMPARE(request.failure, row.expected);
        QVERIFY(!request.feedback.isEmpty());
        if (row.status != Power::OperationStatus::Succeeded) {
            QCOMPARE(request.reasonCode, QStringLiteral("why"));
            QCOMPARE(request.diagnostic, QStringLiteral("detail"));
        }
        QVERIFY(request.phase != RequestPhase::Succeeded);
    }
}

void BrightnessRequestStateTests::hostileRepliesBecomeUncertain()
{
    const BrightnessRequest request = pendingRequest();

    Power::OperationResult malformed = successResult();
    malformed.wireValid = false;
    const BrightnessRequest unreadable =
        applyOperationResult(request, malformed);
    QCOMPARE(unreadable.phase, RequestPhase::Uncertain);
    QCOMPARE(unreadable.failure, FailureKind::MalformedReply);
    QVERIFY(!unreadable.feedback.isEmpty());

    Power::OperationResult hostileStatus = successResult();
    hostileStatus.status = static_cast<Power::OperationStatus>(99U);
    const BrightnessRequest unknown =
        applyOperationResult(request, hostileStatus);
    QCOMPARE(unknown.phase, RequestPhase::Uncertain);
    QCOMPARE(unknown.failure, FailureKind::MalformedReply);
}

void BrightnessRequestStateTests::
    ownerLossAndEpochReplacementInvalidatePending()
{
    const BrightnessRequest request = pendingRequest();

    QVERIFY(sameState(observeGeneration(request, true, kEpoch), request));

    const BrightnessRequest ownerGone = observeGeneration(request, false, kEpoch);
    QCOMPARE(ownerGone.phase, RequestPhase::Uncertain);
    QCOMPARE(ownerGone.failure, FailureKind::OwnerLost);
    QVERIFY(!ownerGone.feedback.isEmpty());

    const BrightnessRequest replaced =
        observeGeneration(request, true, kEpoch + 1);
    QCOMPARE(replaced.phase, RequestPhase::Uncertain);
    QCOMPARE(replaced.failure, FailureKind::OwnerLost);
}

void BrightnessRequestStateTests::terminalRequestsNeverMutateAgain()
{
    const BrightnessRequest succeeded =
        applyOperationResult(pendingRequest(), successResult());
    QVERIFY(sameState(
        applyOperationResult(succeeded, successResult()), succeeded));
    QVERIFY(sameState(observeGeneration(succeeded, false, kEpoch + 1),
                      succeeded));

    Power::OperationResult rejection = successResult();
    rejection.status = Power::OperationStatus::Rejected;
    const BrightnessRequest failed =
        applyOperationResult(pendingRequest(), rejection);
    QCOMPARE(failed.phase, RequestPhase::Failed);
    QVERIFY(sameState(
        applyOperationResult(failed, successResult()), failed));
    QVERIFY(sameState(observeGeneration(failed, true, kEpoch + 1), failed));

    const BrightnessRequest uncertain = observeGeneration(
        pendingRequest(), false, kEpoch);
    QVERIFY(sameState(
        applyOperationResult(uncertain, successResult()), uncertain));
}

QTEST_GUILESS_MAIN(BrightnessRequestStateTests)
#include "tst_brightness_request_state.moc"
