// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"
#include "support/fake_upower_service.h"
#include "support/upower_activation_bus.h"

#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

FakeUpowerService::DeviceSpec startupBattery(const double percentage = 28.0)
{
    return {QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"),
            {{QStringLiteral("Type"), QVariant(uint(2))},
             {QStringLiteral("PowerSupply"), QVariant(true)},
             {QStringLiteral("IsPresent"), QVariant(true)},
             {QStringLiteral("State"), QVariant(uint(1))},
             {QStringLiteral("Percentage"), QVariant(percentage)},
             {QStringLiteral("BatteryLevel"), QVariant(uint(1))}}};
}

std::unique_ptr<Upstream::ProductionBatteryCollaborator> productionFor(
    const UpowerActivationBus &bus)
{
    return std::make_unique<Upstream::ProductionBatteryCollaborator>(
        std::make_unique<Upstream::UpowerBatteryCollaborator>(bus.connection),
        std::make_unique<Upstream::SysfsBacklightSource>(
            bus.root.filePath(QStringLiteral("backlight"))));
}

// The daemon activates this same noninstalled test executable. All arguments
// and the starter address must match the private descriptor before any bus use.
int runActivationHelper(QCoreApplication &application)
{
    QString address;
    QString fixtureRoot;
    int delayMs = -1;
    bool fail = false;
    for (const QString &argument : application.arguments()) {
        if (argument.startsWith(QStringLiteral("--bus-address=")))
            address = argument.mid(14);
        else if (argument.startsWith(QStringLiteral("--fixture-root=")))
            fixtureRoot = argument.mid(15);
        else if (argument.startsWith(QStringLiteral("--delay-ms=")))
            delayMs = argument.mid(11).toInt();
        else if (argument == QStringLiteral("--fail=1"))
            fail = true;
    }
    if (!address.startsWith(QStringLiteral("unix:abstract=qindaqt-upower-startup-"))
        || address != qEnvironmentVariable("DBUS_STARTER_ADDRESS").section(QLatin1Char(','), 0, 0)
        || fixtureRoot.isEmpty() || delayMs < 0) return 2;
    const QDBusConnection connection = QDBusConnection::connectToBus(
        address, QStringLiteral("activated-private-upower"));
    if (!connection.isConnected()) return 3;
    QFile marker(fixtureRoot + QStringLiteral("/helper-pid"));
    if (!marker.open(QIODevice::WriteOnly)) return 4;
    marker.write(QByteArray::number(QCoreApplication::applicationPid()));
    marker.close();
    FakeUpowerService fake(connection);
    fake.setDevices({startupBattery()});
    fake.setOnBattery(false);
    QTimer::singleShot(delayMs, &application, [&] {
        if (fail || !fake.registerService()) {
            QFile done(fixtureRoot + QStringLiteral("/helper-done"));
            if (done.open(QIODevice::WriteOnly)) done.write("activation-failed\n");
            application.exit(5);
        }
    });
    QTimer disconnectPoll;
    QObject::connect(&disconnectPoll, &QTimer::timeout, &application, [&] {
        if (!connection.isConnected()) application.quit();
    });
    disconnectPoll.start(50);
    QTimer::singleShot(15000, &application, &QCoreApplication::quit);
    return application.exec();
}

} // namespace

class PowerUpowerStartupTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dormantProductionStartupProvidesSnapshot();
    void failedActivationWithholdsBatteryAndBacklights();
    void activationTimeoutIsBoundedAndRecoversOnOwnerArrival();
    void stopSuppressesActivationResult();
    void restartPublishesOnlyNewGeneration();
    void destroyDuringActivationIsSafe();
    void competingOwnerDuringActivationStaysAuthoritative();
};

void PowerUpowerStartupTests::dormantProductionStartupProvidesSnapshot()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(350));
    QVERIFY(bus.owner().isEmpty());
    const QDBusReply<QStringList> activatable = bus.connection.interface()->call(
        QStringLiteral("ListActivatableNames"));
    QVERIFY(activatable.isValid());
    QVERIFY(activatable.value().contains(QStringLiteral("org.freedesktop.UPower")));
    auto production = productionFor(bus);
    FakeProfileCollaborator profiles;
    FakeSessionCollaborator session;
    PowerServiceCoordinator coordinator(production.get(), &profiles, &session);
    QSignalSpy unavailable(production.get(), &BatteryCollaborator::statusUnavailable);
    bool heartbeat = false;
    QTimer::singleShot(30, &coordinator, [&] { heartbeat = true; });
    QElapsedTimer elapsed;
    elapsed.start();
    coordinator.start();
    QVERIFY2(elapsed.elapsed() < 100, "Dormant activation must not block startup");
    profiles.publish(fixtureProfileFacts());
    session.publish(fixtureSessionFacts());
    QTRY_VERIFY_WITH_TIMEOUT(heartbeat, 250);
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.snapshot().supplies.size(), 1, 5000);
    QVERIFY(bus.helperStarted());
    QCOMPARE(unavailable.size(), 0);
    const Snapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QCOMPARE(snapshot.internalBacklights.size(), 1);
    QVERIFY(snapshot.supplies.first().percentageKnown);
    QCOMPARE(snapshot.supplies.first().percentage, 28.0);
    QCOMPARE(snapshot.supplies.first().state, ChargeState::Charging);
    QVERIFY(!snapshot.source.onBattery);
}

