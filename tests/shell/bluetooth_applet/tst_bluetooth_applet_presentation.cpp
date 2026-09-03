// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtTest>

#include <algorithm>

using namespace QindaQt;
using namespace QindaQt::Shell::BluetoothApplet;

namespace
{

Bluetooth::Snapshot readySnapshot()
{
    Bluetooth::Snapshot snapshot;
    snapshot.schemaVersion = Bluetooth::kSchemaVersion;
    snapshot.epoch = 44;
    snapshot.revision = 9;
    snapshot.availability = Bluetooth::Availability::Ready;
    snapshot.capabilities = Bluetooth::Capability::SetAdapterPower
        | Bluetooth::Capability::DiscoveryLease
        | Bluetooth::Capability::ConnectPaired
        | Bluetooth::Capability::DisconnectPaired;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.adapters = {
        {.handle = {.epoch = 44, .serial = 10},
         .address = QStringLiteral("AA:BB:CC:00:11:22"),
         .name = QStringLiteral("Built-in radio"),
         .powered = true,
         .discovering = true},
        {.handle = {.epoch = 44, .serial = 20},
         .address = QStringLiteral("AA:BB:CC:00:11:33"),
         .name = {},
         .powered = false,
         .discovering = false},
    };
    snapshot.devices = {
        {.handle = {.epoch = 44, .serial = 100},
         .adapterHandle = {.epoch = 44, .serial = 10},
         .address = QStringLiteral("AA:BB:CC:44:55:66"),
         .name = QStringLiteral("Desk keyboard"),
         .deviceClass = Bluetooth::DeviceClass::Keyboard,
         .role = Bluetooth::DeviceRole::Peripheral,
         .paired = true,
         .connected = true,
         .rssiKnown = true,
         .rssi = -41,
         .batteryKnown = true,
         .batteryPercent = 73},
        {.handle = {.epoch = 44, .serial = 200},
         .adapterHandle = {.epoch = 44, .serial = 10},
         .address = QStringLiteral("AA:BB:CC:44:55:77"),
         .name = {},
         .deviceClass = Bluetooth::DeviceClass::Headphones,
         .paired = true,
         .connected = false},
    };
    return snapshot;
}

} // namespace

class BluetoothAppletPresentationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsBoundedNonSecretRowsAndAccessibility();
    void replacesAddressShapedNamesWithSafeFallbacks();
    void failsClosedWithoutExactOwnerOrReadGrant();
    void rejectsMalformedAndUnavailableTruth();
    void controlGrantAndLeaseGateEveryAction();
};

void BluetoothAppletPresentationTests::projectsBoundedNonSecretRowsAndAccessibility()
{
    const BluetoothAppletModel model = projectBluetoothApplet(
        readySnapshot(), true, true, true,
        Bluetooth::Handle{.epoch = 44, .serial = 10});
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.epoch, quint64(44));
    QCOMPARE(model.revision, quint64(9));
    QCOMPARE(model.adapters.size(), 2);
    QCOMPARE(model.devices.size(), 2);
    QCOMPARE(model.summaryLabel, QStringLiteral("Bluetooth 1"));
    QVERIFY(model.accessibleName.contains(QStringLiteral("1 device connected")));

    const AdapterRow &adapter = model.adapters.constFirst();
    QCOMPARE(adapter.id, QStringLiteral("adapter-44-10"));
    QCOMPARE(adapter.label, QStringLiteral("Built-in radio"));
    QVERIFY(adapter.canReleaseDiscovery);
    QVERIFY(!adapter.canAcquireDiscovery);
    QVERIFY(!adapter.accessibleName.isEmpty());
    QVERIFY(!adapter.accessibleDescription.isEmpty());

    const AdapterRow &fallbackAdapter = model.adapters.constLast();
    QCOMPARE(fallbackAdapter.label, QStringLiteral("Bluetooth adapter 2"));
    QVERIFY(!fallbackAdapter.label.contains(QStringLiteral("AA:BB")));

    const DeviceRow &keyboard = model.devices.constFirst();
    QCOMPARE(keyboard.id, QStringLiteral("device-44-100"));
    QVERIFY(keyboard.canDisconnect);
    QVERIFY(!keyboard.canConnect);
    QVERIFY(keyboard.accessibleDescription.contains(QStringLiteral("battery 73")));
    QVERIFY(keyboard.accessibleDescription.contains(QStringLiteral("signal -41")));
    QVERIFY(!keyboard.label.contains(QStringLiteral("AA:BB")));

    const DeviceRow &headphones = model.devices.constLast();
    QCOMPARE(headphones.label, QStringLiteral("Headphones"));
    QVERIFY(headphones.canConnect);
    QVERIFY(!headphones.accessibleName.isEmpty());
    QVERIFY(!headphones.accessibleDescription.isEmpty());
}

