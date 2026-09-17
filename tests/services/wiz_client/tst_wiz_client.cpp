// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fakewiztransport.h"

#include <qindaqt/services/wiz_client/wiz_client.h>
#include <qindaqt/services/wiz_protocol/wiz_messages.h>

#include <QtTest/QtTest>

using namespace QindaQt::Wiz;
using QindaQt::Wiz::Testing::FakeWizClock;
using QindaQt::Wiz::Testing::FakeWizTransport;

namespace
{

const QString deviceMac = QStringLiteral("d8a011769356");
const QString deviceAddress = QStringLiteral("10.0.0.234");

[[nodiscard]] QByteArray pilotReply(const bool on = true)
{
    return QStringLiteral(
               R"({"method":"getPilot","result":{"mac":"%1","state":%2,"dimming":50,"sceneId":0,"r":0,"g":0,"b":255,"c":0,"w":0,"rssi":-50}})")
        .arg(deviceMac, on ? QStringLiteral("true") : QStringLiteral("false"))
        .toUtf8();
}

[[nodiscard]] QByteArray systemReply()
{
    return QStringLiteral(
               R"({"method":"getSystemConfig","result":{"mac":"%1","moduleName":"ESP25_SHRGB_01","fwVersion":"1.38.0"}})")
        .arg(deviceMac)
        .toUtf8();
}

// AGENT-NOTE: copied from firmware 1.38.0, which answers getModelConfig
// without a `mac` member. Adding one here would make the test pass while the
// real device left capabilities incomplete.
[[nodiscard]] QByteArray modelReply()
{
    return QByteArrayLiteral(
        R"({"method":"getModelConfig","env":"pro","result":{"devTotal":1,"headTotal":1,"minDimLevel":1,"lightType":1,"cctRange":[2200,2700,6500,6500]}})");
}

constexpr auto setPilotAck = R"({"method":"setPilot","result":{"success":true}})";
constexpr auto setPilotError =
    R"({"method":"setPilot","error":{"code":-32602,"message":"Invalid params"}})";

// Brings a client to the state a test needs: started, with one fully
// interrogated luminaire.
class Fixture
{
public:
    Fixture()
        : client(&transport, &clock)
    {
        client.setAutomaticPolling(false);
        client.start();
        transport.deliver(deviceAddress, pilotReply());
        transport.deliver(deviceAddress, systemReply());
        transport.deliver(deviceAddress, modelReply());
        transport.unicasts.clear();
    }

    FakeWizTransport transport;
    FakeWizClock clock;
    WizClient client;
};

} // namespace

class WizClientTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void broadcastsDiscoveryOnStart();
    void refusesToStartWithoutATransport();
    void interrogatesNewDevices();
    void completesCapabilitiesFromAMacLessModelReply();
    void dispatchCompletesOnAcknowledgement();
    void deviceErrorFailsTheOperation();
    void unansweredOperationRetriesOnceThenReportsUncertain();
    void serializesQueuedOperations();
    void rejectsUnknownDeviceAsynchronously();
    void refusesUnsupportedCapability();
    void missedPollRoundsDegradeReachability();
    void aReturningLightIsInterrogatedAgain();
    void stopCompletesQueuedWork();
    void transportFailureEndsAuthority();
    void doesNotSubscribeWithoutAListenerAddress();
};

void WizClientTests::broadcastsDiscoveryOnStart()
{
    FakeWizTransport transport;
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();

    QCOMPARE(client.state(), ClientState::Ready);
    QCOMPARE(transport.broadcasts.size(), 1);
    const auto message = decodeMessage(transport.broadcasts.first());
    QVERIFY(message.has_value());
    QCOMPARE(message->method, Method::Registration);
    QVERIFY(transport.broadcasts.first().contains("\"register\":true"));
    QVERIFY(transport.broadcasts.first().contains("10.0.0.7"));
}

void WizClientTests::refusesToStartWithoutATransport()
{
    FakeWizTransport transport;
    transport.startSucceeds = false;
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();

    QCOMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(client.snapshot().availability, Availability::Unavailable);
    QVERIFY(transport.broadcasts.isEmpty());
}

void WizClientTests::interrogatesNewDevices()
{
    FakeWizTransport transport;
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();
    transport.deliver(deviceAddress, pilotReply());

    // A device that has only answered a pilot is asked what it is.
    QCOMPARE(transport.unicastCount("getSystemConfig"), 1);
    QCOMPARE(transport.unicastCount("getModelConfig"), 1);

    transport.deliver(deviceAddress, systemReply());
    transport.deliver(deviceAddress, modelReply());
    const Snapshot snapshot = client.snapshot();
    QCOMPARE(snapshot.devices.size(), 1);
    QVERIFY(snapshot.devices.first().capabilitiesKnown);
    QVERIFY(snapshot.devices.first().features.testFlag(Feature::Color));
}

