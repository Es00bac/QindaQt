// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

struct CoordinatorHarness
{
    FakeBatteryCollaborator battery;
    FakeProfileCollaborator profiles;
    FakeSessionCollaborator session;
    std::unique_ptr<PowerServiceCoordinator> coordinator;

    CoordinatorHarness()
    {
        coordinator = std::make_unique<PowerServiceCoordinator>(&battery, &profiles,
                                                                &session);
        coordinator->start();
    }
};

void publishValidDomains(FakeBatteryCollaborator &battery,
                         FakeProfileCollaborator &profiles,
                         FakeSessionCollaborator &session)
{
    battery.publish(fixtureBatteryFacts());
    profiles.publish(fixtureProfileFacts());
    session.publish(fixtureSessionFacts());
}

ProfileFacts fixtureProfileFactsCollidingWithBattery()
{
    ProfileFacts facts = fixtureProfileFacts();
    facts.profiles.holds.first().handle.opaqueId = QStringLiteral("battery-bat0");
    return facts;
}

} // namespace

class PowerServicePublicationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initialSnapshotIsStarting();
    void validDomainsPublishReadySnapshot();
    void malformedBatteryDomainDegradesOnlyThatDomain();
    void batteryIdentityWinsAcrossFactArrivalOrder_data();
    void batteryIdentityWinsAcrossFactArrivalOrder();
    void sanitizedHostileTextIsPublishedBounded();
    void allDomainsUnavailablePublishesUnavailable();
    void mixedValidAndUnavailablePublishesDegraded();
    void staleGenerationFactsAreIgnored();
    void authorityReplacementAdvancesEpochAndRestampsHandles();
    void restartClearsContentAndAdvancesEpoch();
    void everyPublishedSnapshotIsValid();
};

