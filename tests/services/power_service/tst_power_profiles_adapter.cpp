// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_collaborators.h"
#include "support/fake_ppd_service.h"
#include "support/private_bus.h"

#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

struct ProfilesRow
{
    std::unique_ptr<PrivateBus> bus;
    std::unique_ptr<FakePpdService> fake;
    QDBusConnection fakeConnection{QStringLiteral("invalid-fake")};
    std::unique_ptr<Upstream::PowerProfilesCollaborator> adapter;
    FakeBatteryCollaborator battery;
    FakeSessionCollaborator session;
    std::unique_ptr<PowerServiceCoordinator> coordinator;
    std::unique_ptr<QSignalSpy> snapshotSpy;

    bool start(bool legacyOnly = false)
    {
        bus = std::make_unique<PrivateBus>();
        if (!bus->start()) {
            return false;
        }
        fakeConnection = bus->openConnection(QStringLiteral("fake"));
        fake = std::make_unique<FakePpdService>(fakeConnection, legacyOnly);
        if (!fake->registerService()) {
            return false;
        }
        fake->setProfiles({QStringLiteral("power-saver"), QStringLiteral("balanced"),
                           QStringLiteral("performance")});
        fake->setActiveProfile(QStringLiteral("balanced"));
        adapter = std::make_unique<Upstream::PowerProfilesCollaborator>(
            bus->connection);
        coordinator = std::make_unique<PowerServiceCoordinator>(
            &battery, adapter.get(), &session);
        snapshotSpy = std::make_unique<QSignalSpy>(
            coordinator.get(), &PowerServiceCoordinator::snapshotChanged);
        coordinator->start();
        return true;
    }
};

} // namespace

class PowerProfilesAdapterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesProfilesHoldsAndActiveProfile();
    void legacyNameAndInterfaceStillServeFacts();
    void setProfileUsesStandardPropertySet();
    void setProfileErrorCompletesFailed();
    void acquireHoldRecordsUpstreamAndIsReleasable();
    void foreignHoldIsNotReleasable();
    void propertiesChangedUpdatesFacts();
    void ownerLossFailsClosedUnavailable();
    void ownerReplacementInvalidatesEpoch();
    void absentServiceStaysUnavailable();
    void hostileFiveProfilesDegradeThroughSanitization();
    void hostileDuplicateHoldsDegradeThroughSanitization();
    void hostileWrongTypedActiveProfileFailsClosedMalformed();
};

void PowerProfilesAdapterTests::publishesProfilesHoldsAndActiveProfile()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().profiles.supported.size(), 3);

    const Snapshot snapshot = row.coordinator->snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QVERIFY(snapshot.capabilities.testFlag(Capability::Profiles));
    QVERIFY(snapshot.capabilities.testFlag(Capability::ProfileHolds));
    QCOMPARE(snapshot.profiles.supported.at(0).id, QStringLiteral("power-saver"));
    QCOMPARE(snapshot.profiles.supported.at(1).label, QStringLiteral("Balanced"));
    QCOMPARE(snapshot.profiles.activeProfileId, QStringLiteral("balanced"));
    QCOMPARE(snapshot.profiles.holds.size(), 0);
}

void PowerProfilesAdapterTests::legacyNameAndInterfaceStillServeFacts()
{
    ProfilesRow row;
    QVERIFY(row.start(true));
    row.fake->setHolds({{QStringLiteral("performance"),
                         QStringLiteral("Foreign"),
                         QStringLiteral("thermal")}});
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().profiles.supported.size(), 3);
    QCOMPARE(row.coordinator->snapshot().profiles.holds.size(), 1);
    QCOMPARE(row.coordinator->snapshot().profiles.holds.constFirst().applicationName,
             QStringLiteral("Foreign"));
}

void PowerProfilesAdapterTests::setProfileUsesStandardPropertySet()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    PowerServiceRequest request;
    request.kind = OperationKind::SetProfile;
    request.profileId = QStringLiteral("performance");
    const OperationSubmission submission = row.coordinator->submit(request);
    QVERIFY(submission.pending);
    QTRY_COMPARE(row.fake->setProfileRequests.size(), 1);
    QCOMPARE(row.fake->setProfileRequests.constFirst(),
             QStringLiteral("performance"));

    QSignalSpy completed(row.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    row.fake->setActiveProfile(QStringLiteral("performance"));
    row.fake->emitPropertiesChanged();
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
    QTRY_COMPARE(row.coordinator->snapshot().profiles.activeProfileId,
                 QStringLiteral("performance"));
}

