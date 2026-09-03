// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"
#include "support/fake_upower_service.h"
#include "support/private_bus.h"

#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

const QString kBatteryPath =
    QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0");
const QString kUpsPath = QStringLiteral("/org/freedesktop/UPower/devices/battery_UPS0");
const QString kLinePath =
    QStringLiteral("/org/freedesktop/UPower/devices/line_power_AC");

QVariantMap batteryProperties()
{
    QVariantMap properties;
    properties.insert(QStringLiteral("Type"), QVariant(uint(2)));
    properties.insert(QStringLiteral("IsPresent"), QVariant(true));
    properties.insert(QStringLiteral("State"), QVariant(uint(2)));
    properties.insert(QStringLiteral("Percentage"), QVariant(55.5));
    properties.insert(QStringLiteral("BatteryLevel"), QVariant(uint(1)));
    properties.insert(QStringLiteral("Energy"), QVariant(27.5));
    properties.insert(QStringLiteral("EnergyFull"), QVariant(50.0));
    properties.insert(QStringLiteral("EnergyRate"), QVariant(9.5));
    properties.insert(QStringLiteral("TimeToEmpty"), QVariant(qint64(10'432)));
    properties.insert(QStringLiteral("Vendor"), QVariant(QStringLiteral("QindaQt")));
    properties.insert(QStringLiteral("Model"), QVariant(QStringLiteral("Fixture cell")));
    properties.insert(QStringLiteral("WarningLevel"), QVariant(uint(2)));
    return properties;
}

FakeUpowerService::DeviceSpec batteryDevice()
{
    return {kBatteryPath, batteryProperties()};
}

FakeUpowerService::DeviceSpec linePowerDevice()
{
    QVariantMap properties;
    properties.insert(QStringLiteral("Type"), QVariant(uint(1)));
    properties.insert(QStringLiteral("IsPresent"), QVariant(true));
    return {kLinePath, properties};
}

// One adapter plus coordinator running with fake sibling collaborators, so
// every row asserts the public snapshot outcome rather than adapter internals.
struct UpowerRow
{
    std::unique_ptr<PrivateBus> bus;
    std::unique_ptr<FakeUpowerService> fake;
    QDBusConnection fakeConnection{QStringLiteral("invalid-fake")};
    std::unique_ptr<Upstream::UpowerBatteryCollaborator> adapter;
    FakeProfileCollaborator profiles;
    FakeSessionCollaborator session;
    std::unique_ptr<PowerServiceCoordinator> coordinator;
    std::unique_ptr<QSignalSpy> snapshotSpy;

    bool start(const QList<FakeUpowerService::DeviceSpec> &devices, bool onBattery)
    {
        bus = std::make_unique<PrivateBus>();
        if (!bus->start()) {
            return false;
        }
        fakeConnection = bus->openConnection(QStringLiteral("fake"));
        fake = std::make_unique<FakeUpowerService>(fakeConnection);
        if (!fake->registerService()) {
            return false;
        }
        fake->setDevices(devices);
        fake->setOnBattery(onBattery);
        adapter = std::make_unique<Upstream::UpowerBatteryCollaborator>(
            bus->connection);
        coordinator = std::make_unique<PowerServiceCoordinator>(adapter.get(), &profiles,
                                                               &session);
        snapshotSpy = std::make_unique<QSignalSpy>(
            coordinator.get(), &PowerServiceCoordinator::snapshotChanged);
        coordinator->start();
        return true;
    }
};

} // namespace

class PowerUpowerAdapterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesSuppliesAcAndOnBatteryTruth();
    void coarseBatteryLevelSuppressesPercentage();
    void absentOptionalPropertiesYieldUnknownFlags();
    void deviceAddedAndRemovedUpdateSupplies();
    void devicePropertiesChangedUpdatesSupply();
    void onBatteryServicePropertyChangeUpdatesTruth();
    void ownerLossFailsClosedUnavailable();
    void ownerReplacementInvalidatesEpoch();
    void hostileWrongTypedPercentageFailsClosedMalformed();
    void hostileOutOfRangeEstimateDegradesThroughSanitization();
    void hostileUnknownStateOrdinalFailsClosedMalformed();
    void oversizeEnumerationDegradesThroughSanitization();
    void disconnectedBusFailsClosed();
    void keyboardOperationStaysUnsupported();
    void mergedProductionFactsCarryInternalBacklights();
    void sysfsTruthWithheldUntilUpowerFactsExist();
};

void PowerUpowerAdapterTests::publishesSuppliesAcAndOnBatteryTruth()
{
    UpowerRow row;
    QVERIFY(row.start({batteryDevice(), linePowerDevice(),
                       [] {
                           QVariantMap properties;
                           properties.insert(QStringLiteral("Type"), QVariant(uint(3)));
                           properties.insert(QStringLiteral("IsPresent"), QVariant(true));
                           properties.insert(QStringLiteral("Percentage"),
                                             QVariant(90.0));
                           properties.insert(QStringLiteral("BatteryLevel"),
                                             QVariant(uint(1)));
                           return FakeUpowerService::DeviceSpec{kUpsPath, properties};
                       }()},
                      true));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 2);
    const Snapshot snapshot = row.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QVERIFY(snapshot.capabilities.testFlag(Capability::Supplies));
    QVERIFY(snapshot.source.onBattery);
    QVERIFY(snapshot.source.acPresent);
    const PowerSupply &supply = snapshot.supplies.constFirst();
    QCOMPARE(supply.kind, SupplyKind::Battery);
    QVERIFY(supply.present);
    QVERIFY(supply.percentageKnown);
    QCOMPARE(supply.percentage, 55.5);
    QCOMPARE(supply.level, BatteryLevel::None);
    QCOMPARE(supply.state, ChargeState::Discharging);
    QVERIFY(supply.energyKnown);
    QCOMPARE(supply.energyWattHours, 27.5);
    QCOMPARE(supply.energyFullWattHours, 50.0);
    QVERIFY(supply.rateKnown);
    QCOMPARE(supply.energyRateWatts, 9.5);
    QVERIFY(supply.timeToEmptyKnown);
    QCOMPARE(supply.timeToEmptySeconds, qint64(10'432));
    QVERIFY(!supply.timeToFullKnown);
    QCOMPARE(supply.warning, WarningLevel::Discharging);
    QCOMPARE(supply.vendor, QStringLiteral("QindaQt"));
    QCOMPARE(supply.handle.opaqueId.size(), 32);
    // PB-0 aggregation stays the composite authority.
    QVERIFY(snapshot.composite.present);
    QCOMPARE(snapshot.composite.sourceCount, quint32(2));
}

void PowerUpowerAdapterTests::coarseBatteryLevelSuppressesPercentage()
{
    QVariantMap properties = batteryProperties();
    properties.insert(QStringLiteral("BatteryLevel"), QVariant(uint(3)));
    properties.remove(QStringLiteral("TimeToEmpty"));
    UpowerRow row;
    QVERIFY(row.start({{kBatteryPath, properties}}, false));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    const PowerSupply &supply = row.coordinator->snapshot().supplies.constFirst();
    QCOMPARE(supply.level, BatteryLevel::Critical);
    QVERIFY(!supply.percentageKnown);
}

void PowerUpowerAdapterTests::absentOptionalPropertiesYieldUnknownFlags()
{
    QVariantMap properties;
    properties.insert(QStringLiteral("Type"), QVariant(uint(2)));
    properties.insert(QStringLiteral("IsPresent"), QVariant(true));
    UpowerRow row;
    QVERIFY(row.start({{kBatteryPath, properties}}, false));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    const PowerSupply &supply = row.coordinator->snapshot().supplies.constFirst();
    QVERIFY(supply.present);
    QVERIFY(!supply.percentageKnown);
    QCOMPARE(supply.level, BatteryLevel::Unknown);
    QCOMPARE(supply.state, ChargeState::Unknown);
    QVERIFY(!supply.energyKnown);
    QVERIFY(!supply.rateKnown);
    QVERIFY(!supply.timeToEmptyKnown);
    QVERIFY(!supply.timeToFullKnown);
}

void PowerUpowerAdapterTests::deviceAddedAndRemovedUpdateSupplies()
{
    UpowerRow row;
    QVERIFY(row.start({}, false));
    QTRY_VERIFY(row.snapshotSpy->size() >= 1);
    QCOMPARE(row.coordinator->snapshot().supplies.size(), 0);

    row.fake->setDevices({batteryDevice()});
    row.fake->emitDeviceAdded(kBatteryPath);
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);

    row.fake->setDevices({});
    row.fake->emitDeviceRemoved(kBatteryPath);
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 0);
}

