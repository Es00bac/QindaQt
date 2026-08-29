// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/display_writer_test_support.h"

#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayWriter;
using namespace QindaQt::DisplayWriter::TestSupport;

class DisplayWriterPortTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesExactlyOnceAndFencesLateReplies();
    void ownerReplacementAndTimeoutAreUncertain();
    void malformedAndConcurrentRequestsFailClosed();
    void preservesJournalBoundaryAndStopCompletion();
    void fencesHostileSynchronousAndLineageCompletions();
    void mapsTransportSubmissionFailures();
    void rebindsAfterImmediateStopAndRestart();
};

void DisplayWriterPortTests::appliesExactlyOnceAndFencesLateReplies()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    auto journal = std::make_unique<FakeJournalStore>();
    WriterTransactionPort port(std::move(output), std::move(journal), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(4, true);
    port.beginMachineLineage(17);

    port.requestApply(completeRequest(44));
    QCOMPARE(observer.completions.size(), 0);
    QCOMPARE(outputPointer->submissions.size(), 1);
    const quint64 requestId = outputPointer->submissions.constLast().requestId;
    outputPointer->complete(4, requestId, CompletionOutcome::Applied);
    QCOMPARE(observer.completions.size(), 0);
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0],
             (Completion{17, 44, DisplayTransaction::ApplyOutcome::Applied}));

    outputPointer->complete(4, requestId, CompletionOutcome::Rejected);
    QCoreApplication::processEvents();
    QCOMPARE(observer.completions.size(), 1);
}

void DisplayWriterPortTests::ownerReplacementAndTimeoutAreUncertain()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    WriterTransactionPort port(std::move(output),
                               std::make_unique<FakeJournalStore>(), 1);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(8, true);
    port.beginMachineLineage(21);

    port.requestApply(completeRequest(50));
    const quint64 oldRequest = outputPointer->submissions.constLast().requestId;
    outputPointer->publishOwner(9, false);
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0].outcome,
             DisplayTransaction::ApplyOutcome::TransportUncertain);
    outputPointer->complete(8, oldRequest, CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    QCOMPARE(observer.completions.size(), 1);

    outputPointer->publishOwner(10, true);
    port.requestApply(completeRequest(51));
    QTRY_COMPARE(observer.completions.size(), 2);
    QCOMPARE(observer.completions[1].token, 51);
    QCOMPARE(observer.completions[1].outcome,
             DisplayTransaction::ApplyOutcome::TransportUncertain);
}

void DisplayWriterPortTests::malformedAndConcurrentRequestsFailClosed()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    WriterTransactionPort port(std::move(output),
                               std::make_unique<FakeJournalStore>(), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(3, true);
    port.beginMachineLineage(5);

    auto malformed = completeRequest(60);
    malformed.candidate.outputs[0].stableId = QStringLiteral("edid:unsupported");
    port.requestApply(malformed);
    QCOMPARE(observer.completions.size(), 0);
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0].outcome,
             DisplayTransaction::ApplyOutcome::Rejected);

    port.requestApply(completeRequest(61));
    port.requestApply(completeRequest(62));
    QTRY_COMPARE(observer.completions.size(), 2);
    QCOMPARE(observer.completions[1].token, 62);
    QCOMPARE(observer.completions[1].outcome,
             DisplayTransaction::ApplyOutcome::Rejected);
    const quint64 acceptedId = outputPointer->submissions.constLast().requestId;
    outputPointer->complete(3, acceptedId, CompletionOutcome::Rejected);
    QTRY_COMPARE(observer.completions.size(), 3);
    QCOMPARE(observer.completions[2].token, 61);
}

