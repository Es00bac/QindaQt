// SPDX-License-Identifier: LGPL-3.0-or-later

// Reply/completion attribution fencing for the Clipboard applet controller:
// unique-but-unordered request ids, hostile synchronous flushes inside
// dispatch calls, injected/duplicated completions, and monotonic promote
// ticks. Lifecycle, authority, and capability gating live in
// tst_clipboard_applet_controller.cpp.

#include <QtTest/QtTest>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>

#include "clipboard_applet_test_fakes.h"

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::ShellClipboardApplet::Tests;
using namespace QindaQt::Services::ClipboardModel;

class TstClipboardAppletFencing : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testSearchReplyFreshnessWithUnorderedIds();
    void testHostileSynchronousFlushCannotDisplaySupersededReply();
    void testSynchronousCompletionLeavesNoPendingRecord();
    void testUnknownAndDuplicateCompletionsAreIgnored();
    void testPromoteTicksAreStrictlyMonotonic();
    void testDeferredCompletionForEarlierRequestAttributesById();
};

void TstClipboardAppletFencing::testSearchReplyFreshnessWithUnorderedIds()
{
    UnorderedFakeClient client;
    client.m_snapshot.generation = 7;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;

    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

    // First query dispatches request id 902; a revision of the query
    // dispatches id 101 — numerically SMALLER, exactly the disorder the
    // unique-but-unordered seam contract permits.
    controller.setSearchQuery(QStringLiteral("alpha"));
    QCOMPARE(client.m_issuedSearchRequestIds.size(), 1);
    const quint64 alphaId = client.m_issuedSearchRequestIds.first();
    QCOMPARE(alphaId, quint64(902));

    controller.setSearchQuery(QStringLiteral("beta"));
    QCOMPARE(client.m_issuedSearchRequestIds.size(), 2);
    const quint64 betaId = client.m_issuedSearchRequestIds.at(1);
    QCOMPARE(betaId, quint64(101));
    QVERIFY(betaId < alphaId);

    auto matchWith = [](const char *preview) {
        SearchOutcome outcome;
        outcome.matches.append(
            floorValidDescriptor(7, 3, QString::fromLatin1(preview)));
        return outcome;
    };

    // Late reply for the superseded "alpha" query must be dropped even
    // though its id (902) is numerically larger than the live query's (101).
    client.deliverSearchReply(alphaId, matchWith("alpha result"));
    QCOMPARE(controller.searchQuery(), QStringLiteral("beta"));
    QCOMPARE(controller.searchResultCount(), 0);

    // The live "beta" reply must be accepted.
    client.deliverSearchReply(betaId, matchWith("beta result"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));

    // A duplicated or replayed stale reply must not replace current results.
    client.deliverSearchReply(alphaId, matchWith("alpha hijack"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));

    // A third query supersedes "beta": its late reply is dropped after the
    // new dispatch, and only the newest reply updates the rows.
    controller.setSearchQuery(QStringLiteral("gamma"));
    QCOMPARE(client.m_issuedSearchRequestIds.size(), 3);
    const quint64 gammaId = client.m_issuedSearchRequestIds.last();
    client.deliverSearchReply(betaId, matchWith("beta hijack"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));
    client.deliverSearchReply(gammaId, matchWith("gamma result"));
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma result"));
}
void TstClipboardAppletFencing::testHostileSynchronousFlushCannotDisplaySupersededReply()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;

    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

    auto outcomeWithPreview = [](const char *preview) {
        SearchOutcome outcome;
        outcome.matches.append(
            floorValidDescriptor(3, 7, QString::fromLatin1(preview)));
        return outcome;
    };

    // Dispatch "alpha" (request id 501); the hostile seam holds its reply.
    controller.setSearchQuery(QStringLiteral("alpha"));
    const quint64 alphaId = 501;

    // Queue the stale alpha reply and arm the flush: issuing "gamma" (id 502)
    // will re-entrantly emit the alpha reply inside requestSearch().
    client.queueSearchReply(alphaId, outcomeWithPreview("alpha stale secret"));
    client.m_flushQueuedReplyDuringNextSearch = true;
    controller.setSearchQuery(QStringLiteral("gamma"));

    // The flushed superseded reply must not display anything.
    QCOMPARE(controller.searchQuery(), QStringLiteral("gamma"));
    QCOMPARE(controller.searchResultCount(), 0);

    // The real gamma reply — async, correct id — is accepted.
    const quint64 gammaId = 502;
    client.emitSearchReply(gammaId, outcomeWithPreview("gamma live result"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma live result"));

    // Replaying the stale alpha reply afterwards still changes nothing.
    client.emitSearchReply(alphaId, outcomeWithPreview("alpha replay hijack"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma live result"));

    // Benign variant: a seam that ALSO answers the new request synchronously
    // inside the call still attributes correctly by id.
    client.m_answerSearchSynchronously = true;
    client.m_scriptedSearchOutcome = outcomeWithPreview("delta sync result");
    controller.setSearchQuery(QStringLiteral("delta"));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("delta sync result"));
}
void TstClipboardAppletFencing::testSynchronousCompletionLeavesNoPendingRecord()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    client.m_snapshot.entries.append(
        floorValidDescriptor(3, 7, QStringLiteral("entry")));

    client.m_completeOperationsSynchronously = true;
    client.m_scriptedCompletion.code = OperationErrorCode::None;

    ClipboardAppletController controller(&client, true, true);

    // Clear-history completing synchronously inside requestClear() must not
    // leak a permanently pending record.
    QVERIFY(controller.clearHistory(true));
    QCOMPARE(controller.pendingOperationCount(), 0);

    // Promote with a synchronous success: marker and record both resolved.
    QVERIFY(controller.selectEntry(3, 7));
    QCOMPARE(controller.pendingOperationCount(), 0);
    QVERIFY(!controller.feedbackPresent());

    // Synchronous refusal: typed feedback appears, still no leaked record.
    client.m_scriptedCompletion.code = OperationErrorCode::StaleGeneration;
    client.m_scriptedCompletion.message = QStringLiteral("stale");
    QVERIFY(controller.deleteEntry(3, 7));
    QCOMPARE(controller.pendingOperationCount(), 0);
    QVERIFY(controller.feedbackPresent());
    QCOMPARE(controller.feedback(), QStringLiteral("stale"));
}
void TstClipboardAppletFencing::testUnknownAndDuplicateCompletionsAreIgnored()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    client.m_snapshot.entries.append(
        floorValidDescriptor(3, 7, QStringLiteral("entry")));

    ClipboardAppletController controller(&client, true, true);

    QVERIFY(controller.selectEntry(3, 7));
    QCOMPARE(controller.pendingOperationCount(), 1);
    const quint64 issuedId = client.m_operations.first().requestId;

    // Unknown-id completion carrying a VALID entry payload: hostile noise.
    OperationOutcome injected;
    injected.code = OperationErrorCode::None;
    injected.id = { 3, 7 };
    client.emitCompletion(issuedId + 987654, injected);
    QCOMPARE(controller.pendingOperationCount(), 1);
    QVERIFY(!controller.feedbackPresent());

    // The pending marker survived the injection attempt: the same entry is
    // still in flight and a second intent is refused.
    QVERIFY(!controller.selectEntry(3, 7));

    // The genuine completion clears exactly its own record.
    OperationOutcome genuine;
    genuine.code = OperationErrorCode::None;
    genuine.id = { 3, 7 };
    client.emitCompletion(issuedId, genuine);
    QCOMPARE(controller.pendingOperationCount(), 0);
    QVERIFY(controller.selectEntry(3, 7));
    QCOMPARE(controller.pendingOperationCount(), 1);

    // A duplicated completion of an already-consumed id is ignored.
    client.emitCompletion(issuedId, genuine);
    QCOMPARE(controller.pendingOperationCount(), 1);
}
void TstClipboardAppletFencing::testPromoteTicksAreStrictlyMonotonic()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    auto desc = floorValidDescriptor(3, 7, QStringLiteral("entry"));
    desc.admittedTick = 999998;
    desc.lastUsedTick = 1000000;
    client.m_snapshot.entries.append(desc);

    client.m_completeOperationsSynchronously = true;
    client.m_scriptedCompletion.code = OperationErrorCode::None;

    ClipboardAppletController controller(&client, true, true);

    QVERIFY(controller.selectEntry(3, 7));
    QVERIFY(controller.selectEntry(3, 7));
    const quint64 firstTick = client.m_operations.at(0).tick;
    const quint64 secondTick = client.m_operations.at(1).tick;

    // Issued ticks rise above every tick observed in the snapshot; wall
    // clock is never consulted.
    QVERIFY(firstTick > quint64(1000000));
    QVERIFY(secondTick > firstTick);

    // A snapshot whose entries carry even higher ticks lifts the floor.
    client.m_snapshot.entries.first().lastUsedTick = secondTick + 500;
    client.emitSnapshot();
    QVERIFY(controller.selectEntry(3, 7));
    const quint64 thirdTick = client.m_operations.at(2).tick;
    QVERIFY(thirdTick > secondTick + 500);
}
void TstClipboardAppletFencing::testDeferredCompletionForEarlierRequestAttributesById()
{
    // AGENT-GUARD (P2 regression): a seam that completes an EARLIER pending
    // request synchronously inside a LATER dispatch call must have that
    // completion attributed by its exact registered id. Dropping it would
    // strand a permanently pending record and pin the entry's busy marker.
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    client.m_snapshot.entries.append(
        floorValidDescriptor(3, 7, QStringLiteral("first")));
    client.m_snapshot.entries.append(
        floorValidDescriptor(3, 8, QStringLiteral("second")));

    ClipboardAppletController controller(&client, true, true);

    // Dispatch remove for the first entry; the seam holds the completion.
    QVERIFY(controller.deleteEntry(3, 7));
    QCOMPARE(controller.pendingOperationCount(), 1);
    const quint64 removeId = client.m_operations.first().requestId;

    // Arm the seam: while the promote for the second entry is being issued,
    // it re-entrantly flushes the completion of the EARLIER remove first,
    // then completes the promote itself.
    client.m_completeOperationsSynchronously = true;
    client.m_scriptedCompletion.code = OperationErrorCode::None;
    OperationOutcome earlierCompletion;
    earlierCompletion.code = OperationErrorCode::None;
    earlierCompletion.id = { 3, 7 };
    client.m_flushEarlierCompletionDuringRecord = true;
    client.m_flushedEarlierRequestId = removeId;
    client.m_flushedEarlierCompletion = earlierCompletion;

    QVERIFY(controller.selectEntry(3, 8));
    // Both the earlier remove and the current promote completed; no record
    // leaks and the first entry's pending marker is cleared.
    QCOMPARE(controller.pendingOperationCount(), 0);
}

QTEST_MAIN(TstClipboardAppletFencing)
#include "tst_clipboard_applet_fencing.moc"
