// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_controller.h"
#include "network_applet_test_support.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt;
using namespace QindaQt::Shell::NetworkApplet;
using namespace QindaQt::Shell::NetworkApplet::TestSupport;
using Network::OperationKind;
using Network::OperationStatus;

namespace
{

NetworkAppletController::Timing fastApplet()
{
    NetworkAppletController::Timing timing;
    timing.readbackPollMilliseconds = 20;
    timing.confirmMilliseconds = 400;
    timing.connectConfirmMilliseconds = 400;
    return timing;
}

struct Fixture final {
    qint64 now = 3'000;
    FakeNetworkTransport transport;
    Network::Client::NetworkClient client;
    NetworkAppletController controller;

    explicit Fixture(const Network::Snapshot &initial = appletSnapshot(),
                     const bool control = true)
        : client(transport, [this] { return now; }, fastTiming())
        , controller(&client, true, control, fastApplet())
    {
        transport.setSnapshot(initial);
        const bool started = client.start();
        Q_ASSERT(started);
        Q_UNUSED(started);
        transport.announceOwner(initial.owner);
    }

    QVariantMap pointRow(const QString &label) const
    {
        for (const QVariant &value : controller.accessPointRows()) {
            if (value.toMap().value(QStringLiteral("label")).toString() == label)
                return value.toMap();
        }
        return {};
    }
};

} // namespace

class NetworkAppletControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsTheReadySnapshot();
    void savedNetworkConnectConfirmsOnlyAfterNewerSnapshot();
    void newNetworkSendsOnlyTheOpaqueAccessPointId();
    void serviceRefusalAndLocalRefusal();
    void uncertainTransportFailureIsNeverReplayed();
    void ownerReplacementRetiresThePendingRequest();
    void ownerLossClearsAllRows();
    void radioSwitchConfirmsAndHardwareRefuses();
    void noWifiDeviceHasNoNetworkList();
    void serviceAbsentIsUnavailable();
    void unconfirmedChangeBecomesUncertainAtTheDeadline();
    void rescanAndDisconnect();
    void controlDenialAndSettingsLaunch();
};

void NetworkAppletControllerTests::projectsTheReadySnapshot()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QCOMPARE(f.controller.indicator(), QStringLiteral("wireless"));
    QCOMPARE(f.controller.iconName(), QStringLiteral("network-wireless-signal-excellent"));
    QCOMPARE(f.controller.accessPointRows().size(), 4);
    QCOMPARE(f.controller.connectionRows().size(), 1);
    QCOMPARE(f.controller.radioRows().size(), 1);
    QVERIFY(f.controller.scanAvailable());
    QVERIFY(!f.controller.operationPending());
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("idle"));
    QVERIFY(!f.controller.feedbackPresent());
}

void NetworkAppletControllerTests::savedNetworkConnectConfirmsOnlyAfterNewerSnapshot()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestConnect(cafePoint()));
    QCOMPARE(f.transport.operations.size(), 1);
    QCOMPARE(f.transport.operations.last().kind, OperationKind::ConnectKnownNetwork);
    QCOMPARE(f.transport.operations.last().parameters,
             QVariantMap({{QStringLiteral("knownNetworkId"), cafeId()}}));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("pending"));
    QVERIFY(f.controller.operationPending());
    QVERIFY(f.pointRow(QStringLiteral("Cafe")).value(QStringLiteral("pending")).toBool());
    // Every action is fenced while a request is in flight.
    QVERIFY(!f.pointRow(QStringLiteral("Guest")).value(QStringLiteral("canConnect")).toBool());
    QVERIFY(!f.controller.requestScan());
    QCOMPARE(f.transport.operations.size(), 1);
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("pending"));

    f.transport.finishLast(operationResult(OperationKind::ConnectKnownNetwork,
                                           OperationStatus::Succeeded));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("confirming"));
    QVERIFY(f.controller.feedback().startsWith(QStringLiteral("Connecting to Cafe")));
    QVERIFY(!f.pointRow(QStringLiteral("Cafe")).value(QStringLiteral("active")).toBool());

    Network::Snapshot connected = appletSnapshot(2);
    connected.activeConnections = {{QStringLiteral("wlan0"), cafeId()}};
    f.transport.setSnapshot(connected);
    QTRY_COMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    QCOMPARE(f.controller.feedback(), QStringLiteral("Connected to Cafe."));
    QVERIFY(f.pointRow(QStringLiteral("Cafe")).value(QStringLiteral("active")).toBool());
    QCOMPARE(f.controller.summaryLabel(), QStringLiteral("Wi-Fi: Cafe"));
    QCOMPARE(f.transport.operations.size(), 1);

    f.controller.setExpanded(true);
    f.controller.setExpanded(false);
    QVERIFY(!f.controller.feedbackPresent()); // settled text ends with the popup
}

