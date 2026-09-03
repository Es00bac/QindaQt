// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/bluez_harness.h"

#include <QtTest>

#include <algorithm>

using namespace QindaQt::Bluetooth;
using BluezHarness = QindaQt::Tests::BluezHarness;
using FakeBluez = QindaQt::Tests::FakeBluez;

// Inventory, lifecycle, hostile payloads, and owner-loss rows for the
// production BlueZ adapter. Every row runs against a private bus carrying a
// fake org.bluez; none touches host Bluetooth, rfkill, or radios.
class BluezAdapterBackendTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void absentAtStartupPublishesUnavailable();
    void initialInventoryMapsTruth();
    void powerToggleRoundTrip();
    void hostilePropertiesAreBounded();
    void interfaceChurnUpdatesInventory();
    void duplicateAdapterAddressDeduplicated();
    void duplicateDeviceAddressDeduplicated();
    void ownerLossAndReturnRetiresTruth();
    void eventsAfterStopDoNotPublish();
    void invalidConnectionFailsClosed();
};

void BluezAdapterBackendTests::absentAtStartupPublishesUnavailable()
{
    BluezHarness harness(7001);
    QVERIFY(harness.ready());
    // No org.bluez owner exists: the activated service cannot order against
    // the system BlueZ unit, so it must tolerate absence truthfully.
    harness.model->start();
    QVERIFY(harness.waitUnavailable());
    const Snapshot snapshot = harness.model->snapshot();
    QCOMPARE(snapshot.reasonCode, QStringLiteral("no-adapter"));
    QVERIFY(snapshot.adapters.isEmpty());
    QVERIFY(snapshot.devices.isEmpty());
}

void BluezAdapterBackendTests::initialInventoryMapsTruth()
{
    BluezHarness harness(7002);
    QVERIFY(harness.ready());
    const QString adapterPath =
        harness.fake->addAdapter(QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
                                  QStringLiteral("Internal adapter"), true);
    const QString keyboardPath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *keyboard = harness.fake->device(keyboardPath);
    keyboard->paired = true;
    keyboard->connected = true;
    keyboard->deviceClass = 0x540;
    keyboard->rssiKnown = true;
    keyboard->rssi = -52;
    const QString mousePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:66"), QStringLiteral("Mouse"));
    FakeBluez::DeviceEntity *mouse = harness.fake->device(mousePath);
    mouse->icon = QStringLiteral("input-mouse");
    mouse->rssiKnown = true;
    mouse->rssi = 30;
    const QString malformedPath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:77"), QStringLiteral("Broken"));
    harness.fake->device(malformedPath)->rawAddress = QStringLiteral("not-an-address");
    const QString orphanPath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:88"), QStringLiteral("Orphan"));
    harness.fake->device(orphanPath)->adapterPath = QStringLiteral("/org/bluez/hci9");

    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    const Snapshot snapshot = harness.model->snapshot();

    QCOMPARE(snapshot.adapters.size(), 1);
    const Adapter &adapter = snapshot.adapters.constFirst();
    QCOMPARE(adapter.address, QStringLiteral("AA:BB:CC:00:11:22"));
    QCOMPARE(adapter.name, QStringLiteral("Internal adapter"));
    QVERIFY(adapter.powered);
    QVERIFY(!adapter.discovering);

    QCOMPARE(snapshot.devices.size(), 2);
    const Device &mappedKeyboard = snapshot.devices.constFirst();
    QCOMPARE(mappedKeyboard.address, QStringLiteral("AA:BB:CC:33:44:55"));
    QCOMPARE(mappedKeyboard.deviceClass, DeviceClass::Keyboard);
    QVERIFY(mappedKeyboard.paired);
    QVERIFY(mappedKeyboard.connected);
    QVERIFY(mappedKeyboard.rssiKnown);
    QCOMPARE(mappedKeyboard.rssi, qint16(-52));
    const Device &mappedMouse = snapshot.devices.last();
    QCOMPARE(mappedMouse.deviceClass, DeviceClass::Mouse);
    // A hostile positive RSSI is unrepresentable truth: unknown, never clamped.
    QVERIFY(!mappedMouse.rssiKnown);
    QCOMPARE(mappedMouse.rssi, qint16(0));
}

