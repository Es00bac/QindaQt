// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_test_support.h"

#include <qindaqt/shell/network_applet/network_applet_presentation.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::NetworkApplet;
using namespace QindaQt::Shell::NetworkApplet::TestSupport;

namespace
{

Network::Model::NetworkModel modelFor(const Network::Snapshot &snapshot)
{
    Network::Model::NetworkModel model([] { return qint64{5'000}; });
    const auto applied = model.applySnapshot(snapshot);
    Q_ASSERT_X(applied.accepted, "modelFor", qPrintable(applied.reasonCode));
    return model;
}

ProjectionContext readyContext()
{
    ProjectionContext context;
    context.readGranted = true;
    context.controlGranted = true;
    context.snapshotCurrent = true;
    context.clientPhase = ServicePhase::Ready;
    context.admissionOpen = true;
    return context;
}

const AccessPointRow *row(const NetworkAppletModel &model, const QString &label)
{
    for (const AccessPointRow &candidate : model.accessPoints) {
        if (candidate.label == label) return &candidate;
    }
    return nullptr;
}

} // namespace

class NetworkAppletPresentationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readySnapshotProjectsRowsStrongestFirst();
    void duplicateAccessPointsCollapseToStrongestAndHiddenAreOmitted();
    void wiredConnectionWinsTheIndicator();
    void radioOffAndNoWifiDeviceIndicators();
    void signalLevelsSelectTheClosedIconVocabulary();
    void lostCurrencyDropsEveryRow();
    void grantsAndAdmissionGateEveryAction();
    void unsupportedSecurityIsVisibleButNotJoinable();
};

void NetworkAppletPresentationTests::readySnapshotProjectsRowsStrongestFirst()
{
    const auto model = projectNetworkApplet(modelFor(appletSnapshot()), readyContext());
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.owner, kOwner);
    QCOMPARE(model.epoch, 20u);
    QCOMPARE(model.indicator, Indicator::Wireless);
    QCOMPARE(model.iconName, QStringLiteral("network-wireless-signal-excellent"));
    QCOMPARE(model.summaryLabel, QStringLiteral("Wi-Fi: Home"));
    QVERIFY(model.accessibleName.contains(QStringLiteral("Home")));
    QVERIFY(model.accessibleName.contains(QStringLiteral("88%")));
    QVERIFY(model.accessibleDescription.contains(QStringLiteral("Internet available")));
    QVERIFY(model.wifiDevicePresent);
    QVERIFY(model.scanAvailable);

    QCOMPARE(model.accessPoints.size(), 4);
    QStringList order;
    for (const AccessPointRow &point : model.accessPoints) order.append(point.label);
    QCOMPARE(order, QStringList({QStringLiteral("Home"), QStringLiteral("Guest"),
                                 QStringLiteral("Cafe"), QStringLiteral("Secure Guest")}));
    const AccessPointRow *home = row(model, QStringLiteral("Home"));
    QVERIFY(home->active && home->saved && home->secured);
    QVERIFY(!home->canConnect);
    QCOMPARE(home->connectBlockedReason, QStringLiteral("network-already-active"));
    const AccessPointRow *cafe = row(model, QStringLiteral("Cafe"));
    QVERIFY(cafe->saved && !cafe->secured && !cafe->active && cafe->canConnect);
    QCOMPARE(cafe->knownNetworkId, cafeId());
    const AccessPointRow *guest = row(model, QStringLiteral("Guest"));
    QVERIFY(!guest->saved && guest->canConnect && guest->knownNetworkId.isEmpty());
    QCOMPARE(guest->id, guestPoint());
    const AccessPointRow *secure = row(model, QStringLiteral("Secure Guest"));
    QVERIFY(secure->secured && !secure->saved && secure->canConnect);
    QVERIFY(secure->accessibleDescription.contains(QStringLiteral("secured")));

    QCOMPARE(model.connections.size(), 1);
    QCOMPARE(model.connections[0].id, QStringLiteral("wlan0"));
    QCOMPARE(model.connections[0].label, QStringLiteral("Home"));
    QVERIFY(model.connections[0].canDisconnect);

    QCOMPARE(model.radios.size(), 1); // the absent mobile radio is omitted
    QCOMPARE(model.radios[0].id, QStringLiteral("wifi"));
    QVERIFY(model.radios[0].softwareEnabled);
    QVERIFY(model.radios[0].canToggle);

    const auto withoutRadioControl =
        projectNetworkApplet(modelFor(appletSnapshot(1, false)), readyContext());
    QVERIFY(!withoutRadioControl.radios[0].canToggle);
    QCOMPARE(withoutRadioControl.radios[0].blockedReason,
             QStringLiteral("radio-control-unsupported"));
}

void NetworkAppletPresentationTests::duplicateAccessPointsCollapseToStrongestAndHiddenAreOmitted()
{
    Network::Snapshot snapshot = appletSnapshot();
    snapshot.accessPoints.append({QStringLiteral("wlan0"), QStringLiteral("Guest"), false,
                                  QStringLiteral("66:77:88:99:aa:ee"),
                                  Network::SecuritySuite::Open, 5200, 95});
    snapshot.accessPoints.append({QStringLiteral("wlan0"), QString(), true,
                                  QStringLiteral("66:77:88:99:aa:ff"),
                                  Network::SecuritySuite::Wpa2Personal, 2412, 99});
    const auto model = projectNetworkApplet(modelFor(snapshot), readyContext());
    QCOMPARE(model.accessPoints.size(), 4);
    QCOMPARE(model.accessPoints[0].label, QStringLiteral("Guest"));
    QCOMPARE(model.accessPoints[0].signalPercent, 95);
    QCOMPARE(model.accessPoints[0].id, pointId(QStringLiteral("66:77:88:99:aa:ee")));
}

void NetworkAppletPresentationTests::wiredConnectionWinsTheIndicator()
{
    Network::Snapshot snapshot = appletSnapshot();
    const QString wiredProfile = networkId(u'd');
    snapshot.knownNetworks.append({wiredProfile, QStringLiteral("Wired connection 1"),
                                   false, Network::SecuritySuite::Open, true});
    snapshot.devices[1].state = Network::DeviceState::Connected;
    snapshot.activeConnections.append({QStringLiteral("enp3s0"), wiredProfile});
    const auto model = projectNetworkApplet(modelFor(snapshot), readyContext());
    QCOMPARE(model.indicator, Indicator::Wired);
    QCOMPARE(model.iconName, QStringLiteral("network-wired"));
    QCOMPARE(model.summaryLabel, QStringLiteral("Wired"));
    QCOMPARE(model.connections.size(), 2);
    QCOMPARE(model.connections[1].label, QStringLiteral("Wired connection"));
    QCOMPARE(model.connections[1].kindLabel, QStringLiteral("Wired"));
}

void NetworkAppletPresentationTests::radioOffAndNoWifiDeviceIndicators()
{
    Network::Snapshot off = appletSnapshot();
    off.radios[0].softwareEnabled = false;
    off.activeConnections.clear();
    off.devices[0].state = Network::DeviceState::Unavailable;
    off.accessPoints.clear();
    const auto radioOff = projectNetworkApplet(modelFor(off), readyContext());
    QCOMPARE(radioOff.indicator, Indicator::RadioOff);
    QCOMPARE(radioOff.iconName, QStringLiteral("network-wireless-off"));
    QCOMPARE(radioOff.accessibleName, QStringLiteral("Network: Wi-Fi is off"));
    QVERIFY(radioOff.radios[0].canToggle);

    Network::Snapshot blocked = off;
    blocked.radios[0].hardwareEnabled = false;
    const auto hardware = projectNetworkApplet(modelFor(blocked), readyContext());
    QVERIFY(!hardware.radios[0].canToggle);
    QCOMPARE(hardware.radios[0].blockedReason, QStringLiteral("radio-hardware-disabled"));
    QVERIFY(hardware.radios[0].accessibleDescription.contains(QStringLiteral("hardware")));

    Network::Snapshot wiredOnly = appletSnapshot();
    wiredOnly.devices.removeFirst();
    wiredOnly.accessPoints.clear();
    wiredOnly.activeConnections.clear();
    const auto disconnected = projectNetworkApplet(modelFor(wiredOnly), readyContext());
    QCOMPARE(disconnected.indicator, Indicator::Disconnected);
    QCOMPARE(disconnected.iconName, QStringLiteral("network-offline"));
    QVERIFY(!disconnected.wifiDevicePresent);
    QVERIFY(disconnected.accessPoints.isEmpty());
    QVERIFY(disconnected.connections.isEmpty());
    // Connectivity is only claimed for an actual connection.
    QVERIFY(!disconnected.accessibleDescription.contains(QStringLiteral("Internet")));
}

void NetworkAppletPresentationTests::signalLevelsSelectTheClosedIconVocabulary()
{
    QCOMPARE(indicatorIconName(Indicator::Wireless, 100, true),
             QStringLiteral("network-wireless-signal-excellent"));
    QCOMPARE(indicatorIconName(Indicator::Wireless, 60, true),
             QStringLiteral("network-wireless-signal-good"));
    QCOMPARE(indicatorIconName(Indicator::Wireless, 30, true),
             QStringLiteral("network-wireless-signal-ok"));
    QCOMPARE(indicatorIconName(Indicator::Wireless, 5, true),
             QStringLiteral("network-wireless-signal-weak"));
    QCOMPARE(indicatorIconName(Indicator::Wireless, 0, true),
             QStringLiteral("network-wireless-signal-none"));
    // Unknown strength is not rendered as a zero bar.
    QCOMPARE(indicatorIconName(Indicator::Wireless, -1, true),
             QStringLiteral("network-wireless"));
    QCOMPARE(indicatorIconName(Indicator::Disconnected, -1, true),
             QStringLiteral("network-wireless-disconnected"));
    QCOMPARE(indicatorIconName(Indicator::Unavailable, -1, true),
             QStringLiteral("network-offline"));
}

void NetworkAppletPresentationTests::lostCurrencyDropsEveryRow()
{
    const auto networkModel = modelFor(appletSnapshot());
    ProjectionContext stale = readyContext();
    stale.snapshotCurrent = false;
    stale.clientPhase = ServicePhase::Degraded;
    const auto staleModel = projectNetworkApplet(networkModel, stale);
    QCOMPARE(staleModel.phase, ServicePhase::Unavailable);
    QVERIFY(staleModel.accessPoints.isEmpty());
    QVERIFY(staleModel.connections.isEmpty());
    QVERIFY(staleModel.radios.isEmpty());
    QCOMPARE(staleModel.indicator, Indicator::Unavailable);
    QCOMPARE(staleModel.iconName, QStringLiteral("network-offline"));

    ProjectionContext loading = readyContext();
    loading.clientPhase = ServicePhase::Loading;
    QCOMPARE(projectNetworkApplet(Network::Model::NetworkModel(), loading).phase,
             ServicePhase::Loading);

    ProjectionContext denied = readyContext();
    denied.readGranted = false;
    const auto deniedModel = projectNetworkApplet(networkModel, denied);
    QCOMPARE(deniedModel.phase, ServicePhase::Unavailable);
    QVERIFY(deniedModel.diagnostic.contains(QStringLiteral("not granted")));
    QVERIFY(deniedModel.accessPoints.isEmpty());

    ProjectionContext degraded = readyContext();
    degraded.clientPhase = ServicePhase::Degraded;
    degraded.clientDiagnostic = QStringLiteral("radio inventory is partial");
    const auto degradedModel = projectNetworkApplet(networkModel, degraded);
    QCOMPARE(degradedModel.phase, ServicePhase::Degraded);
    QCOMPARE(degradedModel.diagnostic, QStringLiteral("radio inventory is partial"));
    QCOMPARE(degradedModel.accessPoints.size(), 4);
}

void NetworkAppletPresentationTests::grantsAndAdmissionGateEveryAction()
{
    const auto networkModel = modelFor(appletSnapshot());
    for (const bool controlGranted : {false, true}) {
        ProjectionContext context = readyContext();
        context.controlGranted = controlGranted;
        context.admissionOpen = !controlGranted; // exactly one gate closed
        const auto model = projectNetworkApplet(networkModel, context);
        QVERIFY(!model.scanAvailable);
        for (const AccessPointRow &point : model.accessPoints) QVERIFY(!point.canConnect);
        for (const ConnectionRow &connection : model.connections)
            QVERIFY(!connection.canDisconnect);
        for (const RadioRow &radio : model.radios) QVERIFY(!radio.canToggle);
        QCOMPARE(model.accessPoints.size(), 4); // read truth stays visible
    }
}

void NetworkAppletPresentationTests::unsupportedSecurityIsVisibleButNotJoinable()
{
    Network::Snapshot snapshot = appletSnapshot();
    snapshot.accessPoints.append({QStringLiteral("wlan0"), QStringLiteral("Office"), false,
                                  QStringLiteral("66:77:88:99:ab:01"),
                                  Network::SecuritySuite::Wpa2Enterprise, 5180, 70});
    const auto model = projectNetworkApplet(modelFor(snapshot), readyContext());
    const AccessPointRow *office = row(model, QStringLiteral("Office"));
    QVERIFY(office != nullptr);
    QVERIFY(!office->canConnect);
    QCOMPARE(office->connectBlockedReason, QStringLiteral("enterprise-network-unsupported"));
}

QTEST_GUILESS_MAIN(NetworkAppletPresentationTests)
#include "tst_network_applet_presentation.moc"
