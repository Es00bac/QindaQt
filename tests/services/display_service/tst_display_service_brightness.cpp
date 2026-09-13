// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "display_brightness_authority_p.h"
#include "support/display_brightness_test_support.h"
#include "support/display_service_test_support.h"

#include <QtTest/QTest>

#include <functional>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;
using namespace QindaQt::DisplayService::TestSupport::Brightness;
using Display::ErrorCode;
using Display::OperationStatus;
using DisplayTransaction::MachineState;
using DisplayTransaction::SafetyState;

namespace
{

class AuthorityFixture
{
public:
    AuthorityFixture()
    {
        authority.devicesObserved(devices(), &current);
        (void)authority.takePublicationChanged();
    }

    BrightnessRequestResult request(const quint32 value, const QString &stableId = kExternal,
                                    const MachineState state = MachineState::Ready,
                                    const SafetyState safety = SafetyState::Safe)
    {
        return authority.request(requestFor(*authority.snapshot(), stableId, value), &current,
                                 state, safety);
    }

    FakeClock clock;
    FakeTransactionPort port;
    Display::Snapshot current = topology();
    Private::BrightnessAuthority authority{clock, port, 500};
};

} // namespace

class DisplayServiceBrightnessTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesJoinedCapabilityAndValue();
    void clearsOnDeviceAndOwnerLoss();
    void joinsOnlyExactConnectorAndUuid();
    void pinsRevisionToTheJoinedTopology();
    void refusesInadmissibleRequestsWithoutWriting();
    void appliedRequiresAcknowledgementAndObservation();
    void serializesAndTypesPortFailures();
    void neverReplaysAfterTopologyOrOwnerChange();
    void deadlineMakesUnobservedApplyUncertain();
};