void BluezAdapterBackendTests::powerToggleRoundTrip()
{
    BluezHarness harness(7003);
    QVERIFY(harness.ready());
    const QString adapterPath =
        harness.fake->addAdapter(QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
                                  QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->paired = true;
    device->connected = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());

    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.snapshotAdapter().handle;
    const OperationSubmission off = harness.model->submit(
        {.kind = OperationKind::SetAdapterPower, .target = handle, .powered = false},
        QStringLiteral(":1.10"));
    QVERIFY(off.pending);
    const std::optional<OperationResult> offResult =
        harness.awaitResult(completed, off.operationId);
    QVERIFY(offResult.has_value());
    QCOMPARE(offResult->status, OperationStatus::Succeeded);
    QCOMPARE(offResult->reasonCode, QStringLiteral("adapter-power-set"));
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().adapters.constFirst().powered;
    }));
    // BlueZ truth: powering off terminated the connection.
    QVERIFY(!harness.model->snapshot().devices.constFirst().connected);

    const OperationSubmission on = harness.model->submit(
        {.kind = OperationKind::SetAdapterPower, .target = handle, .powered = true},
        QStringLiteral(":1.10"));
    QVERIFY(on.pending);
    const std::optional<OperationResult> onResult =
        harness.awaitResult(completed, on.operationId);
    QVERIFY(onResult.has_value());
    QCOMPARE(onResult->status, OperationStatus::Succeeded);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().adapters.constFirst().powered;
    }));
}

void BluezAdapterBackendTests::hostilePropertiesAreBounded()
{
    BluezHarness harness(7004);
    QVERIFY(harness.ready());
    QString hostileAlias;
    for (int index = 0; index < 300; ++index) {
        hostileAlias += QLatin1Char(index == 150 ? '\x01' : 'M');
    }
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"), hostileAlias, false);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), hostileAlias);
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->deviceClass = 0xFFFFFFFF;
    device->icon = QStringLiteral("mystery-widget");
    device->rawAddress = QStringLiteral("aa:bb:cc:33:44:55");
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    const Snapshot snapshot = harness.model->snapshot();

    QCOMPARE(snapshot.adapters.size(), 1);
    const Adapter &adapter = snapshot.adapters.constFirst();
    QVERIFY(adapter.name.toUtf8().size() <= 256);
    QVERIFY(!adapter.name.contains(QLatin1Char('\x01')));
    QCOMPARE(snapshot.devices.size(), 1);
    const Device &mapped = snapshot.devices.constFirst();
    // Lowercase platform spellings normalize into the canonical grammar.
    QCOMPARE(mapped.address, QStringLiteral("AA:BB:CC:33:44:55"));
    QVERIFY(mapped.name.toUtf8().size() <= 256);
    QVERIFY(!mapped.name.contains(QLatin1Char('\x01')));
    // Class above the 24-bit CoD space and an unknown icon never invent a
    // category.
    QCOMPARE(mapped.deviceClass, DeviceClass::Unknown);

    harness.fake->emitDeviceProperties(
        devicePath,
        {{QStringLiteral("Alias"), quint32(7)},
         {QStringLiteral("Class"), QStringLiteral("1344")},
         {QStringLiteral("Paired"), QStringLiteral("true")},
         {QStringLiteral("RSSI"), QStringLiteral("-5")}});
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.constFirst().name.isEmpty();
    }));
    const Device strictTypes = harness.model->snapshot().devices.constFirst();
    QCOMPARE(strictTypes.deviceClass, DeviceClass::Unknown);
    QVERIFY(!strictTypes.paired);
    QVERIFY(!strictTypes.rssiKnown);

    harness.fake->emitAdapterProperties(
        adapterPath, {{QStringLiteral("Powered"), QStringLiteral("true")}});
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().adapters.constFirst().powered;
    }));
}

