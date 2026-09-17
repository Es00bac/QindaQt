// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/smart_lights_applet/smart_lights_request_state.h>

#include <QtTest/QtTest>

using namespace QindaQt::Shell::SmartLightsApplet;
using namespace QindaQt::Wiz;

namespace
{

const QString deviceMac = QStringLiteral("d8a011769356");

[[nodiscard]] Snapshot snapshotWithDevice(const Reachability reachability,
                                          const bool capabilitiesKnown = true)
{
    Device device;
    device.identity.mac = deviceMac;
    device.identity.moduleName = QStringLiteral("ESP25_SHRGB_01");
    device.label = QStringLiteral("Reading lamp");
    device.features |= Feature::Power;
    device.features |= Feature::Dimming;
    device.features |= Feature::Scenes;
    device.dimming = {1, 100};
    device.capabilitiesKnown = capabilitiesKnown;
    device.reachability = reachability;
    device.pilotKnown = true;
    device.pilot.on = true;

    Snapshot snapshot;
    snapshot.availability = Availability::Ready;
    snapshot.epoch = 7;
    snapshot.revision = 42;
    snapshot.devices.append(device);
    return snapshot;
}

[[nodiscard]] OperationRequest brightnessRequest()
{
    OperationRequest request;
    request.kind = OperationKind::SetBrightness;
    request.targetMac = deviceMac;
    request.state.setDimming = true;
    request.state.dimmingPercent = 40;
    return request;
}

} // namespace

class SmartLightsRequestStateTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutControlGrantNothingIsAdmitted();
    void unreachableDeviceIsRefusedWithItsName();
    void unsupportedIntentIsRefusedBeforeDispatch();
    void admitsAValidIntentWithItsLineage();
    void foreignResultLeavesTheRequestPending();
    void uncertainResultIsReportedNotReplayed();
    void losingAuthorityEndsAPendingRequest();
};

void SmartLightsRequestStateTests::withoutControlGrantNothingIsAdmitted()
{
    const RequestState state = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), brightnessRequest(), false);
    QVERIFY(!state.pending());
    QCOMPARE(state.phase, RequestPhase::Failed);
    QVERIFY(!state.feedback.isEmpty());
}

void SmartLightsRequestStateTests::unreachableDeviceIsRefusedWithItsName()
{
    const RequestState state = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Unreachable), brightnessRequest(), true);
    QVERIFY(!state.pending());
    QVERIFY(state.feedback.contains(QStringLiteral("Reading lamp")));
}

void SmartLightsRequestStateTests::unsupportedIntentIsRefusedBeforeDispatch()
{
    OperationRequest colour;
    colour.kind = OperationKind::SetColor;
    colour.targetMac = deviceMac;
    colour.state.setColor = true;
    colour.state.red = 255;

    // This luminaire has no colour channels; admission refuses it here so an
    // enabled control never reaches the client.
    const RequestState state = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), colour, true);
    QVERIFY(!state.pending());
    QVERIFY(!state.feedback.isEmpty());
}

void SmartLightsRequestStateTests::admitsAValidIntentWithItsLineage()
{
    const RequestState state = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), brightnessRequest(), true);
    QVERIFY(state.pending());
    QCOMPARE(state.initiatingEpoch, quint64{7});
    QCOMPARE(state.initiatingRevision, quint64{42});
    QVERIFY(state.feedback.isEmpty());
}

void SmartLightsRequestStateTests::foreignResultLeavesTheRequestPending()
{
    const RequestState pending = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), brightnessRequest(), true);

    OperationResult other;
    other.kind = OperationKind::SetPower;
    other.targetMac = deviceMac;
    other.status = OperationStatus::Succeeded;
    other.initiatingEpoch = 7;
    QVERIFY(applySmartLightResult(pending, other).pending());

    OperationResult staleEpoch;
    staleEpoch.kind = OperationKind::SetBrightness;
    staleEpoch.targetMac = deviceMac;
    staleEpoch.status = OperationStatus::Succeeded;
    staleEpoch.initiatingEpoch = 6;
    QVERIFY(applySmartLightResult(pending, staleEpoch).pending());

    OperationResult mine;
    mine.kind = OperationKind::SetBrightness;
    mine.targetMac = deviceMac;
    mine.status = OperationStatus::Succeeded;
    mine.initiatingEpoch = 7;
    const RequestState done = applySmartLightResult(pending, mine);
    QCOMPARE(done.phase, RequestPhase::Succeeded);
    QVERIFY(done.feedback.isEmpty());
}

void SmartLightsRequestStateTests::uncertainResultIsReportedNotReplayed()
{
    const RequestState pending = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), brightnessRequest(), true);

    OperationResult uncertain;
    uncertain.kind = OperationKind::SetBrightness;
    uncertain.targetMac = deviceMac;
    uncertain.status = OperationStatus::Uncertain;
    uncertain.initiatingEpoch = 7;

    const RequestState state = applySmartLightResult(pending, uncertain);
    QCOMPARE(state.phase, RequestPhase::Uncertain);
    QVERIFY(!state.feedback.isEmpty());
    // Applying the same result again cannot resurrect the request.
    QCOMPARE(applySmartLightResult(state, uncertain), state);
}

void SmartLightsRequestStateTests::losingAuthorityEndsAPendingRequest()
{
    const RequestState pending = beginSmartLightRequest(
        snapshotWithDevice(Reachability::Online), brightnessRequest(), true);

    QCOMPARE(observeSmartLightAuthority(pending, true, 7), pending);
    QCOMPARE(observeSmartLightAuthority(pending, false, 7).phase, RequestPhase::Uncertain);
    QCOMPARE(observeSmartLightAuthority(pending, true, 8).phase, RequestPhase::Uncertain);
}

QTEST_MAIN(SmartLightsRequestStateTests)
#include "tst_smart_lights_request_state.moc"