void PowerServicePublicationTests::initialSnapshotIsStarting()
{
    CoordinatorHarness harness;
    const Snapshot &snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Starting);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("starting"));
    QVERIFY(snapshot.epoch != 0);
    QCOMPARE(snapshot.revision, quint64(1));
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::validDomainsPublishReadySnapshot()
{
    CoordinatorHarness harness;
    QSignalSpy changed(harness.coordinator.get(),
                       &PowerServiceCoordinator::invalidated);
    publishValidDomains(harness.battery, harness.profiles, harness.session);

    const Snapshot snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("ready"));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Supplies));
    QVERIFY(snapshot.capabilities.testFlag(Capability::KeyboardBacklight));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Profiles));
    QVERIFY(snapshot.capabilities.testFlag(Capability::ProfileHolds));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Inhibitors));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Lid));
    QVERIFY(!snapshot.capabilities.testFlag(Capability::InternalBacklight));
    QVERIFY(!snapshot.capabilities.testFlag(Capability::IdleHint));
    QCOMPARE(snapshot.supplies.size(), 1);
    QCOMPARE(snapshot.supplies.first().handle.epoch, snapshot.epoch);
    QCOMPARE(snapshot.supplies.first().handle.opaqueId,
             QStringLiteral("battery-bat0"));
    QCOMPARE(snapshot.keyboardBacklights.size(), 1);
    QCOMPARE(snapshot.keyboardBacklights.first().handle.epoch, snapshot.epoch);
    QCOMPARE(snapshot.profiles.supported.size(), 3);
    QCOMPARE(snapshot.profiles.activeProfileId, QStringLiteral("balanced"));
    QCOMPARE(snapshot.profiles.holds.size(), 1);
    QCOMPARE(snapshot.profiles.holds.first().handle.epoch, snapshot.epoch);
    QCOMPARE(snapshot.inhibitors.size(), 1);
    QVERIFY(snapshot.source.onBattery);
    QVERIFY(snapshot.source.lidPresent);
    QVERIFY(snapshot.composite.present);
    QCOMPARE(snapshot.composite.sourceCount, quint32(1));
    QVERIFY(snapshot.composite.percentageKnown);
    // Energy-weighted aggregation: 27.5/50.0 Wh scaled by 100 is exactly 55.
    QCOMPARE(snapshot.composite.percentage, 55.0);
    QCOMPARE(snapshot.composite.state, ChargeState::Discharging);
    QCOMPARE(snapshot.composite.timeToEmptySeconds, qint64(10'432));
    QVERIFY(!snapshot.waylandBinding.available);
    QVERIFY(snapshot.internalBacklights.isEmpty());
    QCOMPARE(changed.size(), 3);
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::malformedBatteryDomainDegradesOnlyThatDomain()
{
    CoordinatorHarness harness;
    publishValidDomains(harness.battery, harness.profiles, harness.session);
    const Snapshot accepted = harness.coordinator->snapshot();

    BatteryFacts malformed = fixtureBatteryFacts();
    malformed.supplies.push_back(fixtureSupply(QStringLiteral("battery-bat1")));
    for (int i = 0; i < kMaxPowerSupplies; ++i) {
        malformed.supplies.push_back(
            fixtureSupply(QStringLiteral("battery-overflow-%1").arg(i)));
    }
    harness.battery.publish(malformed);

    const Snapshot degraded = harness.coordinator->snapshot();
    QCOMPARE(degraded.availability, Availability::Degraded);
    QCOMPARE(degraded.reasonCode, QStringLiteral("battery-malformed"));
    QVERIFY(degraded.epoch == accepted.epoch);
    QVERIFY(degraded.revision > accepted.revision);
    QVERIFY(degraded.supplies.isEmpty());
    QVERIFY(degraded.keyboardBacklights.isEmpty());
    QVERIFY(!degraded.capabilities.testFlag(Capability::Supplies));
    QVERIFY(!degraded.composite.present);
    // Last-known-good: the untouched domains keep their accepted content.
    QCOMPARE(degraded.profiles.activeProfileId, QStringLiteral("balanced"));
    QCOMPARE(degraded.inhibitors.size(), 1);
    QVERIFY(degraded.capabilities.testFlag(Capability::Profiles));
    QVERIFY(validateSnapshot(degraded).accepted);
}

void PowerServicePublicationTests::batteryIdentityWinsAcrossFactArrivalOrder_data()
{
    QTest::addColumn<bool>("profileFactsArriveFirst");

    QTest::newRow("profile-before-battery") << true;
    QTest::newRow("battery-before-profile") << false;
}

void PowerServicePublicationTests::batteryIdentityWinsAcrossFactArrivalOrder()
{
    QFETCH(bool, profileFactsArriveFirst);

    CoordinatorHarness harness;
    const ProfileFacts collidingProfiles = fixtureProfileFactsCollidingWithBattery();
    if (profileFactsArriveFirst) {
        harness.profiles.publish(collidingProfiles);
        harness.session.publish(fixtureSessionFacts());
        harness.battery.publish(fixtureBatteryFacts());
    } else {
        harness.battery.publish(fixtureBatteryFacts());
        harness.session.publish(fixtureSessionFacts());
        harness.profiles.publish(collidingProfiles);
    }

    const Snapshot snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Degraded);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("profile-malformed"));
    QCOMPARE(snapshot.supplies.size(), 1);
    QCOMPARE(snapshot.supplies.first().handle.opaqueId,
             QStringLiteral("battery-bat0"));
    QCOMPARE(snapshot.keyboardBacklights.size(), 1);
    QCOMPARE(snapshot.inhibitors.size(), 1);
    QVERIFY(snapshot.source.onBattery);
    QVERIFY(snapshot.source.lidPresent);
    QVERIFY(snapshot.capabilities.testFlag(Capability::Supplies));
    QVERIFY(snapshot.capabilities.testFlag(Capability::KeyboardBacklight));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Inhibitors));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Lid));
    QVERIFY(!snapshot.capabilities.testFlag(Capability::Profiles));
    QVERIFY(!snapshot.capabilities.testFlag(Capability::ProfileHolds));
    QVERIFY(snapshot.profiles.supported.isEmpty());
    QVERIFY(snapshot.profiles.holds.isEmpty());
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::sanitizedHostileTextIsPublishedBounded()
{
    CoordinatorHarness harness;
    BatteryFacts hostile = fixtureBatteryFacts();
    PowerSupply &supply = hostile.supplies.first();
    supply.vendor = QString::fromUtf8("Ven\x01dor\u2068Inline");
    supply.model = QString(kMaxNameUtf8Bytes * 2, QLatin1Char('m'));
    supply.handle.opaqueId = QStringLiteral("battery-bat0");
    harness.battery.publish(hostile);
    harness.profiles.publish(fixtureProfileFacts());
    harness.session.publish(fixtureSessionFacts());

    const Snapshot snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    const QString vendor = snapshot.supplies.first().vendor;
    QVERIFY(!vendor.contains(QChar(0x01)));
    QVERIFY(!vendor.contains(QChar(0x2068)));
    QVERIFY(vendor.toUtf8().size() <= kMaxNameUtf8Bytes);
    QVERIFY(snapshot.supplies.first().model.toUtf8().size() <= kMaxNameUtf8Bytes);
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::allDomainsUnavailablePublishesUnavailable()
{
    CoordinatorHarness harness;
    harness.battery.publishUnavailable(QStringLiteral("no-upower"));
    harness.profiles.publishUnavailable(QStringLiteral("no-profile-daemon"));
    harness.session.publishUnavailable(QStringLiteral("no-logind"));

    const Snapshot snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Unavailable);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("no-upower"));
    QCOMPARE(snapshot.capabilities, Capabilities{});
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::mixedValidAndUnavailablePublishesDegraded()
{
    CoordinatorHarness harness;
    harness.battery.publish(fixtureBatteryFacts());
    harness.profiles.publishUnavailable(QStringLiteral("no-profile-daemon"));
    harness.session.publish(fixtureSessionFacts());

    const Snapshot snapshot = harness.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Degraded);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("no-profile-daemon"));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Supplies));
    QVERIFY(!snapshot.capabilities.testFlag(Capability::Profiles));
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void PowerServicePublicationTests::staleGenerationFactsAreIgnored()
{
    CoordinatorHarness harness;
    const Snapshot initial = harness.coordinator->snapshot();
    const quint64 supersededRun = harness.battery.generation;
    harness.coordinator->stop();
    harness.coordinator->start();

    // A late emission from the superseded collaborator run must not mutate
    // any state, and generation zero can never claim authority.
    harness.battery.publishForGeneration(supersededRun, fixtureBatteryFacts());
    QCOMPARE(harness.coordinator->snapshot().revision, initial.revision);
    QCOMPARE(harness.coordinator->snapshot().availability, Availability::Starting);

    harness.battery.publishUnavailableForGeneration(0, QStringLiteral("stale"));
    QCOMPARE(harness.coordinator->snapshot().revision, initial.revision);

    // The current run still publishes normally.
    harness.battery.publish(fixtureBatteryFacts());
    QVERIFY(harness.coordinator->snapshot().revision > initial.revision);
    QCOMPARE(harness.coordinator->snapshot().supplies.size(), 1);
}