void DisplayWriterPortTests::preservesJournalBoundaryAndStopCompletion()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    auto journal = std::make_unique<FakeJournalStore>();
    auto *journalPointer = journal.get();
    WriterTransactionPort port(std::move(output), std::move(journal), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(1, true);
    port.beginMachineLineage(2);

    DisplayTransaction::Journal value;
    QCOMPARE(port.storeJournal(value),
             DisplayTransaction::JournalMutationOutcome::Durable);
    QCOMPARE(port.clearJournal(),
             DisplayTransaction::JournalMutationOutcome::Durable);
    journalPointer->storeOutcome =
        DisplayTransaction::JournalMutationOutcome::DurabilityUncertain;
    journalPointer->clearOutcome =
        DisplayTransaction::JournalMutationOutcome::DurabilityUncertain;
    QCOMPARE(port.storeJournal(value),
             DisplayTransaction::JournalMutationOutcome::DurabilityUncertain);
    QCOMPARE(port.clearJournal(),
             DisplayTransaction::JournalMutationOutcome::DurabilityUncertain);
    QCOMPARE(journalPointer->journals.size(), 2);
    QCOMPARE(journalPointer->clearCalls, 2);

    port.requestApply(completeRequest(70));
    port.stop();
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0],
             (Completion{2, 70,
                         DisplayTransaction::ApplyOutcome::TransportUncertain}));
    QVERIFY(!port.isStarted());
    QVERIFY(!port.isOutputManagementAvailable());
}

void DisplayWriterPortTests::fencesHostileSynchronousAndLineageCompletions()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    outputPointer->synchronousOutcome = CompletionOutcome::Applied;
    WriterTransactionPort port(std::move(output),
                               std::make_unique<FakeJournalStore>(), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(12, true);
    port.beginMachineLineage(30);

    port.requestApply(completeRequest(80));
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0],
             (Completion{30, 80, DisplayTransaction::ApplyOutcome::Applied}));

    outputPointer->synchronousOutcome.reset();
    port.requestApply(completeRequest(81));
    const quint64 pendingId = outputPointer->submissions.constLast().requestId;
    port.beginMachineLineage(31);
    QTRY_COMPARE(observer.completions.size(), 2);
    QCOMPARE(observer.completions[1],
             (Completion{30, 81,
                         DisplayTransaction::ApplyOutcome::TransportUncertain}));
    outputPointer->complete(12, pendingId, CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    QCOMPARE(observer.completions.size(), 2);
}

void DisplayWriterPortTests::mapsTransportSubmissionFailures()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    WriterTransactionPort port(std::move(output),
                               std::make_unique<FakeJournalStore>(), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(14, true);
    port.beginMachineLineage(40);

    outputPointer->submitStatus = SubmitStatus::Malformed;
    port.requestApply(completeRequest(91));
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0].outcome,
             DisplayTransaction::ApplyOutcome::Rejected);

    outputPointer->submitStatus = SubmitStatus::Unavailable;
    port.requestApply(completeRequest(92));
    QTRY_COMPARE(observer.completions.size(), 2);
    QCOMPARE(observer.completions[1].outcome,
             DisplayTransaction::ApplyOutcome::TransportUncertain);
}

void DisplayWriterPortTests::rebindsAfterImmediateStopAndRestart()
{
    auto output = std::make_unique<FakeOutputManagementPort>();
    auto *outputPointer = output.get();
    outputPointer->detachObserverOnStop = true;
    WriterTransactionPort port(std::move(output),
                               std::make_unique<FakeJournalStore>(), 100);
    RecordingObserver observer;
    port.setObserver(&observer);
    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(20, true);
    port.beginMachineLineage(50);

    port.requestApply(completeRequest(101));
    const quint64 abandonedId = outputPointer->submissions.constLast().requestId;
    outputPointer->publishOwner(21, false);
    port.stop();
    QTRY_COMPARE(observer.completions.size(), 1);
    QCOMPARE(observer.completions[0],
             (Completion{50, 101,
                         DisplayTransaction::ApplyOutcome::TransportUncertain}));

    QCOMPARE(port.start(), PortStartStatus::Started);
    outputPointer->publishOwner(22, true);
    port.beginMachineLineage(51);
    port.requestApply(completeRequest(102));
    const quint64 restartedId = outputPointer->submissions.constLast().requestId;
    outputPointer->complete(21, abandonedId, CompletionOutcome::Applied);
    outputPointer->complete(22, restartedId, CompletionOutcome::Applied);
    QTRY_COMPARE(observer.completions.size(), 2);
    QCOMPARE(observer.completions[1],
             (Completion{51, 102, DisplayTransaction::ApplyOutcome::Applied}));
}

QTEST_GUILESS_MAIN(DisplayWriterPortTests)
#include "tst_display_writer_port.moc"