void PowerProfilesAdapterTests::setProfileErrorCompletesFailed()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.fake->setRejectSetProfile(true);
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    QSignalSpy completed(row.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    PowerServiceRequest request;
    request.kind = OperationKind::SetProfile;
    request.profileId = QStringLiteral("balanced");
    const OperationSubmission submission = row.coordinator->submit(request);
    QVERIFY(submission.pending);
    QTRY_COMPARE(completed.size(), 1);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Failed);
    QCOMPARE(result.reasonCode, QStringLiteral("profiles-rejected"));
}

void PowerProfilesAdapterTests::acquireHoldRecordsUpstreamAndIsReleasable()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    QSignalSpy completed(row.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    PowerServiceRequest request;
    request.kind = OperationKind::AcquireProfileHold;
    request.profileId = QStringLiteral("performance");
    request.applicationName = QStringLiteral("QindaQt");
    request.reason = QStringLiteral("test hold");
    const OperationSubmission submission = row.coordinator->submit(request);
    QVERIFY(submission.pending);
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
    QTRY_COMPARE(row.fake->holdRequests.size(), 1);
    QCOMPARE(row.fake->holdRequests.constFirst().reason, QStringLiteral("test hold"));
    QCOMPARE(row.fake->holdRequests.constFirst().application,
             QStringLiteral("QindaQt"));
    QCOMPARE(row.fake->holdRequests.constFirst().appId, QStringLiteral("QindaQt"));

    // The hold becomes visible in facts and is releasable through its handle.
    row.fake->emitPropertiesChanged();
    QTRY_COMPARE(row.coordinator->snapshot().profiles.holds.size(), 1);
    const Handle hold = row.coordinator->snapshot().profiles.holds.constFirst().handle;
    QCOMPARE(hold.opaqueId.size(), 32);

    QSignalSpy released(row.coordinator.get(),
                        &PowerServiceCoordinator::operationCompleted);
    PowerServiceRequest releaseRequest;
    releaseRequest.kind = OperationKind::ReleaseProfileHold;
    releaseRequest.handle = hold;
    const OperationSubmission release = row.coordinator->submit(releaseRequest);
    QVERIFY(release.pending);
    QTRY_COMPARE(released.size(), 1);
    QCOMPARE(released.first().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
    QTRY_COMPARE(row.fake->releaseRequests.size(), 1);
}

void PowerProfilesAdapterTests::foreignHoldIsNotReleasable()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.fake->setHolds({{QStringLiteral("performance"),
                         QStringLiteral("ForeignApp"),
                         QStringLiteral("foreign reason")}});
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().profiles.holds.size(), 1);
    const Handle foreign =
        row.coordinator->snapshot().profiles.holds.constFirst().handle;

    QSignalSpy completed(row.coordinator.get(),
                         &PowerServiceCoordinator::operationCompleted);
    PowerServiceRequest releaseRequest;
    releaseRequest.kind = OperationKind::ReleaseProfileHold;
    releaseRequest.handle = foreign;
    const OperationSubmission release = row.coordinator->submit(releaseRequest);
    QVERIFY(release.pending);
    QTRY_COMPARE(completed.size(), 1);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    // Upstream exposes no object path for holds this process did not acquire,
    // so releasing foreign holds fails closed without any upstream call.
    QCOMPARE(result.status, OperationStatus::Unsupported);
    QCOMPARE(result.reasonCode, QStringLiteral("hold-not-releasable"));
    QCOMPARE(row.fake->releaseRequests.size(), 0);
}

void PowerProfilesAdapterTests::propertiesChangedUpdatesFacts()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().profiles.activeProfileId,
                 QStringLiteral("balanced"));

    row.fake->setProfiles({QStringLiteral("power-saver"), QStringLiteral("balanced")});
    row.fake->setActiveProfile(QStringLiteral("power-saver"));
    row.fake->emitPropertiesChanged();
    QTRY_COMPARE(row.coordinator->snapshot().profiles.supported.size(), 2);
    QTRY_COMPARE(row.coordinator->snapshot().profiles.activeProfileId,
                 QStringLiteral("power-saver"));
}

void PowerProfilesAdapterTests::ownerLossFailsClosedUnavailable()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("profiles-unavailable"));
    QCOMPARE(row.coordinator->snapshot().profiles.supported.size(), 0);
}