void WizClientTests::completesCapabilitiesFromAMacLessModelReply()
{
    Fixture fixture;
    const Snapshot snapshot = fixture.client.snapshot();
    QCOMPARE(snapshot.devices.size(), 1);
    const Device &device = snapshot.devices.first();
    QVERIFY(device.capabilitiesKnown);
    // The device's own floor, not the conservative default, proves the model
    // reply was attributed rather than dropped.
    QCOMPARE(device.dimming.minimumPercent, 1);
    QCOMPARE(device.temperature.minimumKelvin, 2200);

    OperationRequest request;
    request.kind = OperationKind::SetBrightness;
    request.targetMac = deviceMac;
    request.state.setDimming = true;
    request.state.dimmingPercent = 5;
    static_cast<void>(fixture.client.dispatch(request));
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 1);
    QVERIFY(fixture.transport.unicasts.last().datagram.contains("\"dimming\":5"));
}

void WizClientTests::dispatchCompletesOnAcknowledgement()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetBrightness;
    request.targetMac = deviceMac;
    request.state.setDimming = true;
    request.state.dimmingPercent = 40;
    const quint64 id = fixture.client.dispatch(request);

    QCOMPARE(fixture.transport.unicastCount("setPilot"), 1);
    QVERIFY(fixture.transport.unicasts.last().datagram.contains("\"dimming\":40"));
    QCOMPARE(fixture.transport.unicasts.last().address, deviceAddress);

    fixture.transport.deliver(deviceAddress, QByteArray(setPilotAck));
    QCOMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), id);
    const auto result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.kind, OperationKind::SetBrightness);
    // The client asks for the new truth instead of assuming it.
    QCOMPARE(fixture.transport.unicastCount("getPilot"), 1);
    QVERIFY(!fixture.client.operationPending());
}

void WizClientTests::deviceErrorFailsTheOperation()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetScene;
    request.targetMac = deviceMac;
    request.state.setScene = true;
    request.state.sceneId = 4;
    static_cast<void>(fixture.client.dispatch(request));
    fixture.transport.deliver(deviceAddress, QByteArray(setPilotError));

    QCOMPARE(completed.size(), 1);
    const auto result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Failed);
    QCOMPARE(result.diagnostic, QStringLiteral("Invalid params"));
}

void WizClientTests::unansweredOperationRetriesOnceThenReportsUncertain()
{
    Fixture fixture;
    fixture.client.setRequestTimeoutMilliseconds(1000);
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetPower;
    request.targetMac = deviceMac;
    request.state.setPower = true;
    request.state.on = false;
    static_cast<void>(fixture.client.dispatch(request));
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 1);

    fixture.clock.now += 1500;
    fixture.client.tick();
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 2);
    QVERIFY(completed.isEmpty());

    fixture.clock.now += 1500;
    fixture.client.tick();
    QCOMPARE(completed.size(), 1);
    const auto result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);

    // AGENT-GUARD: an uncertain change is never replayed on its own.
    fixture.clock.now += 10000;
    fixture.client.tick();
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 2);
}

void WizClientTests::serializesQueuedOperations()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest first;
    first.kind = OperationKind::SetBrightness;
    first.targetMac = deviceMac;
    first.state.setDimming = true;
    first.state.dimmingPercent = 30;
    OperationRequest second = first;
    second.state.dimmingPercent = 70;

    static_cast<void>(fixture.client.dispatch(first));
    static_cast<void>(fixture.client.dispatch(second));
    // One light, one datagram at a time.
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 1);
    QCOMPARE(fixture.client.queuedOperationCount(), 2);

    fixture.transport.deliver(deviceAddress, QByteArray(setPilotAck));
    QCOMPARE(completed.size(), 1);
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 2);
    QVERIFY(fixture.transport.unicasts.last().datagram.contains("\"dimming\":70"));

    fixture.transport.deliver(deviceAddress, QByteArray(setPilotAck));
    QCOMPARE(completed.size(), 2);
    QVERIFY(!fixture.client.operationPending());
}