void NetworkAppletControllerTests::newNetworkSendsOnlyTheOpaqueAccessPointId()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestConnect(secureGuestPoint()));
    QCOMPARE(f.transport.operations.size(), 1);
    QCOMPARE(f.transport.operations.last().kind, OperationKind::ConnectVisibleNetwork);
    // AGENT-GUARD: ADR-0069. Exactly one opaque id crosses; no credential.
    QCOMPARE(f.transport.operations.last().parameters,
             QVariantMap({{QStringLiteral("accessPointId"), secureGuestPoint()}}));
    QVERIFY(f.controller.feedback().contains(QStringLiteral("password prompt")));

    f.transport.finishLast(operationResult(OperationKind::ConnectVisibleNetwork,
                                           OperationStatus::Succeeded));
    Network::Snapshot created = appletSnapshot(2);
    created.knownNetworks.append({guestKnownId(), QStringLiteral("Secure Guest"), false,
                                  Network::SecuritySuite::Wpa2Personal, true});
    created.activeConnections = {{QStringLiteral("wlan0"), guestKnownId()}};
    f.transport.setSnapshot(created);
    QTRY_COMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    QVERIFY(f.pointRow(QStringLiteral("Secure Guest")).value(QStringLiteral("saved")).toBool());
}

void NetworkAppletControllerTests::serviceRefusalAndLocalRefusal()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(!f.controller.requestConnect(QStringLiteral("not-a-row")));
    QVERIFY(!f.controller.requestConnect(homePoint())); // already active
    QVERIFY(f.controller.feedback().contains(QStringLiteral("already connected")));
    QCOMPARE(f.transport.operations.size(), 0);

    QVERIFY(f.controller.requestConnect(guestPoint()));
    f.transport.finishLast(operationResult(OperationKind::ConnectVisibleNetwork,
                                           OperationStatus::Rejected, 20, 1,
                                           QStringLiteral("credentials-required")));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("failed"));
    QVERIFY(f.controller.feedback().contains(QStringLiteral("Could not connect to Guest")));
    QVERIFY(!f.controller.operationPending());
    f.controller.clearFeedback();
    QVERIFY(!f.controller.feedbackPresent());
}

void NetworkAppletControllerTests::uncertainTransportFailureIsNeverReplayed()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestConnect(cafePoint()));
    f.transport.failLast();
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("uncertain"));
    QVERIFY(f.controller.feedback().contains(QStringLiteral("before trying again")));
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QTest::qWait(150);
    QCOMPARE(f.transport.operations.size(), 1);
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("uncertain"));
}

void NetworkAppletControllerTests::ownerReplacementRetiresThePendingRequest()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestConnect(cafePoint()));
    const Network::Snapshot replacement = readySnapshot(QStringLiteral(":1.77"), 30, 1);
    f.transport.setSnapshot(replacement);
    f.transport.announceOwner(QStringLiteral(":1.77"));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("uncertain"));
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QCOMPARE(f.controller.serviceEpoch(), 30u);
    // A late reply for the retired owner cannot complete anything.
    f.transport.finishLast(operationResult(OperationKind::ConnectKnownNetwork,
                                           OperationStatus::Succeeded),
                           QStringLiteral(":1.20"));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("uncertain"));
    QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkAppletControllerTests::ownerLossClearsAllRows()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestScan());
    f.transport.finishLast(operationResult(OperationKind::RequestScan,
                                           OperationStatus::Succeeded));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    f.transport.announceOwner(QString());
    QCOMPARE(f.controller.phase(), QStringLiteral("unavailable"));
    QVERIFY(f.controller.accessPointRows().isEmpty());
    QVERIFY(f.controller.connectionRows().isEmpty());
    QVERIFY(f.controller.radioRows().isEmpty());
    QCOMPARE(f.controller.iconName(), QStringLiteral("network-offline"));
    QCOMPARE(f.controller.indicator(), QStringLiteral("unavailable"));
    // The old owner's settled outcome is not carried over.
    QVERIFY(!f.controller.feedbackPresent());
}