void PowerUpowerAdapterTests::devicePropertiesChangedUpdatesSupply()
{
    UpowerRow row;
    QVERIFY(row.start({batteryDevice()}, true));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);

    QVariantMap properties = batteryProperties();
    properties.insert(QStringLiteral("Percentage"), QVariant(90.0));
    properties.insert(QStringLiteral("State"), QVariant(uint(1)));
    row.fake->setDevices({{kBatteryPath, properties}});
    row.fake->emitDevicePropertiesChanged(kBatteryPath);
    QTRY_COMPARE(row.coordinator->snapshot().supplies.constFirst().percentage, 90.0);
    QCOMPARE(row.coordinator->snapshot().supplies.constFirst().state,
             ChargeState::Charging);
}

void PowerUpowerAdapterTests::onBatteryServicePropertyChangeUpdatesTruth()
{
    UpowerRow row;
    QVERIFY(row.start({batteryDevice()}, true));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    QVERIFY(row.coordinator->snapshot().source.onBattery);

    row.fake->setOnBattery(false);
    row.fake->emitServicePropertiesChanged();
    QTRY_VERIFY(!row.coordinator->snapshot().source.onBattery);
}

void PowerUpowerAdapterTests::ownerLossFailsClosedUnavailable()
{
    UpowerRow row;
    QVERIFY(row.start({batteryDevice()}, true));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    row.profiles.publish(fixtureProfileFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("upower-unavailable"));
    // Fail closed: battery content does not outlive its authority.
    QCOMPARE(row.coordinator->snapshot().supplies.size(), 0);
    QVERIFY(!row.coordinator->snapshot().capabilities.testFlag(Capability::Supplies));
}

