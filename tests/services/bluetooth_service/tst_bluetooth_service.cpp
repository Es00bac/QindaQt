// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_adapter_backend.h"

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtCore/QCoreApplication>
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
    void residentCompositionWiresModelToOwner();
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

void BluetoothServiceTests::residentCompositionWiresModelToOwner()
{
    auto backend = std::make_unique<FakeAdapterBackend>();
    FakeAdapterBackend *backendPtr = backend.get();
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        QSKIP("This composition check requires a session bus connection.");
    }
    const QString serviceName = QStringLiteral("org.qindaqt.BluetoothServiceTest.%1")
                                    .arg(QCoreApplication::applicationPid());
    ResidentBluetoothService service(std::move(backend), connection, serviceName, 7002);
    QCOMPARE(service.start(), ServiceStartStatus::Started);
    QVERIFY(service.isRunning());
    QVERIFY(service.model() != nullptr);
    QCOMPARE(backendPtr->startCalls, 1);

    // A second owner of the same well-known name is refused, not stolen.
    auto secondBackend = std::make_unique<FakeAdapterBackend>();
    ResidentBluetoothService second(std::move(secondBackend), connection, serviceName,
                                     7003);
    QCOMPARE(second.start(), ServiceStartStatus::NameAlreadyOwned);
    QVERIFY(!second.isRunning());
    QVERIFY(second.model() != nullptr);

    service.stop();
    QVERIFY(!service.isRunning());
    QCOMPARE(backendPtr->stopCalls, 1);

    // The name is free again after stop, so a new owner can take it.
    ResidentBluetoothService third(std::make_unique<FakeAdapterBackend>(), connection,
                                   serviceName, 7004);
    QCOMPARE(third.start(), ServiceStartStatus::Started);
    third.stop();
}

QTEST_GUILESS_MAIN(BluetoothServiceTests)
#include "tst_bluetooth_service.moc"