void BluezAdapterBackendTests::interfaceChurnUpdatesInventory()
{
    BluezHarness harness(7005);
    QVERIFY(harness.ready());
    const QString adapterPath =
        harness.fake->addAdapter(QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
                                  QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.model->snapshot().devices.isEmpty());

    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    QVERIFY(harness.waitUntil(
        [&harness] { return harness.model->snapshot().devices.size() == 1; }));
    QVERIFY(!harness.model->snapshot().devices.constFirst().paired);

    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Paired"), true}});
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.constFirst().paired;
    }));

    // An interface the adapter does not consume must be ignored, not grafted.
    harness.fake->emitInterfacesAdded(devicePath,
                                      {{QStringLiteral("org.bluez.Battery1"),
                                        {{QStringLiteral("Percentage"), quint8(80)}}}});
    QTest::qWait(100);
    QVERIFY(!harness.model->snapshot().devices.constFirst().batteryKnown);

    harness.fake->removeDeviceObject(devicePath);
    QVERIFY(harness.waitUntil(
        [&harness] { return harness.model->snapshot().devices.isEmpty(); }));

    const QString secondAdapterPath = harness.fake->addAdapter(
        QStringLiteral("hci1"), QStringLiteral("AA:BB:CC:00:11:33"), QStringLiteral("USB"), true);
    (void)harness.fake->addDevice(secondAdapterPath, QStringLiteral("AA:BB:CC:33:44:66"),
                            QStringLiteral("Headset"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().adapters.size() == 2
            && harness.model->snapshot().devices.size() == 1;
    }));
    // Removing an adapter removes its devices with it.
    harness.fake->removeAdapterObject(secondAdapterPath);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().adapters.size() == 1
            && harness.model->snapshot().devices.isEmpty();
    }));
}