void PowerProfilesAdapterTests::ownerReplacementInvalidatesEpoch()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Ready);
    const quint64 firstEpoch = row.coordinator->snapshot().epoch;

    row.fake->unregisterService();
    QDBusConnection::disconnectFromBus(row.fakeConnection.name());
    row.fakeConnection = row.bus->openConnection(QStringLiteral("replacement"));
    row.fake = std::make_unique<FakePpdService>(row.fakeConnection, false);
    QVERIFY(row.fake->registerService());
    row.fake->setProfiles({QStringLiteral("balanced")});
    row.fake->setActiveProfile(QStringLiteral("balanced"));
    QTRY_VERIFY(row.coordinator->snapshot().epoch > firstEpoch);
    QTRY_COMPARE(row.coordinator->snapshot().profiles.supported.size(), 1);
}

void PowerProfilesAdapterTests::absentServiceStaysUnavailable()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeBatteryCollaborator battery;
    FakeSessionCollaborator session;
    Upstream::PowerProfilesCollaborator adapter(bus.connection);
    PowerServiceCoordinator coordinator(&battery, &adapter, &session);
    coordinator.start();
    battery.publish(fixtureBatteryFacts());
    session.publish(fixtureSessionFacts());
    QTRY_COMPARE(coordinator.snapshot().availability, Availability::Degraded);
    QCOMPARE(coordinator.snapshot().reasonCode,
             QStringLiteral("profiles-unavailable"));
    QCOMPARE(coordinator.snapshot().profiles.supported.size(), 0);
}

void PowerProfilesAdapterTests::hostileFiveProfilesDegradeThroughSanitization()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.fake->setProfiles({QStringLiteral("a"), QStringLiteral("b"),
                           QStringLiteral("c"), QStringLiteral("d"),
                           QStringLiteral("e")});
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("profile-malformed"));
}

void PowerProfilesAdapterTests::hostileDuplicateHoldsDegradeThroughSanitization()
{
    ProfilesRow row;
    QVERIFY(row.start());
    row.fake->setHolds({{QStringLiteral("performance"), QStringLiteral("Same"),
                         QStringLiteral("same reason")},
                        {QStringLiteral("performance"), QStringLiteral("Same"),
                         QStringLiteral("same reason")}});
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("profile-malformed"));
}

void PowerProfilesAdapterTests::hostileWrongTypedActiveProfileFailsClosedMalformed()
{
    // The wrong-typed ActiveProfile is delivered through a minimal virtual
    // object sibling rather than the typed fake, which cannot represent it.
    ProfilesRow row;
    QVERIFY(row.start());
    row.fake->unregisterService();

    class HostilePpd final : public QDBusVirtualObject
    {
    public:
        using QDBusVirtualObject::QDBusVirtualObject;
        QString introspect(const QString &) const override
        {
            return QStringLiteral("<node/>");
        }
        bool handleMessage(const QDBusMessage &message,
                           const QDBusConnection &connection) override
        {
            if (message.member() == QStringLiteral("GetAll")) {
                QDBusMessage reply = message.createReply();
                QVariantMap properties;
                properties.insert(QStringLiteral("ActiveProfile"), QVariant(uint(7)));
                reply.setArguments({QVariant(properties)});
                connection.send(reply);
                return true;
            }
            connection.send(message.createErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                QStringLiteral("hostile")));
            return true;
        }
    };

    QDBusConnection hostileConnection = row.bus->openConnection(QStringLiteral("hostile"));
    HostilePpd hostile;
    hostileConnection.registerService(QStringLiteral("org.freedesktop.UPower.PowerProfiles"));
    hostileConnection.registerVirtualObject(QStringLiteral("/net/hadess/PowerProfiles"),
                                            &hostile, QDBusConnection::SubPath);
    row.battery.publish(fixtureBatteryFacts());
    row.session.publish(fixtureSessionFacts());
    QTRY_COMPARE(row.coordinator->snapshot().availability, Availability::Degraded);
    QCOMPARE(row.coordinator->snapshot().reasonCode,
             QStringLiteral("profiles-malformed"));
    QDBusConnection::disconnectFromBus(hostileConnection.name());
}

QTEST_GUILESS_MAIN(PowerProfilesAdapterTests)
#include "tst_power_profiles_adapter.moc"
