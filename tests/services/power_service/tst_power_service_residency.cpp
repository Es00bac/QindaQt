// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"

#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_service/resident_power_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

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
        name = QStringLiteral("qindaqt-power-test-%1")
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

std::unique_ptr<ResidentPowerService> residentOnBus(const QDBusConnection &connection,
                                                    const QString &serviceName,
                                                    FakeBatteryCollaborator **batteryOut,
                                                    FakeProfileCollaborator **profilesOut,
                                                    FakeSessionCollaborator **sessionOut = nullptr)
{
    auto battery = std::make_unique<FakeBatteryCollaborator>();
    auto profiles = std::make_unique<FakeProfileCollaborator>();
    auto session = std::make_unique<FakeSessionCollaborator>();
    if (batteryOut != nullptr) {
        *batteryOut = battery.get();
    }
    if (profilesOut != nullptr) {
        *profilesOut = profiles.get();
    }
    if (sessionOut != nullptr) {
        *sessionOut = session.get();
    }
    auto service = std::make_unique<ResidentPowerService>(
        std::move(battery), std::move(profiles), std::move(session), connection,
        serviceName);
    return service;
}

} // namespace

class PowerServiceResidencyTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesSnapshotAndChangedSignalOverBus();
    void delayedOperationRepliesExactlyOnce();
    void secondOwnerTakesNameAndPublishesNewEpoch();
    void alreadyOwnedNameReportsTheft();
    void introspectionExposesExactPower1Signatures();

private Q_SLOTS:
    void onChanged(quint64 epoch, quint64 revision)
    {
        Q_EMIT changedReceived(epoch, revision);
    }

Q_SIGNALS:
    void changedReceived(quint64 epoch, quint64 revision);
};

void PowerServiceResidencyTests::publishesSnapshotAndChangedSignalOverBus()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName =
        QStringLiteral("org.qindaqt.PowerTest.r%1").arg(QCoreApplication::applicationPid());
    const QString connectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, connectionName);
    QVERIFY(serviceConnection.isConnected());

    FakeBatteryCollaborator *battery = nullptr;
    auto service = residentOnBus(serviceConnection, serviceName, &battery, nullptr);
    QCOMPARE(service->start(), PowerServiceStartStatus::Started);

    QVERIFY(bus.connection.connect(
        serviceName, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("Changed"), this,
        SLOT(onChanged(quint64, quint64))));
    QSignalSpy changed(this, &PowerServiceResidencyTests::changedReceived);
    battery->publish(fixtureBatteryFacts());
    QTRY_COMPARE(changed.size(), 1);
    const quint64 signalledEpoch = changed.first().at(0).toULongLong();
    const quint64 signalledRevision = changed.first().at(1).toULongLong();

    QDBusPendingCallWatcher pending(
        bus.connection.asyncCall(QDBusMessage::createMethodCall(
            serviceName, QString::fromLatin1(kObjectPath),
            QString::fromLatin1(kInterfaceName), QStringLiteral("GetSnapshot"))));
    QSignalSpy replied(&pending, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(replied.size(), 1);
    const QDBusPendingReply<Snapshot> reply = pending;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    const Snapshot snapshot = reply.value();
    QCOMPARE(snapshot.availability, Availability::Starting);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("starting"));
    QCOMPARE(snapshot.supplies.size(), 1);
    QCOMPARE(snapshot.epoch, signalledEpoch);
    QCOMPARE(snapshot.revision, signalledRevision);
    QVERIFY(snapshot.epoch != 0);

    bus.connection.disconnect(serviceName, QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Changed"), this,
                              SLOT(onChanged(quint64, quint64)));
    service->stop();
    QDBusConnection::disconnectFromBus(connectionName);
}

void PowerServiceResidencyTests::delayedOperationRepliesExactlyOnce()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.PowerTest.o%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString connectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, connectionName);
    QVERIFY(serviceConnection.isConnected());

    FakeBatteryCollaborator *battery = nullptr;
    FakeProfileCollaborator *profiles = nullptr;
    FakeSessionCollaborator *session = nullptr;
    auto service = residentOnBus(serviceConnection, serviceName, &battery, &profiles,
                                 &session);
    QCOMPARE(service->start(), PowerServiceStartStatus::Started);
    battery->publish(fixtureBatteryFacts());
    profiles->publish(fixtureProfileFacts());
    session->publish(fixtureSessionFacts());

    const Snapshot current = service->coordinator()->snapshot();
    QCOMPARE(current.availability, Availability::Ready);
    QDBusMessage call = QDBusMessage::createMethodCall(
        serviceName, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("SetProfile"));
    call.setArguments({QStringLiteral("performance")});
    QDBusPendingCallWatcher pending(bus.connection.asyncCall(call));
    QSignalSpy finished(&pending, &QDBusPendingCallWatcher::finished);
    // The delayed reply arrives only after the upstream collaborator finishes.
    QTRY_COMPARE(profiles->profileOperations.size(), 1);
    profiles->finish(profiles->profileOperations.first().operationId,
                     succeededOutcome());
    QTRY_COMPARE(finished.size(), 1);
    const QDBusPendingReply<OperationResult> reply = pending;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.value().status, OperationStatus::Succeeded);
    QCOMPARE(reply.value().initiatingEpoch, current.epoch);
    QVERIFY(reply.value().observedRevision >= reply.value().initiatingRevision);

    service->stop();
    QDBusConnection::disconnectFromBus(connectionName);
}

