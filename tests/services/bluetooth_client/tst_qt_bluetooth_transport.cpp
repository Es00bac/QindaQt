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
                          .paired = true,
                          .connected = false,
                          .rssiKnown = true,
                          .rssi = -52}};
    return inventory;
}

} // namespace

class QtBluetoothTransportTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void successiveOwnersLeasesAndOperations();
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
        QStringLiteral("type=\"(uutuussa((tt)ssbb)a((tt)(tt)ssubbbbn))\"")));
    QVERIFY(introspection.contains(QStringLiteral("type=\"(uuttttss)\"")));
    QVERIFY(introspection.contains(QStringLiteral("name=\"AcquireDiscovery\"")));

    QtBluetoothTransport transport(bus.connection, serviceName);
    BluetoothClient client(&transport);
    QSignalSpy completed(&client, &BluetoothClient::operationCompleted);
    client.start();
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(9001));
    QVERIFY(client.owner().startsWith(QLatin1Char(':')));

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
    auto secondHost = std::make_unique<ResidentBluetoothService>(
        std::move(secondBackend), secondConnection, serviceName, 9002);
    QCOMPARE(secondHost->start(), ServiceStartStatus::Started);
    // The empty replacement backend truthfully publishes no-adapter until its
    // inventory is populated.
    QTRY_COMPARE(client.state(), ClientState::Unavailable);
    secondBackend->setInventory(populatedInventory());
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(9002));
    QVERIFY(client.owner() != firstOwner);
    // The old-epoch handle is stale against the new authority.
    const quint64 stale = client.connectDevice(deviceHandle);
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(completed.last()[0].toULongLong(), stale);
    QCOMPARE(completed.last()[1].value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));
    secondHost->stop();
    secondHost.reset();
    QDBusConnection::disconnectFromBus(secondConnectionName);
    client.stop();
}

QTEST_GUILESS_MAIN(QtBluetoothTransportTests)
#include "tst_qt_bluetooth_transport.moc"
