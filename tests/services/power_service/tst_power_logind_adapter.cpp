// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_logind_service.h"
#include "support/fake_power_collaborators.h"
#include "support/private_bus.h"

#include <qindaqt/services/power_service/adapters/logind_session_collaborator.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

struct SessionRow
{
    std::unique_ptr<PrivateBus> bus;
    std::unique_ptr<FakeLogindService> fake;
    QDBusConnection fakeConnection{QStringLiteral("invalid-fake")};
    std::unique_ptr<Upstream::LogindSessionCollaborator> adapter;
    FakeBatteryCollaborator battery;
    FakeProfileCollaborator profiles;
    std::unique_ptr<PowerServiceCoordinator> coordinator;

    bool start(bool lidClosed, bool docked)
    {
        bus = std::make_unique<PrivateBus>();
        if (!bus->start()) {
            return false;
        }
        fakeConnection = bus->openConnection(QStringLiteral("fake"));
        fake = std::make_unique<FakeLogindService>(fakeConnection);
        if (!fake->registerService()) {
            return false;
        }
        fake->setSessionTruth(lidClosed, docked, false);
        adapter =
            std::make_unique<Upstream::LogindSessionCollaborator>(bus->connection);
        coordinator = std::make_unique<PowerServiceCoordinator>(
            &battery, &profiles, adapter.get());
        coordinator->start();
        return true;
    }
};

FakeLogindService::InhibitorSpec inhibitor(const QString &what, const QString &who,
                                           const QString &why, const QString &mode)
{
    return {what, who, why, mode, quint32(4242), quint32(9999)};
}

} // namespace

class PowerLogindAdapterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesLidDockAndSanitizedInhibitors();
    void openLidKeepsProvenLidPresent();
    void neverClosedLidReportsNoLid();
    void prepareForSleepSignalFlipsTruthAndRereadsInhibitors();
    void propertiesChangedUpdatesLidAndDock();
    void hostileOversizeInhibitorsDegradeThroughSanitization();
    void ownerLossFailsClosedUnavailable();
    void ownerReplacementInvalidatesEpoch();
    void absentServiceStaysUnavailable();
    void disconnectedBusFailsClosed();
};

void PowerLogindAdapterTests::publishesLidDockAndSanitizedInhibitors()
{
    SessionRow row;
    QVERIFY(row.start(true, true));
    row.fake->setInhibitors(
        {inhibitor(QStringLiteral("sleep"), QStringLiteral("QindaQt"),
                   QStringLiteral("fixture inhibitor"), QStringLiteral("delay")),
         inhibitor(QStringLiteral("shutdown"), QStringLiteral("Updater"),
                   QStringLiteral("updates running"), QStringLiteral("block"))});
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().inhibitors.size(), 2);

    const Snapshot snapshot = row.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QVERIFY(snapshot.capabilities.testFlag(Capability::Lid));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Inhibitors));
    QVERIFY(snapshot.source.lidPresent);
    QVERIFY(snapshot.source.lidClosed);
    QVERIFY(snapshot.source.docked);
    QVERIFY(!snapshot.source.preparingForSleep);
    const Inhibitor &first = snapshot.inhibitors.constFirst();
    QCOMPARE(first.what, QStringLiteral("sleep"));
    QCOMPARE(first.who, QStringLiteral("QindaQt"));
    QCOMPARE(first.why, QStringLiteral("fixture inhibitor"));
    QCOMPARE(first.mode, QStringLiteral("delay"));
}

void PowerLogindAdapterTests::openLidKeepsProvenLidPresent()
{
    SessionRow row;
    QVERIFY(row.start(true, false));
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_VERIFY(row.coordinator->snapshot().source.lidPresent);

    row.fake->setSessionTruth(false, false, false);
    row.fake->emitManagerPropertiesChanged();
    QTRY_VERIFY(!row.coordinator->snapshot().source.lidClosed);
    // A proven lid stays proven once the lid opens again.
    QVERIFY(row.coordinator->snapshot().source.lidPresent);
}

void PowerLogindAdapterTests::neverClosedLidReportsNoLid()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);
    // logind exposes no lid-present property; an unproven lid is absent truth,
    // never a guessed present one.
    QVERIFY(!row.coordinator->snapshot().source.lidPresent);
    QVERIFY(!row.coordinator->snapshot().source.lidClosed);
}