void BluetoothAppletPresentationTests::replacesAddressShapedNamesWithSafeFallbacks()
{
    Bluetooth::Snapshot snapshot = readySnapshot();
    snapshot.adapters[0].name = QStringLiteral("  AA:BB:CC:00:11:22  ");
    snapshot.devices[0].name = QStringLiteral("AA:BB:CC:44:55:66");

    const BluetoothAppletModel model = projectBluetoothApplet(
        snapshot, true, true, true);
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.adapters[0].label, QStringLiteral("Bluetooth adapter 1"));
    QCOMPARE(model.devices[0].label, QStringLiteral("Keyboard"));
    QVERIFY(!model.adapters[0].accessibleName.contains(
        QStringLiteral("AA:BB:CC:00:11:22")));
    QVERIFY(!model.devices[0].accessibleName.contains(
        QStringLiteral("AA:BB:CC:44:55:66")));
}

void BluetoothAppletPresentationTests::failsClosedWithoutExactOwnerOrReadGrant()
{
    const BluetoothAppletModel noOwner = projectBluetoothApplet(
        readySnapshot(), false, true, true);
    QCOMPARE(noOwner.phase, ServicePhase::Unavailable);
    QCOMPARE(noOwner.epoch, quint64(0));
    QVERIFY(noOwner.adapters.isEmpty());
    QVERIFY(noOwner.devices.isEmpty());

    const BluetoothAppletModel denied = projectBluetoothApplet(
        readySnapshot(), true, false, true);
    QCOMPARE(denied.phase, ServicePhase::Unavailable);
    QVERIFY(denied.diagnostic.contains(QStringLiteral("not granted")));
    QVERIFY(denied.adapters.isEmpty());
}

void BluetoothAppletPresentationTests::rejectsMalformedAndUnavailableTruth()
{
    Bluetooth::Snapshot malformed = readySnapshot();
    malformed.devices[0].connected = true;
    malformed.devices[0].paired = false;
    const BluetoothAppletModel rejected = projectBluetoothApplet(
        malformed, true, true, true);
    QCOMPARE(rejected.phase, ServicePhase::Unavailable);
    QVERIFY(rejected.diagnostic.contains(QStringLiteral("malformed")));
    QVERIFY(rejected.devices.isEmpty());

    Bluetooth::Snapshot unavailable = readySnapshot();
    unavailable.availability = Bluetooth::Availability::Unavailable;
    unavailable.capabilities = {};
    unavailable.adapters.clear();
    unavailable.devices.clear();
    unavailable.reasonCode = QStringLiteral("no-adapter");
    const BluetoothAppletModel absent = projectBluetoothApplet(
        unavailable, true, true, true);
    QCOMPARE(absent.phase, ServicePhase::Unavailable);
    QVERIFY(absent.diagnostic.contains(QStringLiteral("no-adapter")));
}

void BluetoothAppletPresentationTests::controlGrantAndLeaseGateEveryAction()
{
    const BluetoothAppletModel readOnly = projectBluetoothApplet(
        readySnapshot(), true, true, false);
    QVERIFY(std::ranges::none_of(readOnly.adapters, [](const AdapterRow &row) {
        return row.canSetPowered || row.canAcquireDiscovery
            || row.canReleaseDiscovery;
    }));
    QVERIFY(std::ranges::none_of(readOnly.devices, [](const DeviceRow &row) {
        return row.canConnect || row.canDisconnect;
    }));

    const BluetoothAppletModel controlled = projectBluetoothApplet(
        readySnapshot(), true, true, true);
    QVERIFY(controlled.adapters[0].canAcquireDiscovery);
    QVERIFY(controlled.adapters[0].canSetPowered);
    QVERIFY(!controlled.adapters[1].canAcquireDiscovery);
}

QTEST_GUILESS_MAIN(BluetoothAppletPresentationTests)
#include "tst_bluetooth_applet_presentation.moc"