void NetworkAppletControllerTests::radioSwitchConfirmsAndHardwareRefuses()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestRadio(QStringLiteral("wifi"), false));
    QCOMPARE(f.transport.operations.last().kind, OperationKind::SetRadio);
    QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("enable")).toBool(),
             false);
    f.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("confirming"));
    const QVariantMap stillOn = f.controller.radioRows().constFirst().toMap();
    QVERIFY(stillOn.value(QStringLiteral("enabled")).toBool());
    QVERIFY(stillOn.value(QStringLiteral("pending")).toBool());

    Network::Snapshot off = appletSnapshot(2);
    off.radios[0].softwareEnabled = false;
    off.activeConnections.clear();
    off.accessPoints.clear();
    f.transport.setSnapshot(off);
    QTRY_COMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    QCOMPARE(f.controller.feedback(), QStringLiteral("Wi-Fi is off."));
    QCOMPARE(f.controller.indicator(), QStringLiteral("radio-off"));
    QCOMPARE(f.controller.iconName(), QStringLiteral("network-wireless-off"));

    Network::Snapshot blocked = off;
    blocked.revision = 3;
    blocked.radios[0].hardwareEnabled = false;
    f.transport.setSnapshot(blocked);
    f.transport.invalidate();
    QTRY_COMPARE(f.controller.serviceRevision(), 3u);
    QTRY_VERIFY(f.client.operationAdmissionReady());
    QVERIFY(!f.controller.radioRows().constFirst().toMap()
                     .value(QStringLiteral("canToggle")).toBool());
    QVERIFY(!f.controller.requestRadio(QStringLiteral("wifi"), true));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("failed"));
    QVERIFY(f.controller.feedback().contains(QStringLiteral("hardware switch")));
    QCOMPARE(f.transport.operations.size(), 1);
    QVERIFY(!f.controller.requestRadio(QStringLiteral("mobile"), true)); // absent
    QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkAppletControllerTests::noWifiDeviceHasNoNetworkList()
{
    Network::Snapshot wiredOnly = appletSnapshot();
    wiredOnly.devices.removeFirst();
    wiredOnly.accessPoints.clear();
    wiredOnly.activeConnections.clear();
    Fixture f(wiredOnly);
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(!f.controller.wifiDevicePresent());
    QVERIFY(f.controller.accessPointRows().isEmpty());
    QCOMPARE(f.controller.indicator(), QStringLiteral("disconnected"));
    QCOMPARE(f.controller.iconName(), QStringLiteral("network-offline"));
}

void NetworkAppletControllerTests::serviceAbsentIsUnavailable()
{
    FakeNetworkTransport transport;
    Network::Client::NetworkClient client(transport, [] { return qint64{1}; }, fastTiming());
    NetworkAppletController controller(&client, true, true, fastApplet());
    QCOMPARE(controller.phase(), QStringLiteral("unavailable"));
    QCOMPARE(controller.accessibleName(), QStringLiteral("Network is unavailable"));
    QVERIFY(client.start());
    // No Network1 owner has appeared: loading, with nothing to act on.
    QCOMPARE(controller.phase(), QStringLiteral("loading"));
    QVERIFY(controller.accessPointRows().isEmpty());
    QVERIFY(!controller.requestScan());
    QCOMPARE(transport.operations.size(), 0);
    QVERIFY(controller.feedback().contains(QStringLiteral("not ready")));

    FakeNetworkTransport failing;
    failing.startFails = true;
    Network::Client::NetworkClient failed(failing, [] { return qint64{1}; }, fastTiming());
    NetworkAppletController absent(&failed, true, true, fastApplet());
    QVERIFY(!failed.start());
    QCOMPARE(absent.phase(), QStringLiteral("unavailable"));
    QCOMPARE(absent.iconName(), QStringLiteral("network-offline"));
}

