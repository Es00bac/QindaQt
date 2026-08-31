// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/resident_display_service.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_service_test_support.h"
#include "support/private_bus_test_support.h"

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

enum RejectionKind {
    RegressedGeneration,
    ChangedEqualGeneration,
    UnchangedNewGeneration,
    InvalidNewProjection,
};

enum LiveState {
    Staged,
    Applying,
    Observing,
    AwaitingConfirmation,
};

QString rejectionName(const RejectionKind kind)
{
    switch (kind) {
    case RegressedGeneration: return QStringLiteral("regressed");
    case ChangedEqualGeneration: return QStringLiteral("changed-equal");
    case UnchangedNewGeneration: return QStringLiteral("unchanged-new");
    case InvalidNewProjection: return QStringLiteral("invalid-projection");
    }
    Q_UNREACHABLE_RETURN({});
}

QString stateName(const LiveState state)
{
    switch (state) {
    case Staged: return QStringLiteral("staged");
    case Applying: return QStringLiteral("applying");
    case Observing: return QStringLiteral("observing");
    case AwaitingConfirmation: return QStringLiteral("awaiting-confirmation");
    }
    Q_UNREACHABLE_RETURN({});
}

DisplayTransaction::MachineState machineState(const LiveState state)
{
    switch (state) {
    case Staged: return DisplayTransaction::MachineState::Staged;
    case Applying: return DisplayTransaction::MachineState::Applying;
    case Observing: return DisplayTransaction::MachineState::Observing;
    case AwaitingConfirmation:
        return DisplayTransaction::MachineState::AwaitingConfirmation;
    }
    Q_UNREACHABLE_RETURN(DisplayTransaction::MachineState::Discovering);
}

} // namespace

class ResidentDisplayServiceInventoryRejectionPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesLiveTruthAndTransaction_data();
    void preservesLiveTruthAndTransaction();
    void failedOwnerReplacementWithdrawsTruth();
};

void ResidentDisplayServiceInventoryRejectionPrivateBusTest::
    preservesLiveTruthAndTransaction_data()
{
    QTest::addColumn<int>("rejectionValue");
    QTest::addColumn<int>("liveStateValue");

    for (const RejectionKind rejection : {RegressedGeneration,
                                          ChangedEqualGeneration,
                                          UnchangedNewGeneration,
                                          InvalidNewProjection}) {
        for (const LiveState state : {Staged, Applying, Observing,
                                      AwaitingConfirmation}) {
            const QByteArray rowName = QStringLiteral("%1-%2")
                .arg(rejectionName(rejection), stateName(state))
                .toLatin1();
            QTest::newRow(rowName.constData())
                << static_cast<int>(rejection) << static_cast<int>(state);
        }
    }
}