void PowerServiceResidencyTests::secondOwnerTakesNameAndPublishesNewEpoch()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.PowerTest.s%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString firstConnectionName = bus.name + QStringLiteral("-first");
    QDBusConnection firstConnection =
        QDBusConnection::connectToBus(bus.address, firstConnectionName);
    QVERIFY(firstConnection.isConnected());
    FakeBatteryCollaborator *firstBattery = nullptr;
    auto first = residentOnBus(firstConnection, serviceName, &firstBattery, nullptr);
    QCOMPARE(first->start(), PowerServiceStartStatus::Started);
    firstBattery->publish(fixtureBatteryFacts());
    const quint64 firstEpoch = first->coordinator()->snapshot().epoch;

    // A second resident on another connection cannot steal the live name.
    auto second = residentOnBus(bus.connection, serviceName, nullptr, nullptr);
    QCOMPARE(second->start(), PowerServiceStartStatus::NameAlreadyOwned);
    QVERIFY(!second->isRunning());

    first->stop();
    first.reset();
    QDBusConnection::disconnectFromBus(firstConnectionName);

    FakeBatteryCollaborator *secondBattery = nullptr;
    auto third = residentOnBus(bus.connection, serviceName, &secondBattery, nullptr);
    QCOMPARE(third->start(), PowerServiceStartStatus::Started);
    secondBattery->publish(fixtureBatteryFacts());
    const quint64 thirdEpoch = third->coordinator()->snapshot().epoch;
    QVERIFY(thirdEpoch != firstEpoch);
    third->stop();
}

void PowerServiceResidencyTests::alreadyOwnedNameReportsTheft()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.PowerTest.t%1")
                                    .arg(QCoreApplication::applicationPid());
    QVERIFY(bus.connection.registerService(serviceName));
    const QString connectionName = bus.name + QStringLiteral("-loser");
    QDBusConnection loserConnection =
        QDBusConnection::connectToBus(bus.address, connectionName);
    QVERIFY(loserConnection.isConnected());

    // The foreign owner keeps the name; the late resident reports the theft
    // without unregistering anything.
    auto service = residentOnBus(loserConnection, serviceName, nullptr, nullptr);
    QCOMPARE(service->start(), PowerServiceStartStatus::NameAlreadyOwned);
    QVERIFY(!service->isRunning());
    QCOMPARE(bus.connection.interface()->serviceOwner(serviceName).value(),
             bus.connection.baseService());
    service.reset();
    QDBusConnection::disconnectFromBus(connectionName);
    bus.connection.unregisterService(serviceName);
}

void PowerServiceResidencyTests::introspectionExposesExactPower1Signatures()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.PowerTest.i%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString connectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, connectionName);
    QVERIFY(serviceConnection.isConnected());
    auto service = residentOnBus(serviceConnection, serviceName, nullptr, nullptr);
    QCOMPARE(service->start(), PowerServiceStartStatus::Started);

    QDBusPendingCallWatcher pending(
        bus.connection.asyncCall(QDBusMessage::createMethodCall(
            serviceName, QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"))));
    QSignalSpy replied(&pending, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(replied.size(), 1);
    const QDBusPendingReply<QString> reply = pending;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    const QString introspection = reply.value();
    const QString snapshotSignature = QStringLiteral(
        "(uttuuss(bbbbbbb)(bubduubdbxbxu)a((ts)ussbbduubddbdbxbxu)(sa(ss)a((ts)sss)s)"
        "a(ssss)a((ts)sbuuub)a((ts)sbuubuuus)(bsut))");
    QVERIFY2(introspection.contains(
                 QStringLiteral("type=\"%1\"").arg(snapshotSignature)),
             qPrintable(introspection));
    QVERIFY(introspection.contains(QStringLiteral("type=\"(uuttttss)\"")));
    QVERIFY(introspection.contains(QStringLiteral("type=\"(tt)\"")));
    QVERIFY(introspection.contains(QStringLiteral("name=\"AcquireProfileHold\"")));
    QVERIFY(introspection.contains(QStringLiteral("name=\"SetKeyboardBrightness\"")));

    service->stop();
    QDBusConnection::disconnectFromBus(connectionName);
}

QTEST_GUILESS_MAIN(PowerServiceResidencyTests)
#include "tst_power_service_residency.moc"
