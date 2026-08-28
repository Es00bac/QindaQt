// SPDX-License-Identifier: GPL-3.0-or-later

#include "deterministic_adapter_backend.h"

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>
#include <qindaqt/services/bluetooth_client/qt_bluetooth_transport.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

#include <memory>

using namespace QindaQt::Bluetooth;

namespace
{

class PrivateBus final
{
public:
    bool start()
    {
        process.setProgram(QStringLiteral("dbus-daemon"));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-bluetooth-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

BackendInventory populatedInventory()
{
    BackendInventory inventory;
    inventory.adapters = {{.address = QStringLiteral("AA:BB:CC:00:11:22"),
                           .name = QStringLiteral("Internal adapter"),
                           .powered = true,
                           .discovering = false}};
    inventory.devices = {{.adapterAddress = QStringLiteral("AA:BB:CC:00:11:22"),
                          .address = QStringLiteral("AA:BB:CC:33:44:55"),
                          .name = QStringLiteral("Keyboard"),
                          .deviceClass = DeviceClass::Keyboard,
                          .role = DeviceRole::Peripheral,
                          .paired = true,
                          .connected = false,
                          .rssiKnown = true,
                          .rssi = -52,
                          .batteryKnown = true,
                          .batteryPercent = 87}};
    return inventory;
}

// AGENT-NOTE: A hostile service that exploits the wire's freedom beyond the
// protocol bounds: the writer will happily marshal more devices than v1
// allows, so only the client-side bounded decode can catch the payload. A
// locally built QDBusArgument cannot be demarshalled, so this row is what
// actually exercises the real-wire bounded decode.
class HostileSnapshotService final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Bluetooth1")

public:
    explicit HostileSnapshotService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE Snapshot GetSnapshot() const
    {
        Snapshot snapshot;
        snapshot.schemaVersion = kSchemaVersion;
        snapshot.epoch = 77;
        snapshot.revision = 1;
        snapshot.availability = Availability::Ready;
        snapshot.capabilities = Capability::SetAdapterPower | Capability::DiscoveryLease
            | Capability::ConnectPaired | Capability::DisconnectPaired;
        snapshot.reasonCode = QStringLiteral("ready");
        snapshot.adapters = {{.handle = {.epoch = 77, .serial = 400},
                              .address = QStringLiteral("AA:BB:CC:00:11:22"),
                              .name = QStringLiteral("Hostile adapter"),
                              .powered = true,
                              .discovering = false}};
        for (int index = 0; index <= kMaxDevices; ++index) {
            Device device;
            device.handle = {.epoch = 77, .serial = static_cast<quint64>(1000 + index)};
            device.adapterHandle = {.epoch = 77, .serial = 400};
            device.address = QStringLiteral("AA:BB:CC:33:44:%1")
                                 .arg(56 + index, 2, 10, QLatin1Char('0'));
            device.name = QStringLiteral("Hostile %1").arg(index);
            device.deviceClass = DeviceClass::Unknown;
            device.role = DeviceRole::Unknown;
            device.paired = true;
            snapshot.devices.push_back(device);
        }
        return snapshot;
    }

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);
};

} // namespace

class QtBluetoothTransportTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void successiveOwnersLeasesAndOperations();
    void hostileOversizedWireSnapshotIsRejected();
};