void DisplayServiceBrightnessTest::publishesJoinedCapabilityAndValue()
{
    FakeClock clock;
    FakeTransactionPort port;
    Private::BrightnessAuthority authority(clock, port, 500);
    const Display::Snapshot current = topology();
    authority.refresh(&current);
    QVERIFY(authority.takePublicationChanged());
    const Display::BrightnessSnapshot *published = authority.snapshot();
    QVERIFY(published != nullptr);
    QCOMPARE(published->revision, quint64{1});
    QCOMPARE(published->topologyRevision, current.revision);
    QVERIFY(Display::validateBrightnessJoin(current, *published).accepted);
    for (const Display::OutputBrightness &row : published->outputs) {
        QCOMPARE(row, (Display::OutputBrightness{.stableId = row.stableId}));
    }

    authority.devicesObserved(devices(), &current);
    QVERIFY(authority.takePublicationChanged());
    QVERIFY(!authority.takePublicationChanged());
    published = authority.snapshot();
    QCOMPARE(published->revision, quint64{2});
    const QList<Display::OutputBrightness> expected{
        {.stableId = kExternal, .capable = true, .observed = true, .value = 6'000},
        {.stableId = kInternal, .capable = true, .observed = true, .value = 3'000}};
    QCOMPARE(published->outputs, expected);
    QVERIFY(Display::validateBrightnessJoin(current, *published).accepted);

    authority.devicesObserved(devices(), &current);
    QVERIFY(!authority.takePublicationChanged());
    QCOMPARE(authority.snapshot()->revision, quint64{2});
    QVERIFY(port.brightnessRequests.isEmpty());
}

void DisplayServiceBrightnessTest::clearsOnDeviceAndOwnerLoss()
{
    AuthorityFixture f;
    DeviceBrightnessFrame removed = devices();
    removed.devices.removeFirst();
    f.authority.devicesObserved(removed, &f.current);
    QVERIFY(f.authority.takePublicationChanged());
    QCOMPARE(f.authority.snapshot()->outputs.at(0), (Display::OutputBrightness{.stableId = kExternal}));
    QVERIFY(f.authority.snapshot()->outputs.at(1).observed);

    f.authority.devicesObserved({}, &f.current);
    QVERIFY(f.authority.takePublicationChanged());
    for (const Display::OutputBrightness &row : f.authority.snapshot()->outputs) {
        QCOMPARE(row, (Display::OutputBrightness{.stableId = row.stableId}));
    }

    f.authority.refresh(nullptr);
    QVERIFY(f.authority.snapshot() == nullptr);
    QVERIFY(f.authority.takePublicationChanged());

    // A replacement epoch starts its own revision sequence.
    Display::Snapshot replacement = topology(1);
    replacement.serviceEpoch = QStringLiteral("epoch-b");
    f.authority.devicesObserved(devices(6), &replacement);
    QCOMPARE(f.authority.snapshot()->serviceEpoch, QStringLiteral("epoch-b"));
    QCOMPARE(f.authority.snapshot()->revision, quint64{1});
    QVERIFY(f.authority.snapshot()->outputs.at(0).capable);
}

void DisplayServiceBrightnessTest::joinsOnlyExactConnectorAndUuid()
{
    AuthorityFixture f;
    const auto externalAfter = [&f](const DeviceBrightnessFrame &frame) {
        f.authority.devicesObserved(frame, &f.current);
        return f.authority.snapshot()->outputs.at(0);
    };

    DeviceBrightnessFrame frame = devices();
    frame.devices[0].runtimeUuid = QStringLiteral("uuid-other");
    QVERIFY(!externalAfter(frame).capable);

    frame = devices();
    frame.devices.push_back(
        device(QStringLiteral("DP-1"), QStringLiteral("uuid-dp-shadow"), 1'000));
    QVERIFY(!externalAfter(frame).capable);

    frame = devices();
    frame.devices.push_back(device(QStringLiteral("DP-2"), QStringLiteral("uuid-dp"), 1'000));
    QVERIFY(!externalAfter(frame).capable);

    frame = devices();
    frame.devices[0].value = Display::kMaxBrightness + 1;
    QCOMPARE(externalAfter(frame),
             (Display::OutputBrightness{.stableId = kExternal, .capable = true}));

    QCOMPARE(externalAfter(devices()).value, quint32{6'000});
}

void DisplayServiceBrightnessTest::pinsRevisionToTheJoinedTopology()
{
    AuthorityFixture f;
    const Display::BrightnessRequest pinnedBefore =
        requestFor(*f.authority.snapshot(), kExternal, 1'000);
    f.current = topology(5);
    f.authority.refresh(&f.current);
    QCOMPARE(f.authority.snapshot()->revision, quint64{2});
    QCOMPARE(f.authority.snapshot()->topologyRevision, quint64{5});
    const BrightnessRequestResult stale =
        f.authority.request(pinnedBefore, &f.current, MachineState::Ready, SafetyState::Safe);
    QVERIFY(stale.final);
    QINDAQT_VERIFY_IMMEDIATE(stale.operation, OperationStatus::Rejected,
                             ErrorCode::StaleRevision, "stale-revision");

    const Display::BrightnessRequest pinnedAtJoin =
        requestFor(*f.authority.snapshot(), kExternal, 1'000);
    f.authority.devicesObserved(withExternalValue(7'000), &f.current);
    QCOMPARE(f.authority.snapshot()->revision, quint64{3});
    for (const auto &[epoch, revision] : {std::pair{QStringLiteral("epoch-a"), quint64{4}},
                                          std::pair{QStringLiteral("epoch-b"), quint64{2}}}) {
        Display::BrightnessRequest hostile = pinnedAtJoin;
        hostile.baseEpoch = epoch;
        hostile.baseRevision = revision;
        const BrightnessRequestResult refused =
            f.authority.request(hostile, &f.current, MachineState::Ready, SafetyState::Safe);
        QINDAQT_VERIFY_IMMEDIATE(refused.operation, OperationStatus::Rejected,
                                 ErrorCode::StaleRevision, "stale-revision");
    }
    QVERIFY(f.port.brightnessRequests.isEmpty());

    // A value-only republish since the pinned revision is last-writer-wins.
    const BrightnessRequestResult accepted =
        f.authority.request(pinnedAtJoin, &f.current, MachineState::Ready, SafetyState::Safe);
    QVERIFY(!accepted.final);
    QCOMPARE(accepted.operation.status, OperationStatus::Accepted);
    QCOMPARE(f.port.brightnessRequests.size(), 1);
}

void DisplayServiceBrightnessTest::refusesInadmissibleRequestsWithoutWriting()
{
    using Arrange = std::function<void(AuthorityFixture &)>;
    struct Case {
        const char *name;
        Arrange arrange;
        QString stableId;
        MachineState state;
        SafetyState safety;
        ErrorCode error;
        const char *diagnostic;
    };
    const auto mutateExternal = [](const std::function<void(DeviceBrightness &)> &mutate) {
        return [mutate](AuthorityFixture &f) {
            DeviceBrightnessFrame frame = devices();
            mutate(frame.devices[0]);
            f.authority.devicesObserved(frame, &f.current);
        };
    };
    const Arrange none = [](AuthorityFixture &) {};
    const QList<Case> cases{
        {"unknown", none, QStringLiteral("conn:DP-9"), MachineState::Ready, SafetyState::Safe,
         ErrorCode::InvalidCandidate, "unknown-output"},
        {"ambiguous", [](AuthorityFixture &f) { f.current.outputs[0].ambiguousIdentity = true; },
         kExternal, MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate,
         "ambiguous-output"},
        {"internal", none, kInternal, MachineState::Ready, SafetyState::Safe,
         ErrorCode::InvalidCandidate, "internal-output"},
        {"disabled", [](AuthorityFixture &f) { f.current.outputs[0].enabled = false; }, kExternal,
         MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate, "output-disabled"},
        {"replica",
         [](AuthorityFixture &f) { f.current.outputs[0].replicationSourceStableId = kInternal; },
         kExternal, MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate,
         "replica-output"},
        {"non-capable", mutateExternal([](DeviceBrightness &d) { d.capable = false; }), kExternal,
         MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate,
         "brightness-unsupported"},
        {"device-disabled", mutateExternal([](DeviceBrightness &d) { d.enabled = false; }),
         kExternal, MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate,
         "output-disabled"},
        {"unobserved",
         mutateExternal([](DeviceBrightness &d) {
             d.observed = false;
             d.value = 0;
         }),
         kExternal, MachineState::Ready, SafetyState::Safe, ErrorCode::InvalidCandidate,
         "brightness-unobserved"},
        {"staged", none, kExternal, MachineState::Staged, SafetyState::Safe,
         ErrorCode::TransactionActive, "transaction-active"},
        {"awaiting-confirmation", none, kExternal, MachineState::AwaitingConfirmation,
         SafetyState::Safe, ErrorCode::TransactionActive, "transaction-active"},
        {"locked", none, kExternal, MachineState::Ready, SafetyState::Locked, ErrorCode::Locked,
         "locked"},
        {"safety-unknown", none, kExternal, MachineState::Ready, SafetyState::Unknown,
         ErrorCode::CompositorUnavailable, "mutation-authority-unavailable"},
    };
    for (const Case &c : cases) {
        AuthorityFixture f;
        c.arrange(f);
        const BrightnessRequestResult result = f.request(1'000, c.stableId, c.state, c.safety);
        QVERIFY2(result.available && result.final, c.name);
        QVERIFY2(result.operation.diagnostic == QString::fromLatin1(c.diagnostic),
                 qPrintable(QStringLiteral("%1: %2").arg(QString::fromLatin1(c.name),
                                                         result.operation.diagnostic)));
        QINDAQT_VERIFY_IMMEDIATE(result.operation, OperationStatus::Rejected, c.error,
                                 c.diagnostic);
        QVERIFY2(f.port.brightnessRequests.isEmpty() && !f.authority.pending(), c.name);
    }

    AuthorityFixture f;
    Display::BrightnessRequest hostile = requestFor(*f.authority.snapshot(), kExternal, 0);
    hostile.value = Display::kMaxBrightness + 1;
    const BrightnessRequestResult invalid =
        f.authority.request(hostile, &f.current, MachineState::Ready, SafetyState::Safe);
    QINDAQT_VERIFY_IMMEDIATE(invalid.operation, OperationStatus::Rejected,
                             ErrorCode::InvalidCandidate, "invalid-brightness-value");
    const BrightnessRequestResult noOp = f.request(6'000);
    QVERIFY(noOp.final);
    QINDAQT_VERIFY_IMMEDIATE(noOp.operation, OperationStatus::Succeeded, ErrorCode::None, "no-op");
    QVERIFY(f.port.brightnessRequests.isEmpty());
}

void DisplayServiceBrightnessTest::appliedRequiresAcknowledgementAndObservation()
{
    AuthorityFixture f;
    const BrightnessRequestResult accepted = f.request(2'500);
    QVERIFY(accepted.available);
    QVERIFY(!accepted.final);
    QVERIFY(accepted.requestId != 0);
    QCOMPARE(accepted.operation.status, OperationStatus::Accepted);
    const QList<BrightnessApplyRequest> expected{{.requestId = accepted.requestId,
                                                  .ownerGeneration = 5,
                                                  .connectorName = QStringLiteral("DP-1"),
                                                  .runtimeUuid = QStringLiteral("uuid-dp"),
                                                  .value = 2'500}};
    QCOMPARE(f.port.brightnessRequests, expected);
    const quint64 initiating = f.authority.snapshot()->revision;

    // Neither the acknowledgement nor an unrelated republish is proof.
    f.authority.completed(accepted.requestId, BrightnessApplyOutcome::Applied);
    QVERIFY(f.authority.takeFinishes().isEmpty());
    DeviceBrightnessFrame unrelated = devices();
    unrelated.devices[1].value = 2'000;
    f.authority.devicesObserved(unrelated, &f.current);
    QVERIFY(f.authority.takeFinishes().isEmpty());
    QVERIFY(f.authority.pending());

    f.authority.devicesObserved(withExternalValue(2'500), &f.current);
    QVERIFY(f.authority.takePublicationChanged());
    QList<BrightnessFinish> finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QCOMPARE(finishes.at(0).requestId, accepted.requestId);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Succeeded,
                             ErrorCode::None, "");
    QCOMPARE(finishes.at(0).operation.initiatingRevision, initiating);
    QCOMPARE(finishes.at(0).operation.observedRevision, f.authority.snapshot()->revision);
    QVERIFY(finishes.at(0).operation.observedRevision > initiating);

    // KWin order: the device republish precedes the acknowledgement.
    const BrightnessRequestResult second = f.request(9'000);
    QVERIFY(!second.final);
    f.authority.devicesObserved(withExternalValue(9'000), &f.current);
    QVERIFY(f.authority.takeFinishes().isEmpty());
    f.authority.completed(second.requestId, BrightnessApplyOutcome::Applied);
    finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Succeeded,
                             ErrorCode::None, "");
    f.authority.completed(second.requestId, BrightnessApplyOutcome::Applied);
    QVERIFY(f.authority.takeFinishes().isEmpty());
}

void DisplayServiceBrightnessTest::serializesAndTypesPortFailures()
{
    AuthorityFixture f;
    const BrightnessRequestResult first = f.request(2'500);
    QVERIFY(!first.final);
    const BrightnessRequestResult second = f.request(1'000);
    QVERIFY(second.final);
    QINDAQT_VERIFY_IMMEDIATE(second.operation, OperationStatus::Busy,
                             ErrorCode::TransactionActive, "brightness-request-pending");
    QCOMPARE(f.port.brightnessRequests.size(), 1);

    f.authority.completed(first.requestId, BrightnessApplyOutcome::Rejected);
    QList<BrightnessFinish> finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Rejected,
                             ErrorCode::CompositorRejected, "compositor-rejected");

    const BrightnessRequestResult third = f.request(1'000);
    f.authority.completed(third.requestId, BrightnessApplyOutcome::TransportUncertain);
    finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                             ErrorCode::CompositorUnavailable, "transport-uncertain");

    struct PortCase {
        BrightnessSubmitStatus submit;
        OperationStatus status;
        ErrorCode error;
        const char *diagnostic;
    };
    const QList<PortCase> cases{
        {BrightnessSubmitStatus::Busy, OperationStatus::Busy, ErrorCode::TransactionActive,
         "compositor-busy"},
        {BrightnessSubmitStatus::Unavailable, OperationStatus::Rejected,
         ErrorCode::CompositorUnavailable, "compositor-unavailable"},
        {BrightnessSubmitStatus::Unsupported, OperationStatus::Rejected,
         ErrorCode::CompositorRejected, "compositor-unsupported"},
        {BrightnessSubmitStatus::Malformed, OperationStatus::Rejected,
         ErrorCode::MalformedPayload, "compositor-malformed"},
    };
    for (const PortCase &c : cases) {
        f.port.brightnessSubmitStatus = c.submit;
        const BrightnessRequestResult refused = f.request(1'500);
        QVERIFY(refused.final);
        QINDAQT_VERIFY_IMMEDIATE(refused.operation, c.status, c.error, c.diagnostic);
        QVERIFY(!f.authority.pending());
    }
}

void DisplayServiceBrightnessTest::neverReplaysAfterTopologyOrOwnerChange()
{
    {
        AuthorityFixture f;
        const BrightnessRequestResult accepted = f.request(2'500);
        f.current = topology(5);
        f.authority.refresh(&f.current);
        const QList<BrightnessFinish> finishes = f.authority.takeFinishes();
        QCOMPARE(finishes.size(), 1);
        QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                                 ErrorCode::TopologyChanged, "topology-changed");
        f.authority.completed(accepted.requestId, BrightnessApplyOutcome::Applied);
        f.authority.devicesObserved(withExternalValue(2'500), &f.current);
        QVERIFY(f.authority.takeFinishes().isEmpty());
        QCOMPARE(f.port.brightnessRequests.size(), 1);
    }
    {
        AuthorityFixture f;
        const BrightnessRequestResult accepted = f.request(2'500);
        f.authority.completed(accepted.requestId, BrightnessApplyOutcome::Applied);
        f.authority.devicesObserved(withExternalValue(2'500, 6), &f.current);
        const QList<BrightnessFinish> finishes = f.authority.takeFinishes();
        QCOMPARE(finishes.size(), 1);
        QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                                 ErrorCode::CompositorUnavailable, "compositor-owner-changed");
        QCOMPARE(f.port.brightnessRequests.size(), 1);
    }
    {
        AuthorityFixture f;
        (void)f.request(2'500);
        f.authority.refresh(nullptr);
        const QList<BrightnessFinish> finishes = f.authority.takeFinishes();
        QCOMPARE(finishes.size(), 1);
        QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                                 ErrorCode::CompositorUnavailable, "service-lineage-lost");
    }
}

void DisplayServiceBrightnessTest::deadlineMakesUnobservedApplyUncertain()
{
    AuthorityFixture f;
    const BrightnessRequestResult accepted = f.request(2'500);
    QCOMPARE(f.authority.deadlineMonotonicMilliseconds(), f.clock.now + 500);
    f.authority.completed(accepted.requestId, BrightnessApplyOutcome::Applied);
    f.clock.now += 499;
    f.authority.tick();
    QVERIFY(f.authority.takeFinishes().isEmpty());
    f.clock.now += 1;
    f.authority.tick();
    QList<BrightnessFinish> finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                             ErrorCode::Timeout, "brightness-observation-timeout");
    QCOMPARE(f.authority.deadlineMonotonicMilliseconds(), quint64{0});

    (void)f.request(2'600);
    f.clock.now += 500;
    f.authority.tick();
    finishes = f.authority.takeFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                             ErrorCode::Timeout, "brightness-apply-timeout");
}

QTEST_GUILESS_MAIN(DisplayServiceBrightnessTest)
#include "tst_display_service_brightness.moc"
