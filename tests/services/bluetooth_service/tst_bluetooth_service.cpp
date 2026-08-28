// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_adapter_backend.h"
#include "support/private_bus.h"

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <memory>

using namespace QindaQt::Bluetooth;
using namespace QindaQt::Tests;

class BluetoothServiceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void invalidConnectionFailsClosed();
    void privateBusCompositionOwnership();
};

void BluetoothServiceTests::invalidConnectionFailsClosed()
{
    auto backend = std::make_unique<FakeAdapterBackend>();
    FakeAdapterBackend *backendPtr = backend.get();
    QDBusConnection invalid{QStringLiteral("qindaqt-bluetooth-invalid-test")};
    ResidentBluetoothService service(std::move(backend), invalid, {}, 7001);
    QCOMPARE(service.start(), ServiceStartStatus::InvalidConnection);
    QCOMPARE(backendPtr->startCalls, 0);
    QVERIFY(!service.isRunning());
    QVERIFY(service.model() != nullptr);
    QCOMPARE(service.model()->snapshot().availability, Availability::Starting);

    // stop() is idempotent on a never-started service.
    service.stop();
    QVERIFY(!service.isRunning());
}

void BluetoothServiceTests::privateBusCompositionOwnership()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.BluetoothServiceTest.p%1")
                                    .arg(QCoreApplication::applicationPid());

    const QString firstConnectionName = bus.name + QStringLiteral("-first");
    QDBusConnection firstConnection =
        QDBusConnection::connectToBus(bus.address, firstConnectionName);
    QVERIFY(firstConnection.isConnected());
    auto backend = std::make_unique<FakeAdapterBackend>();
    FakeAdapterBackend *backendPtr = backend.get();
    auto service = std::make_unique<ResidentBluetoothService>(
        std::move(backend), firstConnection, serviceName, 7002);
    QCOMPARE(service->start(), ServiceStartStatus::Started);
    QVERIFY(service->isRunning());
    QVERIFY(service->model() != nullptr);
    QCOMPARE(backendPtr->startCalls, 1);

    // A second owner of the same well-known name is refused, not stolen.
    const QString secondConnectionName = bus.name + QStringLiteral("-second");
    QDBusConnection secondConnection =
        QDBusConnection::connectToBus(bus.address, secondConnectionName);
    QVERIFY(secondConnection.isConnected());
    auto secondBackend = std::make_unique<FakeAdapterBackend>();
    ResidentBluetoothService second(std::move(secondBackend), secondConnection,
                                     serviceName, 7003);
    QCOMPARE(second.start(), ServiceStartStatus::NameAlreadyOwned);
    QVERIFY(!second.isRunning());

    service->stop();
    QVERIFY(!service->isRunning());
    QCOMPARE(backendPtr->stopCalls, 1);
    service.reset();
    QDBusConnection::disconnectFromBus(firstConnectionName);

    // The name is free again after stop, so a new owner can take it.
    const QString thirdConnectionName = bus.name + QStringLiteral("-third");
    QDBusConnection thirdConnection =
        QDBusConnection::connectToBus(bus.address, thirdConnectionName);
    QVERIFY(thirdConnection.isConnected());
    ResidentBluetoothService third(std::make_unique<FakeAdapterBackend>(), thirdConnection,
                                   serviceName, 7004);
    QCOMPARE(third.start(), ServiceStartStatus::Started);
    third.stop();
    QDBusConnection::disconnectFromBus(secondConnectionName);
    QDBusConnection::disconnectFromBus(thirdConnectionName);
}

QTEST_GUILESS_MAIN(BluetoothServiceTests)
#include "tst_bluetooth_service.moc"