void ResidentDisplayServiceInventoryRejectionPrivateBusTest::
    preservesLiveTruthAndTransaction()
{
    QFETCH(int, rejectionValue);
    QFETCH(int, liveStateValue);
    const auto rejection = static_cast<RejectionKind>(rejectionValue);
    const auto liveState = static_cast<LiveState>(liveStateValue);

    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));
    const QString connectionName =
        privateConnectionName(QStringLiteral("resident-rejection"));
    QDBusConnection connection =
        QDBusConnection::connectToBus(bus.address(), connectionName);
    QVERIFY(connection.isConnected());

    auto inventory = std::make_unique<FakeInventorySource>();
    FakeInventorySource *inventoryPointer = inventory.get();
    auto port = std::make_unique<FakeTransactionPort>();
    FakeTransactionPort *portPointer = port.get();
    const DisplayTransaction::Timing timing{
        .applyTimeoutMilliseconds = 60'000,
        .observationTimeoutMilliseconds = 60'000,
        .confirmationTimeoutMilliseconds = 60'000,
        .firstRevertBackoffMilliseconds = 60'000,
        .secondRevertBackoffMilliseconds = 60'000};
    ResidentDisplayService service(
        std::move(inventory), std::move(port), std::make_unique<FakeClock>(),
        [] { return QStringLiteral("rejection-restart-seed"); }, connection,
        QString::fromLatin1(Display::kServiceName), timing);
    QCOMPARE(service.start(), ServiceStartStatus::Started);

    InventoryFrame accepted = frame(5, {output()});
    inventoryPointer->publish(accepted);
    QVERIFY(service.model()->available());
    QVERIFY(service.setSafetyState(DisplayTransaction::SafetyState::Safe).accepted);
    Display::Candidate target =
        DisplayTopology::candidateFromSnapshot(*service.model()->snapshot());
    target.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(service.model()->stage(QStringLiteral("retained-tx"), target)
                .command.accepted);

    if (liveState >= Applying) {
        QVERIFY(service.model()->preview(QStringLiteral("retained-tx"))
                    .command.accepted);
    }
    if (liveState >= Observing) {
        portPointer->completeLast(DisplayTransaction::ApplyOutcome::Applied);
    }
    if (liveState == AwaitingConfirmation) {
        accepted.outputGeneration = 6;
        accepted.outputs[0].transform = Display::Transform::Rotate180;
        inventoryPointer->publish(accepted);
    }
    QCOMPARE(service.model()->view()->state, machineState(liveState));

    InventoryFrame rejected = accepted;
    switch (rejection) {
    case RegressedGeneration:
        --rejected.outputGeneration;
        break;
    case ChangedEqualGeneration:
        rejected.outputs[0].model = QStringLiteral("same-generation collision");
        break;
    case UnchangedNewGeneration:
        ++rejected.outputGeneration;
        break;
    case InvalidNewProjection:
        ++rejected.outputGeneration;
        rejected.outputs[0].scale = 4.0;
        break;
    }

    const Display::Snapshot snapshotBefore = *service.model()->snapshot();
    const DisplayTransaction::MachineView viewBefore = *service.model()->view();
    const QString ownerBefore = service.model()->sourceOwner();
    const quint64 generationBefore = service.model()->sourceGeneration();
    const quint64 lineageBefore = service.model()->machineLineage();
    const qsizetype appliesBefore = portPointer->applyRequests.size();
    const qsizetype storesBefore = portPointer->storedJournals.size();
    const int clearsBefore = portPointer->clearCalls;
    QSignalSpy stateChanges(&service, &ResidentDisplayService::modelStateChanged);

    inventoryPointer->publish(rejected);

    QVERIFY(service.model()->available());
    QVERIFY(*service.model()->snapshot() == snapshotBefore);
    QVERIFY(*service.model()->view() == viewBefore);
    QCOMPARE(service.model()->sourceOwner(), ownerBefore);
    QCOMPARE(service.model()->sourceGeneration(), generationBefore);
    QCOMPARE(service.model()->machineLineage(), lineageBefore);
    QCOMPARE(portPointer->applyRequests.size(), appliesBefore);
    QCOMPARE(portPointer->storedJournals.size(), storesBefore);
    QCOMPARE(portPointer->clearCalls, clearsBefore);
    QCOMPARE(stateChanges.count(), 0);

    // The contrast is intentional: a real source-unavailable edge withdraws
    // the public machine and retains any active journal for later recovery.
    inventoryPointer->loseTransport();
    QVERIFY(!service.model()->available());
    QVERIFY(service.model()->snapshot() == nullptr);
    QCOMPARE(stateChanges.count(), 1);

    service.stop();
    QDBusConnection::disconnectFromBus(connectionName);
}

void ResidentDisplayServiceInventoryRejectionPrivateBusTest::
    failedOwnerReplacementWithdrawsTruth()
{
    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));
    const QString connectionName =
        privateConnectionName(QStringLiteral("resident-owner-replacement"));
    QDBusConnection connection =
        QDBusConnection::connectToBus(bus.address(), connectionName);
    QVERIFY(connection.isConnected());

    auto inventory = std::make_unique<FakeInventorySource>();
    FakeInventorySource *inventoryPointer = inventory.get();
    auto port = std::make_unique<FakeTransactionPort>();
    ResidentDisplayService service(
        std::move(inventory), std::move(port), std::make_unique<FakeClock>(),
        [] { return QStringLiteral("owner-replacement-seed"); }, connection,
        QString::fromLatin1(Display::kServiceName));
    QCOMPARE(service.start(), ServiceStartStatus::Started);
    inventoryPointer->publish(frame(5, {output()}));
    QVERIFY(service.model()->available());
    const quint64 originalLineage = service.model()->machineLineage();
    QSignalSpy stateChanges(&service, &ResidentDisplayService::modelStateChanged);

    InventoryOutput invalidReplacement = output();
    invalidReplacement.scale = 4.0;
    inventoryPointer->publish(frame(1, {invalidReplacement},
                                    QStringLiteral(":1.99")));

    QVERIFY(!service.model()->available());
    QVERIFY(service.model()->snapshot() == nullptr);
    QCOMPARE(service.model()->machineLineage(), originalLineage);
    QCOMPARE(stateChanges.count(), 1);

    service.stop();
    QDBusConnection::disconnectFromBus(connectionName);
}

QTEST_MAIN(ResidentDisplayServiceInventoryRejectionPrivateBusTest)

#include "tst_resident_display_service_inventory_rejection_private_bus.moc"
