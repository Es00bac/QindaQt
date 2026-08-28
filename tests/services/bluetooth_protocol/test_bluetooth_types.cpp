// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtTest/QTest>
#include <QtDBus/QDBusArgument>

using namespace QindaQt::Bluetooth;

class TestBluetoothTypes : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        registerDBusTypes();
    }

    void testHandleValidity()
    {
        Handle invalid;
        QVERIFY(!invalid.isValid());

        Handle valid{1, 1};
        QVERIFY(valid.isValid());

        Handle zeroEpoch{0, 1};
        QVERIFY(!zeroEpoch.isValid());

        Handle zeroSerial{1, 0};
        QVERIFY(!zeroSerial.isValid());
    }

    void testHandleEquality()
    {
        Handle handle1{1, 1};
        Handle handle2{1, 1};
        Handle handle3{1, 2};

        QCOMPARE(handle1, handle2);
        QCOMPARE_NE(handle1, handle3);
    }

    void testAdapterCapabilities()
    {
        AdapterCapabilities none = AdapterCapability::None;
        AdapterCapabilities all = AdapterCapability::Discover | AdapterCapability::Pair
            | AdapterCapability::Connect;

        QCOMPARE(all.toInt(), 7);
        QVERIFY(all.testFlag(AdapterCapability::Discover));
        QVERIFY(all.testFlag(AdapterCapability::Pair));
        QVERIFY(all.testFlag(AdapterCapability::Connect));
        QVERIFY(!none.testFlag(AdapterCapability::Discover));
    }

    void testDeviceCapabilities()
    {
        DeviceCapabilities caps = DeviceCapability::Connect | DeviceCapability::Disconnect;
        QCOMPARE(caps.toInt(), 6);
        QVERIFY(caps.testFlag(DeviceCapability::Connect));
        QVERIFY(!caps.testFlag(DeviceCapability::Trust));
    }

    void testAdapterStructure()
    {
        Adapter adapter;
        adapter.handle = {1, 1};
        adapter.address = QStringLiteral("00:1A:7D:DA:71:13");
        adapter.name = QStringLiteral("Test");
        adapter.state = AdapterState::On;
        adapter.discoveringActive = false;
        adapter.capabilities = AdapterCapability::Discover | AdapterCapability::Pair;

        Adapter copy = adapter;
        QCOMPARE(adapter, copy);
    }

    void testDeviceStructure()
    {
        Device device;
        device.handle = {1, 2};
        device.adapterHandle = {1, 1};
        device.address = QStringLiteral("00:1B:63:84:45:E6");
        device.name = QStringLiteral("Headphones");
        device.state = DeviceState::Disconnected;
        device.rssi = -65;
        device.rssiKnown = true;
        device.paired = true;
        device.trusted = false;
        device.capabilities = DeviceCapability::Connect | DeviceCapability::Disconnect;

        Device copy = device;
        QCOMPARE(device, copy);
    }

    void testSnapshotValidity()
    {
        Snapshot snapshot;
        snapshot.schemaVersion = 1;
        snapshot.epoch = 1;
        snapshot.revision = 1;
        snapshot.wireValid = true;

        auto result = validateSnapshot(snapshot);
        QVERIFY(result.accepted);
    }

    void testSnapshotWithInvalidEpoch()
    {
        Snapshot snapshot;
        snapshot.schemaVersion = 1;
        snapshot.epoch = 0;
        snapshot.revision = 1;
        snapshot.wireValid = true;

        auto result = validateSnapshot(snapshot);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reasonCode, QStringLiteral("invalid-lineage"));
    }

    void testSnapshotWithOversizedPayload()
    {
        Snapshot snapshot;
        snapshot.schemaVersion = 1;
        snapshot.epoch = 1;
        snapshot.revision = 1;
        snapshot.wireValid = false;

        auto result = validateSnapshot(snapshot);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reasonCode, QStringLiteral("oversized-payload"));
    }

    void testOperationResultValidity()
    {
        OperationResult result;
        result.kind = OperationKind::Pair;
        result.status = OperationStatus::Succeeded;
        result.initiatingEpoch = 1;
        result.initiatingRevision = 1;
        result.observedEpoch = 1;
        result.observedRevision = 1;
        result.wireValid = true;

        auto validation = validateOperationResult(result);
        QVERIFY(validation.accepted);
    }

    void testOperationResultWithMalformedStatus()
    {
        OperationResult result;
        result.kind = OperationKind::Pair;
        result.status = static_cast<OperationStatus>(100);
        result.initiatingEpoch = 1;
        result.initiatingRevision = 1;
        result.observedEpoch = 1;
        result.observedRevision = 1;
        result.wireValid = true;

        auto validation = validateOperationResult(result);
        QVERIFY(!validation.accepted);
        QCOMPARE(validation.reasonCode, QStringLiteral("malformed-result"));
    }

    void testDBusHandleRoundTrip()
    {
        QDBusArgument arg;
        Handle original{1, 42};

        arg << original;

        QDBusArgument readArg = arg;
        Handle decoded;
        readArg >> decoded;

        QCOMPARE(original, decoded);
    }

    void testBoundedText()
    {
        QVERIFY(isBoundedText(QStringLiteral("test"), 256));
        QVERIFY(!isBoundedText(QStringLiteral("test"), 2));
        QVERIFY(!isBoundedText(QStringLiteral("te\0st"), 256));
    }

    void testBoundedSafeDiagnostic()
    {
        QString diagnostic = QStringLiteral("test message");
        auto result = boundedSafeDiagnostic(diagnostic);
        QCOMPARE(result, diagnostic);
    }

    void testBoundedSafeDiagnosticTruncation()
    {
        QString longDiagnostic(600, QLatin1Char('a'));
        auto result = boundedSafeDiagnostic(longDiagnostic);
        QVERIFY(result.toUtf8().size() <= 512);
    }
};

QTEST_MAIN(TestBluetoothTypes)
#include "test_bluetooth_types.moc"