void NetworkAppletControllerTests::unconfirmedChangeBecomesUncertainAtTheDeadline()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestDisconnect(QStringLiteral("wlan0")));
    f.transport.finishLast(operationResult(OperationKind::DisconnectActive,
                                           OperationStatus::Succeeded));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("confirming"));
    const qsizetype fetchesBefore = f.transport.snapshotRequests.size();
    QTRY_COMPARE_WITH_TIMEOUT(f.controller.requestPhase(), QStringLiteral("uncertain"), 2'000);
    QVERIFY(f.transport.snapshotRequests.size() > fetchesBefore); // readback polled
    QVERIFY(f.controller.feedback().contains(QStringLiteral("not confirmed in time")));
    QCOMPARE(f.transport.operations.size(), 1);
}

void NetworkAppletControllerTests::rescanAndDisconnect()
{
    Fixture f;
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(f.controller.requestScan());
    QCOMPARE(f.transport.operations.last().kind, OperationKind::RequestScan);
    QCOMPARE(f.transport.operations.last().parameters.value(QStringLiteral("deadlineMs"))
                 .toLongLong(),
             kScanDeadlineMilliseconds);
    QVERIFY(f.controller.scanning());
    f.transport.finishLast(operationResult(OperationKind::RequestScan, OperationStatus::Succeeded));
    QCOMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    QTRY_VERIFY(f.controller.connectionRows().constFirst().toMap()
                    .value(QStringLiteral("canDisconnect")).toBool());

    QVERIFY(f.controller.requestDisconnect(QStringLiteral("wlan0")));
    QCOMPARE(f.transport.operations.last().kind, OperationKind::DisconnectActive);
    QCOMPARE(f.transport.operations.last().parameters,
             QVariantMap({{QStringLiteral("deviceInterface"), QStringLiteral("wlan0")}}));
    QVERIFY(f.controller.connectionRows().constFirst().toMap()
                .value(QStringLiteral("pending")).toBool());
    f.transport.finishLast(operationResult(OperationKind::DisconnectActive,
                                           OperationStatus::Succeeded, 20, 1));
    Network::Snapshot idle = appletSnapshot(2);
    idle.activeConnections.clear();
    f.transport.setSnapshot(idle);
    QTRY_COMPARE(f.controller.requestPhase(), QStringLiteral("succeeded"));
    QCOMPARE(f.controller.feedback(), QStringLiteral("Disconnected Home."));
    QCOMPARE(f.controller.indicator(), QStringLiteral("disconnected"));
    QCOMPARE(f.controller.iconName(), QStringLiteral("network-wireless-disconnected"));
}

void NetworkAppletControllerTests::controlDenialAndSettingsLaunch()
{
    Fixture f(appletSnapshot(), false);
    QTRY_COMPARE(f.controller.phase(), QStringLiteral("ready"));
    QVERIFY(!f.controller.scanAvailable());
    QVERIFY(!f.controller.requestScan());
    QVERIFY(f.controller.feedback().contains(QStringLiteral("not allowed")));
    QCOMPARE(f.transport.operations.size(), 0);

    QVERIFY(!f.controller.canOpenSettings());
    QVERIFY(!f.controller.openSettings());
    int launches = 0;
    bool launchResult = true;
    f.controller.setSettingsLaunch([&] { ++launches; return launchResult; });
    QVERIFY(f.controller.canOpenSettings());
    QVERIFY(f.controller.openSettings());
    QCOMPARE(launches, 1);
    launchResult = false;
    QVERIFY(!f.controller.openSettings());
    QCOMPARE(f.controller.feedback(), QStringLiteral("Network Settings could not be opened."));
}

QTEST_GUILESS_MAIN(NetworkAppletControllerTests)
#include "tst_network_applet_controller.moc"
