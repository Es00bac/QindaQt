// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/display_runtime_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayRuntime;
using namespace QindaQt::DisplayRuntime::TestSupport;
using namespace QindaQt::DisplayService::TestSupport;

class ResidentDisplayRuntimeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsInvalidJournalBeforeStartingMutationAuthorities();
    void rejectsFailedStartupAuthoritiesBeforeResident();
    void recoversLoadedTargetWithOneRollbackAndNoForwardReplay();
    void requiresWriterDelayAndUnlockedAuthority();
    void durabilityUncertaintyNeverForwards();
    void suspendAndOwnerReplacementFenceLateCompletion();
    void authorityLossIsTerminalAndRevokesMutation();
};

void ResidentDisplayRuntimeTest::
    rejectsInvalidJournalBeforeStartingMutationAuthorities()
{
    DisplayTransaction::Journal invalid =
        recoveryJournal(frame(1, {output()}));
    invalid.transactionId.clear();
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(invalid), qPrintable(fixture.failure));
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::InvalidStartupJournal);
    QCOMPARE(fixture.output->startCalls, 0);
    QCOMPARE(fixture.safety->startCalls, 0);
    QVERIFY(fixture.output->submissions.isEmpty());
    QCOMPARE(fixture.store->clearCalls, 0);
    QVERIFY(fixture.store->journals.isEmpty());
}

void ResidentDisplayRuntimeTest::rejectsFailedStartupAuthoritiesBeforeResident()
{
    RuntimeFixture writerFailure;
    QVERIFY2(writerFailure.initialize(), qPrintable(writerFailure.failure));
    writerFailure.output->startStatus =
        DisplayWriter::PortStartStatus::ConnectionUnavailable;
    QCOMPARE(writerFailure.runtime->start(), RuntimeStartStatus::WriterStartFailed);
    QCOMPARE(writerFailure.safety->startCalls, 0);
    QVERIFY(!writerFailure.inventory->started);

    RuntimeFixture identityFailure;
    QVERIFY2(identityFailure.initialize(), qPrintable(identityFailure.failure));
    identityFailure.output->configuredPeerProcessId = 0;
    QCOMPARE(identityFailure.runtime->start(),
             RuntimeStartStatus::InvalidCompositorIdentity);
    QCOMPARE(identityFailure.output->stopCalls, 1);
    QCOMPARE(identityFailure.safety->startCalls, 0);
    QVERIFY(!identityFailure.inventory->started);

    RuntimeFixture safetyFailure;
    QVERIFY2(safetyFailure.initialize(), qPrintable(safetyFailure.failure));
    safetyFailure.safety->startStatus =
        SessionSafetyStartStatus::LogindRegistrationFailed;
    QCOMPARE(safetyFailure.runtime->start(),
             RuntimeStartStatus::SessionSafetyStartFailed);
    QCOMPARE(safetyFailure.output->stopCalls, 1);
    QVERIFY(!safetyFailure.inventory->started);

    RuntimeFixture residentFailure;
    QVERIFY2(residentFailure.initialize(), qPrintable(residentFailure.failure));
    residentFailure.inventory->startStatus =
        DisplayService::InventorySourceStartStatus::InvalidConnection;
    QSignalSpy fatalSpy(residentFailure.runtime.get(),
                        &ResidentDisplayRuntime::fatalError);
    QCOMPARE(residentFailure.runtime->start(),
             RuntimeStartStatus::TerminallyFailed);
    QCOMPARE(fatalSpy.size(), 1);
    QCOMPARE(residentFailure.output->stopCalls, 1);
    QCOMPARE(residentFailure.safety->stopCalls, 1);
    QVERIFY(!residentFailure.inventory->started);
}

void ResidentDisplayRuntimeTest::
    recoversLoadedTargetWithOneRollbackAndNoForwardReplay()
{
    const DisplayService::InventoryFrame prior = frame(1, {output()});
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(recoveryJournal(prior)),
             qPrintable(fixture.failure));
    QSignalSpy readySpy(fixture.runtime.get(), &ResidentDisplayRuntime::ready);
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::Started);
    QCOMPARE(readySpy.size(), 1);
    QCOMPARE(fixture.safety->expectedProcessId,
             QCoreApplication::applicationPid());
    fixture.output->publishOwner(1, true);

    DisplayService::InventoryFrame liveTarget = prior;
    liveTarget.outputs[0].transform = Display::Transform::Rotate180;
    fixture.inventory->publish(liveTarget);
    QCOMPARE(fixture.output->submissions.size(), 1);
    const DisplayWriter::Configuration rollback =
        fixture.output->submissions.constFirst();
    QCOMPARE(rollback.scope, DisplayWriter::ConfigurationScope::CompleteTopology);
    QCOMPARE(rollback.outputs.size(), 1);
    QCOMPARE(rollback.outputs.constFirst().transform,
             Display::Transform::Normal);
    QCOMPARE(fixture.runtime->resident()->model()->view()->state,
             DisplayTransaction::MachineState::RevertingApply);
    QCOMPARE(fixture.store->clearCalls, 0);
}