void BluezAdapterBackendTests::duplicateDeviceAddressDeduplicated()
{
    BluezHarness harness(7006);
    QVERIFY(harness.ready());
    const QString first = harness.fake->addAdapter(
        QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"), QStringLiteral("A"), true);
    const QString second = harness.fake->addAdapter(
        QStringLiteral("hci1"), QStringLiteral("AA:BB:CC:00:11:33"), QStringLiteral("B"), true);
    (void)harness.fake->addDevice(first, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("One"));
    (void)harness.fake->addDevice(second, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Two"));
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    const Snapshot snapshot = harness.model->snapshot();
    QCOMPARE(snapshot.devices.size(), 1);
    const Device &device = snapshot.devices.constFirst();
    QCOMPARE(device.name, QStringLiteral("One"));
    const auto adapterIt = std::find_if(
        snapshot.adapters.cbegin(), snapshot.adapters.cend(),
        [&device](const Adapter &adapter) {
            return adapter.handle == device.adapterHandle;
        });
    QVERIFY(adapterIt != snapshot.adapters.cend());
    QCOMPARE(adapterIt->address, QStringLiteral("AA:BB:CC:00:11:22"));
}

void BluezAdapterBackendTests::duplicateAdapterAddressDeduplicated()
{
    BluezHarness harness(7010);
    QVERIFY(harness.ready());
    const QString firstPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
        QStringLiteral("First"), true);
    const QString secondPath = harness.fake->addAdapter(
        QStringLiteral("hci1"), QStringLiteral("AA:BB:CC:00:11:22"),
        QStringLiteral("Second"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QCOMPARE(harness.model->snapshot().adapters.size(), 1);
    QCOMPARE(harness.model->snapshot().adapters.constFirst().name,
             QStringLiteral("First"));

    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission off = harness.model->submit(
        {.kind = OperationKind::SetAdapterPower,
         .target = harness.snapshotAdapter().handle,
         .powered = false},
        QStringLiteral(":1.10"));
    QVERIFY(off.pending);
    const std::optional<OperationResult> result =
        harness.awaitResult(completed, off.operationId);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, OperationStatus::Succeeded);
    QVERIFY(!harness.fake->adapter(firstPath)->powered);
    QVERIFY(harness.fake->adapter(secondPath)->powered);
}

void BluezAdapterBackendTests::ownerLossAndReturnRetiresTruth()
{
    BluezHarness harness(7007);
    QVERIFY(harness.ready());
    const QString adapterPath =
        harness.fake->addAdapter(QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
                                  QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->paired = true;
    device->connected = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());

    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission acquire = harness.model->submit(
        {.kind = OperationKind::AcquireDiscovery,
         .target = harness.snapshotAdapter().handle},
        QStringLiteral(":1.10"));
    QVERIFY(acquire.pending);
    const std::optional<OperationResult> acquired =
        harness.awaitResult(completed, acquire.operationId);
    QVERIFY(acquired.has_value());
    QCOMPARE(acquired->status, OperationStatus::Succeeded);
    QVERIFY(harness.snapshotAdapter().discovering);

    // Owner loss retires every piece of BlueZ-bound truth.
    harness.fake->dropOwnership();
    QVERIFY(harness.waitUnavailable());
    QVERIFY(harness.model->snapshot().adapters.isEmpty());

    // A returned owner republishes from a fresh, empty runtime state; the
    // lease the previous owner's session held is gone with it.
    QVERIFY(harness.fake->returnAsNewOwner());
    QVERIFY(harness.waitReady());
    QVERIFY(!harness.snapshotAdapter().discovering);
    const OperationSubmission release = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery,
         .target = harness.snapshotAdapter().handle},
        QStringLiteral(":1.10"));
    QVERIFY(release.pending);
    const std::optional<OperationResult> released =
        harness.awaitResult(completed, release.operationId);
    QVERIFY(released.has_value());
    QCOMPARE(released->status, OperationStatus::Rejected);
    QCOMPARE(released->reasonCode, QStringLiteral("no-lease"));
}

void BluezAdapterBackendTests::eventsAfterStopDoNotPublish()
{
    BluezHarness harness(7008);
    QVERIFY(harness.ready());
    const QString adapterPath =
        harness.fake->addAdapter(QStringLiteral("hci0"), QStringLiteral("AA:BB:CC:00:11:22"),
                                  QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    harness.model->stop();
    const Snapshot stopped = harness.model->snapshot();

    (void)harness.fake->addDevice(adapterPath, QStringLiteral("AA:BB:CC:33:44:55"),
                            QStringLiteral("Keyboard"));
    QTest::qWait(200);
    QCOMPARE(harness.model->snapshot(), stopped);

    // A restarted model run publishes fresh, generation-fenced truth.
    harness.model->start();
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().availability == Availability::Ready
            && harness.model->snapshot().devices.size() == 1;
    }));
}

void BluezAdapterBackendTests::invalidConnectionFailsClosed()
{
    BluezAdapterBackend backend{QDBusConnection{QStringLiteral("qindaqt-bluez-invalid")}};
    BluetoothModel model(&backend, 7009);
    model.start();
    QVERIFY(QTest::qWaitFor([&model] {
        return model.snapshot().availability == Availability::Unavailable;
    }));
    QCOMPARE(model.snapshot().reasonCode, QStringLiteral("no-adapter"));
    const OperationSubmission submission = model.submit(
        {.kind = OperationKind::SetAdapterPower, .target = {.epoch = 7009, .serial = 1}},
        QStringLiteral(":1.10"));
    QVERIFY(!submission.pending);
    QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("unavailable"));
}

QTEST_GUILESS_MAIN(BluezAdapterBackendTests)
#include "tst_bluez_adapter_backend.moc"
