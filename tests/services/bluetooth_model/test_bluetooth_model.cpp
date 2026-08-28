// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtTest/QTest>

using namespace QindaQt::Bluetooth;

class TestBluetoothModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testModelInitialization()
    {
        BluetoothModel model;

        const auto adapters = model.adapters();
        QVERIFY(!adapters.isEmpty());
        QCOMPARE(adapters.size(), 1);
        QCOMPARE(adapters[0].address, QStringLiteral("00:1A:7D:DA:71:13"));
        QCOMPARE(adapters[0].state, AdapterState::On);

        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices[0].paired, true);
    }

    void testNextEpoch()
    {
        BluetoothModel model;
        const auto epoch1 = model.nextEpoch();
        QVERIFY(epoch1 > 1);
        const auto epoch2 = model.nextEpoch();
        QVERIFY(epoch2 > epoch1);
    }

    void testNextRevision()
    {
        BluetoothModel model;
        const auto rev1 = model.nextRevision();
        const auto rev2 = model.nextRevision();
        QVERIFY(rev2 > rev1);
    }

    void testNextSerial()
    {
        BluetoothModel model;
        const auto serial1 = model.nextSerial();
        const auto serial2 = model.nextSerial();
        QVERIFY(serial2 > serial1);
    }

    void testCurrentSnapshot()
    {
        BluetoothModel model;
        const auto snapshot = model.currentSnapshot();

        QCOMPARE(snapshot.schemaVersion, quint32(1));
        QVERIFY(snapshot.epoch > 0);
        QVERIFY(snapshot.revision >= 0);
        QCOMPARE(snapshot.adapters.size(), model.adapters().size());
        QCOMPARE(snapshot.devices.size(), model.devices().size());
        QVERIFY(snapshot.wireValid);

        // Validate snapshot structure
        auto validation = validateSnapshot(snapshot);
        QVERIFY(validation.accepted);
    }

    void testPairOperation()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());

        OperationRequest request;
        request.kind = OperationKind::Pair;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QCOMPARE(result.kind, OperationKind::Pair);
        QCOMPARE(result.status, OperationStatus::Succeeded);
    }

    void testPairAlreadyPairedDevice()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());
        QVERIFY(devices[0].paired);

        OperationRequest request;
        request.kind = OperationKind::Pair;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Rejected);
        QCOMPARE(result.reasonCode, QStringLiteral("already-paired"));
    }

    void testPairNonExistentDevice()
    {
        BluetoothModel model;

        OperationRequest request;
        request.kind = OperationKind::Pair;
        request.primary = {1, 999};

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Rejected);
        QCOMPARE(result.reasonCode, QStringLiteral("device-not-found"));
    }

    void testConnectOperation()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());
        QVERIFY(devices[0].paired);

        OperationRequest request;
        request.kind = OperationKind::Connect;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Succeeded);

        // Verify the device is now connected
        const auto updated = model.currentSnapshot();
        QVERIFY(!updated.devices.isEmpty());
        QCOMPARE(updated.devices[0].state, DeviceState::Connected);
    }

    void testConnectUnpairedDevice()
    {
        BluetoothModel model;

        OperationRequest request;
        request.kind = OperationKind::Connect;
        request.primary = {1, 999};

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Rejected);
    }

    void testDisconnectOperation()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());

        // First connect
        {
            OperationRequest request;
            request.kind = OperationKind::Connect;
            request.primary = devices[0].handle;
            model.executeOperation(request);
        }

        // Then disconnect
        {
            OperationRequest request;
            request.kind = OperationKind::Disconnect;
            request.primary = devices[0].handle;

            const auto result = model.executeOperation(request);
            QCOMPARE(result.status, OperationStatus::Succeeded);
        }

        // Verify the device is disconnected
        const auto updated = model.currentSnapshot();
        QVERIFY(!updated.devices.isEmpty());
        QCOMPARE(updated.devices[0].state, DeviceState::Disconnected);
    }

    void testDisconnectAlreadyDisconnected()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());
        QCOMPARE(devices[0].state, DeviceState::Disconnected);

        OperationRequest request;
        request.kind = OperationKind::Disconnect;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Rejected);
        QCOMPARE(result.reasonCode, QStringLiteral("already-disconnected"));
    }

    void testTrustOperation()
    {
        BluetoothModel model;
        const auto devices = model.devices();
        QVERIFY(!devices.isEmpty());
        QVERIFY(devices[0].trusted);

        OperationRequest request;
        request.kind = OperationKind::Untrust;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QCOMPARE(result.status, OperationStatus::Succeeded);

        const auto updated = model.currentSnapshot();
        QVERIFY(!updated.devices.isEmpty());
        QVERIFY(!updated.devices[0].trusted);
    }

    void testOperationResultLineage()
    {
        BluetoothModel model;
        const auto devices = model.devices();

        OperationRequest request;
        request.kind = OperationKind::Pair;
        request.primary = devices[0].handle;

        const auto result = model.executeOperation(request);
        QVERIFY(result.initiatingEpoch > 0);
        QVERIFY(result.initiatingRevision >= 0);
        QVERIFY(result.observedEpoch >= result.initiatingEpoch);
        QVERIFY(result.observedRevision >= result.initiatingRevision);
    }
};

QTEST_MAIN(TestBluetoothModel)
#include "test_bluetooth_model.moc"