void PowerUpowerAdapterTests::ownerReplacementInvalidatesEpoch()
{
    UpowerRow row;
    QVERIFY(row.start({batteryDevice()}, true));
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    const quint64 firstEpoch = row.coordinator->snapshot().epoch;
    const QString firstHandle =
        row.coordinator->snapshot().supplies.constFirst().handle.opaqueId;

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    row.fakeConnection = row.bus->openConnection(QStringLiteral("replacement"));
    row.fake = std::make_unique<FakeUpowerService>(row.fakeConnection);
    QVERIFY(row.fake->registerService());
    row.fake->setDevices({batteryDevice()});
    row.fake->setOnBattery(true);
    QTRY_VERIFY(row.coordinator->snapshot().epoch > firstEpoch);
    QTRY_COMPARE(row.coordinator->snapshot().supplies.size(), 1);
    QCOMPARE(row.coordinator->snapshot().supplies.constFirst().handle.opaqueId,
             firstHandle);
    QCOMPARE(row.coordinator->snapshot().supplies.constFirst().handle.epoch,
             row.coordinator->snapshot().epoch);
}

void PowerUpowerAdapterTests::hostileWrongTypedPercentageFailsClosedMalformed()
{
    QVariantMap properties = batteryProperties();
    properties.insert(QStringLiteral("Percentage"),
                      QVariant(QStringLiteral("not-a-double")));
    UpowerRow row;
    QVERIFY(row.start({{kBatteryPath, properties}}, false));
    row.profiles.publish(fixtureProfileFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("upower-malformed"));
    QCOMPARE(row.coordinator->snapshot().supplies.size(), 0);
}

void PowerUpowerAdapterTests::hostileOutOfRangeEstimateDegradesThroughSanitization()
{
    QVariantMap properties = batteryProperties();
    properties.insert(QStringLiteral("TimeToEmpty"), QVariant(qint64(400'000'000)));
    UpowerRow row;
    QVERIFY(row.start({{kBatteryPath, properties}}, false));
    row.profiles.publish(fixtureProfileFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("battery-malformed"));
}

void PowerUpowerAdapterTests::hostileUnknownStateOrdinalFailsClosedMalformed()
{
    QVariantMap properties = batteryProperties();
    properties.insert(QStringLiteral("State"), QVariant(uint(99)));
    UpowerRow row;
    QVERIFY(row.start({{kBatteryPath, properties}}, false));
    row.profiles.publish(fixtureProfileFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("upower-malformed"));
}

void PowerUpowerAdapterTests::oversizeEnumerationDegradesThroughSanitization()
{
    QList<FakeUpowerService::DeviceSpec> devices;
    for (int index = 0; index < 9; ++index) {
        QVariantMap properties;
        properties.insert(QStringLiteral("Type"), QVariant(uint(2)));
        properties.insert(QStringLiteral("IsPresent"), QVariant(true));
        devices.push_back({QStringLiteral("/org/freedesktop/UPower/devices/battery_B%1")
                               .arg(index),
                           properties});
    }
    UpowerRow row;
    QVERIFY(row.start(devices, false));
    row.profiles.publish(fixtureProfileFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("battery-malformed"));
}