void ResidentDisplayRuntimeTest::requiresWriterDelayAndUnlockedAuthority()
{
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.failure));
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::Started);
    fixture.inventory->publish(frame(1, {output()}));

    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(
        *fixture.runtime->resident()->model()->snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->stage(QStringLiteral("authority"), candidate)
                .command.accepted);
    QCOMPARE(fixture.runtime->resident()
                 ->model()
                 ->preview(QStringLiteral("authority"))
                 .command.error,
             DisplayTransaction::CommandError::Locked);
    QVERIFY(fixture.output->submissions.isEmpty());

    fixture.output->publishOwner(1, true);
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->preview(QStringLiteral("authority"))
                .command.accepted);
    QCOMPARE(fixture.output->submissions.size(), 1);

    fixture.output->complete(
        1, fixture.output->submissions.constFirst().requestId,
        DisplayWriter::CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    fixture.inventory->publish(frame(
        2, {output(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0,
                   Display::Transform::Rotate180)}));
    QCOMPARE(fixture.runtime->resident()->model()->view()->state,
             DisplayTransaction::MachineState::AwaitingConfirmation);
    fixture.safety->publishSafety(DisplayTransaction::SafetyState::Locked);
    QCOMPARE(fixture.output->submissions.size(), 2);
    QCOMPARE(fixture.output->submissions.constLast()
                 .outputs.constFirst()
                 .transform,
             Display::Transform::Normal);
}

void ResidentDisplayRuntimeTest::durabilityUncertaintyNeverForwards()
{
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.failure));
    fixture.store->storeOutcome =
        DisplayTransaction::JournalMutationOutcome::DurabilityUncertain;
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::Started);
    fixture.output->publishOwner(1, true);
    fixture.inventory->publish(frame(1, {output()}));

    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(
        *fixture.runtime->resident()->model()->snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->stage(QStringLiteral("uncertain"), candidate)
                .command.accepted);
    const DisplayService::ServiceOperationResult preview =
        fixture.runtime->resident()->model()->preview(
            QStringLiteral("uncertain"));
    QVERIFY(preview.command.accepted);
    QCOMPARE(preview.command.error,
             DisplayTransaction::CommandError::JournalFailure);
    QCOMPARE(fixture.runtime->resident()->model()->view()->state,
             DisplayTransaction::MachineState::Stuck);
    QVERIFY(fixture.runtime->resident()->model()->view()->journalActive);
    QVERIFY(fixture.output->submissions.isEmpty());
    QCOMPARE(fixture.store->clearCalls, 0);
}

void ResidentDisplayRuntimeTest::
    suspendAndOwnerReplacementFenceLateCompletion()
{
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.failure));
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::Started);
    fixture.output->publishOwner(1, true);
    fixture.inventory->publish(frame(1, {output()}));
    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(
        *fixture.runtime->resident()->model()->snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->stage(QStringLiteral("suspend"), candidate)
                .command.accepted);
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->preview(QStringLiteral("suspend"))
                .command.accepted);
    QCOMPARE(fixture.output->submissions.size(), 1);
    const quint64 oldRequest = fixture.output->submissions.constFirst().requestId;

    fixture.safety->prepareForSleep();
    QCOMPARE(fixture.safety->releaseCalls, 0);
    QVERIFY(fixture.safety->delayHeld());
    fixture.output->complete(1, oldRequest,
                             DisplayWriter::CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.safety->releaseCalls, 1);
    QVERIFY(!fixture.safety->delayHeld());

    fixture.safety->resumeWithDelay();
    QCOMPARE(fixture.runtime->resident()->model()->view()->safety,
             DisplayTransaction::SafetyState::Safe);

    fixture.inventory->loseTransport();
    DisplayService::InventoryFrame replacement = frame(
        1,
        {output(QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0,
                Display::Transform::Rotate180)},
        QStringLiteral(":1.88"));
    fixture.inventory->publish(replacement);
    QCOMPARE(fixture.output->submissions.size(), 2);
    QCOMPARE(fixture.output->submissions.constLast()
                 .outputs.constFirst()
                 .transform,
             Display::Transform::Normal);

    fixture.output->complete(1, oldRequest,
                             DisplayWriter::CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.output->submissions.size(), 2);
}

void ResidentDisplayRuntimeTest::authorityLossIsTerminalAndRevokesMutation()
{
    RuntimeFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.failure));
    QSignalSpy fatalSpy(fixture.runtime.get(),
                        &ResidentDisplayRuntime::fatalError);
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::Started);
    fixture.output->publishOwner(1, true);
    fixture.inventory->publish(frame(1, {output()}));
    fixture.safety->loseAuthority(QStringLiteral("logind-owner-replaced"));
    QCOMPARE(fatalSpy.size(), 1);
    QCOMPARE(fatalSpy.constFirst().constFirst().toString(),
             QStringLiteral("logind-owner-replaced"));
    QVERIFY(fixture.runtime->hasFailed());
    QVERIFY(!fixture.runtime->isReady());

    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(
        *fixture.runtime->resident()->model()->snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(fixture.runtime->resident()
                ->model()
                ->stage(QStringLiteral("after-loss"), candidate)
                .command.accepted);
    QCOMPARE(fixture.runtime->resident()
                 ->model()
                 ->preview(QStringLiteral("after-loss"))
                 .command.error,
             DisplayTransaction::CommandError::Locked);
    QVERIFY(fixture.output->submissions.isEmpty());
    QCOMPARE(fixture.runtime->start(), RuntimeStartStatus::TerminallyFailed);
    QCOMPARE(fixture.output->startCalls, 1);
}

QTEST_GUILESS_MAIN(ResidentDisplayRuntimeTest)

#include "tst_resident_display_runtime.moc"