void PowerUpowerStartupTests::failedActivationWithholdsBatteryAndBacklights()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(100, true));
    auto production = productionFor(bus);
    QSignalSpy facts(production.get(), &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(production.get(), &BatteryCollaborator::statusUnavailable);
    const quint64 generation = production->start();
    QTRY_COMPARE_WITH_TIMEOUT(unavailable.size(), 1, 5000);
    QVERIFY(bus.helperStarted());
    QVERIFY(bus.helperFinished());
    QCOMPARE(unavailable.first().at(0).toULongLong(), generation);
    QCOMPARE(unavailable.first().at(1).toString(), QStringLiteral("upower-unavailable"));
    QCOMPARE(facts.size(), 0);
    QVERIFY(bus.owner().isEmpty());
}

void PowerUpowerStartupTests::activationTimeoutIsBoundedAndRecoversOnOwnerArrival()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(4500));
    auto production = productionFor(bus);
    QSignalSpy facts(production.get(), &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(production.get(), &BatteryCollaborator::statusUnavailable);
    bool heartbeat = false;
    QTimer::singleShot(50, production.get(), [&] { heartbeat = true; });
    QElapsedTimer elapsed;
    elapsed.start();
    const quint64 generation = production->start();
    QTRY_VERIFY_WITH_TIMEOUT(heartbeat, 250);
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperStarted(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(unavailable.size(), 1, 4000);
    QVERIFY2(elapsed.elapsed() >= 2500, "The fixture must exercise a pending activation timeout");
    QCOMPARE(unavailable.first().at(0).toULongLong(), generation);
    QCOMPARE(unavailable.first().at(1).toString(), QStringLiteral("upower-unavailable"));
    QCOMPARE(facts.size(), 0);
    // A later owner notification starts a new read cycle, never revives the
    // failed activation callback or replays a control operation.
    QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 3000);
    QCOMPARE(facts.last().at(1).value<BatteryFacts>().supplies.first().percentage, 28.0);
    QCOMPARE(unavailable.size(), 1);
}

void PowerUpowerStartupTests::stopSuppressesActivationResult()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(350));
    Upstream::UpowerBatteryCollaborator adapter(bus.connection);
    QSignalSpy facts(&adapter, &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(&adapter, &BatteryCollaborator::statusUnavailable);
    adapter.start();
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperStarted(), 1000);
    adapter.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!bus.owner().isEmpty(), 2000);
    QTest::qWait(100);
    QCOMPARE(facts.size(), 0);
    QCOMPARE(unavailable.size(), 0);
}

void PowerUpowerStartupTests::restartPublishesOnlyNewGeneration()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(350));
    Upstream::UpowerBatteryCollaborator adapter(bus.connection);
    QSignalSpy facts(&adapter, &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(&adapter, &BatteryCollaborator::statusUnavailable);
    const quint64 oldGeneration = adapter.start();
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperStarted(), 1000);
    adapter.stop();
    const quint64 newGeneration = adapter.start();
    QVERIFY(newGeneration > oldGeneration);
    QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 2000);
    for (const QList<QVariant> &event : facts)
        QCOMPARE(event.at(0).toULongLong(), newGeneration);
    QCOMPARE(unavailable.size(), 0);
}

void PowerUpowerStartupTests::destroyDuringActivationIsSafe()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(350));
    auto adapter = std::make_unique<Upstream::UpowerBatteryCollaborator>(bus.connection);
    QPointer<Upstream::UpowerBatteryCollaborator> lifetime(adapter.get());
    QSignalSpy facts(adapter.get(), &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(adapter.get(), &BatteryCollaborator::statusUnavailable);
    adapter->start();
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperStarted(), 1000);
    adapter.reset();
    QVERIFY(lifetime.isNull());
    QTRY_VERIFY_WITH_TIMEOUT(!bus.owner().isEmpty(), 2000);
    QTest::qWait(100);
    QCOMPARE(facts.size(), 0);
    QCOMPARE(unavailable.size(), 0);
}

void PowerUpowerStartupTests::competingOwnerDuringActivationStaysAuthoritative()
{
    UpowerActivationBus bus;
    QVERIFY(bus.start(500));
    Upstream::UpowerBatteryCollaborator adapter(bus.connection);
    QSignalSpy facts(&adapter, &BatteryCollaborator::factsChanged);
    QSignalSpy unavailable(&adapter, &BatteryCollaborator::statusUnavailable);
    const quint64 generation = adapter.start();
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperStarted(), 1000);
    FakeUpowerService competing(bus.openConnection());
    competing.setDevices({startupBattery(77.0)});
    competing.setOnBattery(true);
    QVERIFY(competing.registerService());
    QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(bus.helperFinished(), 2000);
    QTest::qWait(100);
    for (const QList<QVariant> &event : facts) {
        QCOMPARE(event.at(0).toULongLong(), generation);
        const BatteryFacts truth = event.at(1).value<BatteryFacts>();
        QCOMPARE(truth.supplies.size(), 1);
        QCOMPARE(truth.supplies.first().percentage, 77.0);
        QVERIFY(truth.onBattery);
    }
    QCOMPARE(unavailable.size(), 0);
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains(QStringLiteral("--activation-helper")))
        return runActivationHelper(application);
    PowerUpowerStartupTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_power_upower_startup.moc"