void QtBluetoothTransportTests::successiveOwnersLeasesAndOperations()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.BluetoothTest.p%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString firstConnectionName = bus.name + QStringLiteral("-service-1");
    QDBusConnection firstConnection =
        QDBusConnection::connectToBus(bus.address, firstConnectionName);
    QVERIFY(firstConnection.isConnected());

    auto firstBackend = std::make_unique<DeterministicAdapterBackend>();
    DeterministicAdapterBackend *firstBackendPtr = firstBackend.get();
    auto firstHost = std::make_unique<ResidentBluetoothService>(
        std::move(firstBackend), firstConnection, serviceName, 9001);
    QCOMPARE(firstHost->start(), ServiceStartStatus::Started);
    firstBackendPtr->setInventory(populatedInventory());

    const QDBusMessage introspectionCall = QDBusMessage::createMethodCall(
        serviceName, QString::fromLatin1(kObjectPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));
    QDBusPendingCallWatcher introspectionWatcher(
        bus.connection.asyncCall(introspectionCall));
    QSignalSpy introspectionFinished(&introspectionWatcher,
                                     &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(introspectionFinished.size(), 1);
    const QDBusPendingReply<QString> introspectionReply = introspectionWatcher;
    QVERIFY2(!introspectionReply.isError(),
             qPrintable(introspectionReply.error().message()));
    const QString introspection = introspectionReply.value();
    QVERIFY(introspection.contains(
        QStringLiteral("type=\"(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnby))\"")));
    QVERIFY(introspection.contains(QStringLiteral("type=\"(uuttttss)\"")));
    QVERIFY(introspection.contains(QStringLiteral("name=\"AcquireDiscovery\"")));

    QtBluetoothTransport transport(bus.connection, serviceName);
    BluetoothClient client(&transport);
    QSignalSpy completed(&client, &BluetoothClient::operationCompleted);
    client.start();
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(9001));
    QVERIFY(client.owner().startsWith(QLatin1Char(':')));
    // AGENT-CONTRACT under test: the wire round trip is faithful. The client
    // snapshot decoded over the real bus must equal the model snapshot value
    // for value, including battery and role representation.
    QCOMPARE(client.snapshot(), firstHost->model()->snapshot());

    // A paired-device connect round trip through the real adaptor, model, and
    // wire codecs.
    const Handle deviceHandle = client.snapshot().devices.constFirst().handle;
    const quint64 requestId = client.connectDevice(deviceHandle);
    QTRY_COMPARE(firstBackendPtr->submitCalls(), 1);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(completed[0][1].value<OperationResult>().initiatingEpoch, quint64(9001));
    QTRY_COMPARE(client.snapshot().devices.constFirst().connected, true);

    // A discovery lease acquired by this test's unique name holds after the
    // call completes and appears in the published snapshot.
    const Handle adapterHandleValue = client.snapshot().adapters.constFirst().handle;
    const quint64 leaseId = client.acquireDiscovery(adapterHandleValue);
    QTRY_COMPARE(firstBackendPtr->submitCalls(), 2);
    QTRY_COMPARE(client.snapshot().adapters.constFirst().discovering, true);
    QVERIFY(leaseId != 0);

    const QString firstOwner = client.owner();
    firstHost->stop();
    firstHost.reset();
    QDBusConnection::disconnectFromBus(firstConnectionName);
    QTRY_COMPARE(client.state(), ClientState::Unavailable);

    // The replacement owner must issue a fresh epoch and a fresh snapshot.
    const QString secondConnectionName = bus.name + QStringLiteral("-service-2");
    QDBusConnection secondConnection =
        QDBusConnection::connectToBus(bus.address, secondConnectionName);
    QVERIFY(secondConnection.isConnected());
    auto secondBackend = std::make_unique<DeterministicAdapterBackend>();
    DeterministicAdapterBackend *secondBackendPtr = secondBackend.get();
    auto secondHost = std::make_unique<ResidentBluetoothService>(
        std::move(secondBackend), secondConnection, serviceName, 9002);
    QCOMPARE(secondHost->start(), ServiceStartStatus::Started);
    // The empty replacement backend truthfully publishes no-adapter until its
    // inventory is populated.
    QTRY_COMPARE(client.state(), ClientState::Unavailable);
    secondBackendPtr->setInventory(populatedInventory());
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(9002));
    QVERIFY(client.owner() != firstOwner);
    // The old-epoch handle is stale against the new authority.
    const quint64 stale = client.connectDevice(deviceHandle);
    QTRY_COMPARE(completed.count(), 3);
    QCOMPARE(completed.last()[0].toULongLong(), stale);
    QCOMPARE(completed.last()[1].value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));
    secondHost->stop();
    secondHost.reset();
    QDBusConnection::disconnectFromBus(secondConnectionName);
    client.stop();
}

void QtBluetoothTransportTests::hostileOversizedWireSnapshotIsRejected()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString hostileName = QStringLiteral("org.qindaqt.BluetoothHostile.p%1")
                                    .arg(QCoreApplication::applicationPid());
    HostileSnapshotService hostile;
    QVERIFY(bus.connection.registerObject(QString::fromLatin1(kObjectPath), &hostile,
                                          QDBusConnection::ExportScriptableSlots
                                              | QDBusConnection::ExportScriptableSignals));
    QVERIFY(bus.connection.registerService(hostileName));

    QtBluetoothTransport transport(bus.connection, hostileName);
    BluetoothClient client(&transport);
    QSignalSpy snapshots(&client, &BluetoothClient::snapshotChanged);
    client.start();
    // The bounded decode of the real wire payload must mark the oversized
    // array malformed, reject the whole snapshot, and revoke authority.
    QTRY_COMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QVERIFY(!client.hasSnapshot());
    QCOMPARE(snapshots.count(), 0);

    // No mutation can dispatch against the hostile payload's authority.
    const quint64 requestId = client.connectDevice({.epoch = 77, .serial = 1000});
    QVERIFY(requestId != 0);
    QTRY_COMPARE(snapshots.count(), 0);
    QVERIFY(!client.operationPending());

    bus.connection.unregisterObject(QString::fromLatin1(kObjectPath));
    bus.connection.unregisterService(hostileName);
    client.stop();
}

QTEST_GUILESS_MAIN(QtBluetoothTransportTests)
#include "tst_qt_bluetooth_transport.moc"