void WizClientTests::rejectsUnknownDeviceAsynchronously()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetPower;
    request.targetMac = QStringLiteral("aabbccddeeff");
    request.state.setPower = true;
    request.state.on = true;
    const quint64 id = fixture.client.dispatch(request);

    // Completion never arrives before dispatch returns.
    QVERIFY(completed.isEmpty());
    QVERIFY(completed.wait(500));
    QCOMPARE(completed.first().at(0).toULongLong(), id);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Rejected);
    QCOMPARE(fixture.transport.unicastCount("setPilot"), 0);
}

void WizClientTests::refusesUnsupportedCapability()
{
    FakeWizTransport transport;
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();
    // A tunable-white luminaire: no colour channels.
    transport.deliver(deviceAddress, pilotReply());
    transport.deliver(deviceAddress,
                      QStringLiteral(
                          R"({"method":"getSystemConfig","result":{"mac":"%1","moduleName":"ESP56_SHTW3_01","fwVersion":"1.38.0"}})")
                          .arg(deviceMac)
                          .toUtf8());
    transport.deliver(deviceAddress, modelReply());
    transport.unicasts.clear();

    QSignalSpy completed(&client, &WizClient::operationCompleted);
    OperationRequest request;
    request.kind = OperationKind::SetColor;
    request.targetMac = deviceMac;
    request.state.setColor = true;
    request.state.red = 255;
    static_cast<void>(client.dispatch(request));

    QVERIFY(completed.wait(500));
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Unsupported);
    QCOMPARE(transport.unicastCount("setPilot"), 0);
}

void WizClientTests::missedPollRoundsDegradeReachability()
{
    Fixture fixture;
    fixture.client.setPollIntervalMilliseconds(1000);

    for (int round = 0; round < 6; ++round) {
        fixture.clock.now += 1200;
        fixture.client.tick();
    }
    QCOMPARE(fixture.client.snapshot().devices.first().reachability,
             Reachability::Unreachable);

    fixture.transport.deliver(deviceAddress, pilotReply());
    QCOMPARE(fixture.client.snapshot().devices.first().reachability,
             Reachability::Online);
}

void WizClientTests::aReturningLightIsInterrogatedAgain()
{
    FakeWizTransport transport;
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.setPollIntervalMilliseconds(1000);
    client.start();

    // The light answers a pilot but is still joining the network, so its
    // capability answers never arrive and the interrogation budget runs out.
    transport.deliver(deviceAddress, pilotReply());
    for (int round = 0; round < 8; ++round) {
        clock.now += 1200;
        client.tick();
    }
    QVERIFY(!client.snapshot().devices.first().capabilitiesKnown);
    const int exhausted = transport.unicastCount("getModelConfig");
    clock.now += 1200;
    client.tick();
    QCOMPARE(transport.unicastCount("getModelConfig"), exhausted);

    // It comes back. A fresh budget is what lets its controls ever work.
    transport.deliver(deviceAddress, pilotReply());
    QVERIFY(transport.unicastCount("getModelConfig") > exhausted);
    transport.deliver(deviceAddress, systemReply());
    transport.deliver(deviceAddress, modelReply());
    QVERIFY(client.snapshot().devices.first().capabilitiesKnown);
}

void WizClientTests::stopCompletesQueuedWork()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetPower;
    request.targetMac = deviceMac;
    request.state.setPower = true;
    request.state.on = true;
    static_cast<void>(fixture.client.dispatch(request));
    fixture.client.stop();

    QCOMPARE(completed.size(), 1);
    const auto result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("client-stopped"));
    QCOMPARE(fixture.client.state(), ClientState::Stopped);
}

void WizClientTests::transportFailureEndsAuthority()
{
    Fixture fixture;
    QSignalSpy completed(&fixture.client, &WizClient::operationCompleted);

    OperationRequest request;
    request.kind = OperationKind::SetPower;
    request.targetMac = deviceMac;
    request.state.setPower = true;
    request.state.on = true;
    static_cast<void>(fixture.client.dispatch(request));
    fixture.transport.fail(QStringLiteral("socket closed"));

    QCOMPARE(fixture.client.state(), ClientState::Unavailable);
    QCOMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Uncertain);
}

void WizClientTests::doesNotSubscribeWithoutAListenerAddress()
{
    FakeWizTransport transport;
    transport.listener.clear();
    FakeWizClock clock;
    WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();

    QCOMPARE(transport.broadcasts.size(), 1);
    // A light is never asked to push notifications nobody can receive.
    QVERIFY(transport.broadcasts.first().contains("\"register\":false"));
}

QTEST_MAIN(WizClientTests)
#include "tst_wiz_client.moc"