void PowerUpowerAdapterTests::disconnectedBusFailsClosed()
{
    Upstream::UpowerBatteryCollaborator adapter(
        QDBusConnection(QStringLiteral("upower-disconnected")));
    QSignalSpy unavailable(&adapter, &BatteryCollaborator::statusUnavailable);
    const quint64 generation = adapter.start();
    QVERIFY(generation != 0);
    QTRY_COMPARE(unavailable.size(), 1);
    QCOMPARE(unavailable.first().at(0).toULongLong(), generation);
    QCOMPARE(unavailable.first().at(1).toString(),
             QStringLiteral("upower-bus-unavailable"));
}

void PowerUpowerAdapterTests::keyboardOperationStaysUnsupported()
{
    Upstream::UpowerBatteryCollaborator adapter(
        QDBusConnection(QStringLiteral("upower-disconnected-2")));
    QSignalSpy finished(&adapter, &BatteryCollaborator::operationFinished);
    adapter.submitSetKeyboardBrightness(
        7, Handle{.epoch = 1, .opaqueId = QStringLiteral("kbd")}, 10);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().at(1).toULongLong(), quint64(7));
    const CollaboratorOutcome outcome =
        finished.first().at(2).value<CollaboratorOutcome>();
    QCOMPARE(outcome.status, CollaboratorStatus::Unsupported);
    QCOMPARE(outcome.reasonCode, QStringLiteral("keyboard-backlight-unsupported"));
}

void writeFixtureFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
    file.close();
}

void PowerUpowerAdapterTests::mergedProductionFactsCarryInternalBacklights()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString panel = root.path() + QStringLiteral("/panel");
    writeFixtureFile(panel + QStringLiteral("/type"), QByteArrayLiteral("firmware\n"));
    writeFixtureFile(panel + QStringLiteral("/max_brightness"),
                     QByteArrayLiteral("255\n"));
    writeFixtureFile(panel + QStringLiteral("/brightness"),
                     QByteArrayLiteral("100\n"));

    PrivateBus bus;
    QVERIFY(bus.start());
    QDBusConnection fakeConnection = bus.openConnection(QStringLiteral("fake"));
    FakeUpowerService fake(fakeConnection);
    QVERIFY(fake.registerService());
    fake.setDevices({batteryDevice()});
    fake.setOnBattery(true);

    auto upower =
        std::make_unique<Upstream::UpowerBatteryCollaborator>(bus.connection);
    auto backlights = std::make_unique<Upstream::SysfsBacklightSource>(root.path());
    Upstream::ProductionBatteryCollaborator production(std::move(upower),
                                                       std::move(backlights));
    QSignalSpy facts(&production, &BatteryCollaborator::factsChanged);
    production.start();
    QTRY_VERIFY(facts.size() >= 1);
    const BatteryFacts merged =
        facts.last().at(1).value<BatteryFacts>();
    QCOMPARE(merged.supplies.size(), 1);
    QCOMPARE(merged.internalBacklights.size(), 1);
    QCOMPARE(merged.internalBacklights.constFirst().deviceName,
             QStringLiteral("panel"));
    production.stop();
}

void PowerUpowerAdapterTests::sysfsTruthWithheldUntilUpowerFactsExist()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QDBusConnection fakeConnection = bus.openConnection(QStringLiteral("fake"));
    FakeUpowerService fake(fakeConnection);
    // The UPower authority is deliberately absent at start.

    auto upower =
        std::make_unique<Upstream::UpowerBatteryCollaborator>(bus.connection);
    auto backlights =
        std::make_unique<Upstream::SysfsBacklightSource>(QStringLiteral("/nonexistent"));
    Upstream::ProductionBatteryCollaborator production(std::move(upower),
                                                       std::move(backlights));
    QSignalSpy unavailable(&production, &BatteryCollaborator::statusUnavailable);
    QSignalSpy facts(&production, &BatteryCollaborator::factsChanged);
    production.start();
    QTRY_COMPARE(unavailable.size(), 1);
    QCOMPARE(facts.size(), 0);
    production.stop();
    QDBusConnection::disconnectFromBus(fakeConnection.name());
}

QTEST_GUILESS_MAIN(PowerUpowerAdapterTests)
#include "tst_power_upower_adapter.moc"
