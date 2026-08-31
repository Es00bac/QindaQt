// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/display_service_model.h>

#include <qindaqt/services/display_topology/topology.h>

#include "support/display_service_test_support.h"

#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

DisplayTransaction::Journal recoveryJournal(const InventoryFrame &initial,
                                            const Display::Transform targetTransform)
{
    const InventoryProjectionResult projected =
        projectInventory(initial, QStringLiteral("pre-crash-epoch"));
    Q_ASSERT(projected.accepted());
    Display::Candidate target =
        DisplayTopology::candidateFromSnapshot(projected.snapshot);
    target.outputs[0].transform = targetTransform;
    return {.schemaVersion = DisplayTransaction::kJournalSchemaVersion,
            .transactionId = QStringLiteral("recover-me"),
            .phase = DisplayTransaction::JournalPhase::AwaitingConfirmation,
            .reason = Display::TransactionReason::None,
            .preimage = DisplayTopology::candidateFromSnapshot(projected.snapshot),
            .target = std::move(target),
            .revertAttempt = 0};
}

} // namespace

class DisplayServiceRecoveryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void routesStartupJournalThroughRecoveryBeforeReady();
    void rejectsInvalidStartupJournalWithoutApplyOrClear();
};

void DisplayServiceRecoveryTest::routesStartupJournalThroughRecoveryBeforeReady()
{
    FakeClock clock;
    FakeTransactionPort port;
    const InventoryFrame prior = frame(1, {output()});
    const DisplayTransaction::Journal journal =
        recoveryJournal(prior, Display::Transform::Rotate180);
    DisplayServiceModel model(clock, port,
                              [] { return QStringLiteral("restart-seed"); }, {},
                              journal);

    InventoryFrame liveTarget = prior;
    liveTarget.outputs[0].transform = Display::Transform::Rotate180;
    const InventoryObservationResult recovered = model.observeInventory(liveTarget);
    QCOMPARE(recovered.status, InventoryObservationStatus::AcceptedNewLineage);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::RevertingApply);
    QCOMPARE(model.view()->reason, Display::TransactionReason::Recovery);
    QVERIFY(model.view()->journalActive);
    QCOMPARE(port.applyRequests.size(), 1);
    QCOMPARE(port.applyRequests.constFirst().scope,
             DisplayTransaction::ApplyScope::FullPreimage);
    QCOMPARE(port.requestMachineLineages.constFirst(), model.machineLineage());
    QVERIFY(port.requestMachineLineages.constFirst() > 0);

    // Recovery requests are never forward candidates, even if safety later
    // becomes Safe while the retained journal is active.
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);
    QCOMPARE(port.applyRequests.size(), 1);
    QCOMPARE(model.stage(QStringLiteral("replacement"), journal.target).command.error,
             DisplayTransaction::CommandError::TransactionActive);
}

void DisplayServiceRecoveryTest::rejectsInvalidStartupJournalWithoutApplyOrClear()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayTransaction::Journal invalid =
        recoveryJournal(frame(1, {output()}), Display::Transform::Rotate180);
    invalid.schemaVersion = 999;
    DisplayServiceModel model(clock, port,
                              [] { return QStringLiteral("restart-seed"); }, {},
                              invalid);

    const InventoryObservationResult rejected =
        model.observeInventory(frame(1, {output()}));
    QCOMPARE(rejected.status, InventoryObservationStatus::Rejected);
    QCOMPARE(rejected.reasonCode, QStringLiteral("transaction-recovery-failed"));
    QVERIFY(!model.available());
    QVERIFY(port.applyRequests.isEmpty());
    QCOMPARE(port.clearCalls, 0);
    QCOMPARE(model.machineLineage(), quint64(1));
}

QTEST_GUILESS_MAIN(DisplayServiceRecoveryTest)

#include "tst_display_service_recovery.moc"