void PowerServicePublicationTests::authorityReplacementAdvancesEpochAndRestampsHandles()
{
    CoordinatorHarness harness;
    publishValidDomains(harness.battery, harness.profiles, harness.session);
    const Snapshot before = harness.coordinator->snapshot();
    const QString supplyId = before.supplies.first().handle.opaqueId;

    harness.profiles.replaceAuthority();

    const Snapshot after = harness.coordinator->snapshot();
    QVERIFY(after.epoch > before.epoch);
    QVERIFY(after.revision > before.revision);
    QCOMPARE(after.supplies.first().handle.epoch, after.epoch);
    QCOMPARE(after.supplies.first().handle.opaqueId, supplyId);
    QCOMPARE(after.profiles.holds.first().handle.epoch, after.epoch);
    QCOMPARE(after.availability, Availability::Ready);
    QVERIFY(validateSnapshot(after).accepted);
}

void PowerServicePublicationTests::restartClearsContentAndAdvancesEpoch()
{
    CoordinatorHarness harness;
    publishValidDomains(harness.battery, harness.profiles, harness.session);
    const Snapshot before = harness.coordinator->snapshot();

    harness.coordinator->stop();
    harness.coordinator->start();

    const Snapshot restarting = harness.coordinator->snapshot();
    QCOMPARE(restarting.availability, Availability::Starting);
    QCOMPARE(restarting.reasonCode, QStringLiteral("collaborators-restarting"));
    QVERIFY(restarting.epoch > before.epoch);
    QVERIFY(restarting.revision > before.revision);
    QCOMPARE(restarting.capabilities, Capabilities{});
    QVERIFY(restarting.supplies.isEmpty());
    QVERIFY(validateSnapshot(restarting).accepted);

    harness.battery.publish(fixtureBatteryFacts());
    harness.profiles.publish(fixtureProfileFacts());
    harness.session.publish(fixtureSessionFacts());
    const Snapshot ready = harness.coordinator->snapshot();
    QCOMPARE(ready.availability, Availability::Ready);
    QCOMPARE(ready.epoch, restarting.epoch);
    QVERIFY(validateSnapshot(ready).accepted);
}

void PowerServicePublicationTests::everyPublishedSnapshotIsValid()
{
    CoordinatorHarness harness;
    QSignalSpy published(harness.coordinator.get(),
                         &PowerServiceCoordinator::snapshotChanged);
    publishValidDomains(harness.battery, harness.profiles, harness.session);

    BatteryFacts hostile = fixtureBatteryFacts();
    hostile.supplies.first().percentage = 500.0; // out of range
    harness.battery.publish(hostile);
    harness.battery.publish(fixtureBatteryFacts());

    ProfileFacts duplicateHold = fixtureProfileFacts();
    duplicateHold.profiles.holds.push_back(duplicateHold.profiles.holds.first());
    harness.profiles.publish(duplicateHold);

    ProfileFacts crossDomain = fixtureProfileFacts();
    crossDomain.profiles.holds.first().handle.opaqueId =
        QStringLiteral("battery-bat0");
    harness.profiles.publish(crossDomain);

    SessionFacts lidContradiction;
    lidContradiction.lidPresent = false;
    lidContradiction.lidClosed = true;
    harness.session.publish(lidContradiction);

    for (const QList<QVariant> &arguments : published) {
        const Snapshot snapshot = arguments.first().value<Snapshot>();
        QVERIFY2(validateSnapshot(snapshot).accepted,
                 qPrintable(snapshot.reasonCode));
    }
}

QTEST_GUILESS_MAIN(PowerServicePublicationTests)
#include "tst_power_service_publication.moc"