void PowerLogindAdapterTests::prepareForSleepSignalFlipsTruthAndRereadsInhibitors()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    row.fake->setInhibitors(
        {inhibitor(QStringLiteral("sleep"), QStringLiteral("QindaQt"),
                   QStringLiteral("before"), QStringLiteral("delay"))});
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().inhibitors.size(), 1);
    const int callsBefore = row.fake->listInhibitorsCalls();
    QVERIFY(callsBefore >= 1);

    row.fake->emitPrepareForSleep(true);
    QTRY_VERIFY(row.coordinator->snapshot().source.preparingForSleep);

    row.fake->setInhibitors({});
    row.fake->emitPrepareForSleep(false);
    QTRY_VERIFY(!row.coordinator->snapshot().source.preparingForSleep);
    // Resume re-reads the inhibitor set because it changes across sleep.
    QTRY_COMPARE(row.coordinator->snapshot().inhibitors.size(), 0);
    QVERIFY(row.fake->listInhibitorsCalls() > callsBefore);
}

void PowerLogindAdapterTests::propertiesChangedUpdatesLidAndDock()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    row.fake->setSessionTruth(true, true, false);
    row.fake->emitManagerPropertiesChanged();
    QTRY_VERIFY(row.coordinator->snapshot().source.lidClosed);
    QTRY_VERIFY(row.coordinator->snapshot().source.docked);
}

void PowerLogindAdapterTests::hostileOversizeInhibitorsDegradeThroughSanitization()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    QList<FakeLogindService::InhibitorSpec> inhibitors;
    for (int index = 0; index < 9; ++index) {
        inhibitors.push_back(inhibitor(QStringLiteral("sleep") + QString::number(index),
                                       QStringLiteral("who"), QStringLiteral("why"),
                                       QStringLiteral("delay")));
    }
    row.fake->setInhibitors(inhibitors);
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("session-malformed"));
}

void PowerLogindAdapterTests::ownerLossFailsClosedUnavailable()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("logind-unavailable"));
    QCOMPARE(row.coordinator->snapshot().inhibitors.size(), 0);
}

void PowerLogindAdapterTests::ownerReplacementInvalidatesEpoch()
{
    SessionRow row;
    QVERIFY(row.start(false, false));
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);
    const quint64 firstEpoch = row.coordinator->snapshot().epoch;

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    row.fakeConnection = row.bus->openConnection(QStringLiteral("replacement"));
    row.fake = std::make_unique<FakeLogindService>(row.fakeConnection);
    QVERIFY(row.fake->registerService());
    row.fake->setSessionTruth(true, false, false);
    row.battery.publish(fixtureBatteryFacts());
    row.profiles.publish(fixtureProfileFacts());
    QTRY_VERIFY(row.coordinator->snapshot().epoch > firstEpoch);
    QTRY_VERIFY(row.coordinator->snapshot().source.lidClosed);
}

void PowerLogindAdapterTests::absentServiceStaysUnavailable()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeBatteryCollaborator battery;
    FakeProfileCollaborator profiles;
    Upstream::LogindSessionCollaborator adapter(bus.connection);
    PowerServiceCoordinator coordinator(&battery, &profiles, &adapter);
    coordinator.start();
    battery.publish(fixtureBatteryFacts());
    profiles.publish(fixtureProfileFacts());
    QTRY_COMPARE(coordinator.snapshot().availability, Availability::Degraded);
    QCOMPARE(coordinator.snapshot().reasonCode, QStringLiteral("logind-unavailable"));
}

void PowerLogindAdapterTests::disconnectedBusFailsClosed()
{
    Upstream::LogindSessionCollaborator adapter(
        QDBusConnection(QStringLiteral("logind-disconnected")));
    QSignalSpy unavailable(&adapter, &SessionCollaborator::statusUnavailable);
    const quint64 generation = adapter.start();
    QVERIFY(generation != 0);
    QTRY_COMPARE(unavailable.size(), 1);
    QCOMPARE(unavailable.first().at(1).toString(),
             QStringLiteral("logind-bus-unavailable"));
}

QTEST_GUILESS_MAIN(PowerLogindAdapterTests)
#include "tst_power_logind_adapter.moc"
