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
    void testSnapshotRevisionChangeReissuesAndFencesSearch();
    void testSearchMatchesMustBelongToDispatchSnapshot();
    void testHostileSynchronousFlushCannotDisplaySupersededReply();
    void testSynchronousCompletionLeavesNoPendingRecord();
    void testUnknownAndDuplicateCompletionsAreIgnored();
    void testPromoteTicksAreStrictlyMonotonic();
    void testDeferredCompletionForEarlierRequestAttributesById();
};

void TstClipboardAppletFencing::testSnapshotRevisionChangeReissuesAndFencesSearch()
{
    // AGENT-NOTE (P1-1): e3e2dba fenced live queries only when generation
    // changed. A same-generation removal advances revision, so its late search
    // reply could re-present metadata that no longer exists in the snapshot.
    HostileScriptedClient client;
    client.m_snapshot.generation = 7;
    client.m_snapshot.revision = 1;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    const auto removed = floorValidDescriptor(7, 3, QStringLiteral("secret"));
    setSnapshotEntries(client.m_snapshot, {removed});

    ClipboardAppletController controller(&client, true, true);
    controller.setSearchQuery(QStringLiteral("secret"));
    QCOMPARE(client.m_nextRequestId, quint64(501));
    const quint64 revisionOneRequest = client.m_nextRequestId;

    client.m_snapshot.revision = 2;
    client.m_snapshot.entries.clear();
    client.m_snapshot.totalPayloadBytes = 0;
    client.emitSnapshot();

    // A changed snapshot must issue a query pinned to its new complete
    // lineage, even though generation stayed seven.
    QCOMPARE(client.m_nextRequestId, quint64(502));
    QCOMPARE(controller.searchResultCount(), 0);

    SearchOutcome late;
    late.matches = {removed};
    client.emitSearchReply(revisionOneRequest, late);
    QCOMPARE(controller.searchResultCount(), 0);
    QCOMPARE(controller.entryCount(), 0);

    SearchOutcome current;
    client.emitSearchReply(client.m_nextRequestId, current);
    QCOMPARE(controller.searchResultCount(), 0);
    QCOMPARE(controller.entryCount(), 0);
}

void TstClipboardAppletFencing::testSearchMatchesMustBelongToDispatchSnapshot()
{
    // AGENT-NOTE (P1-1): request-id/generation fences are insufficient when a
    // hostile completion fabricates a floor-valid descriptor absent from the
    // exact snapshot entry set used to dispatch the query.
    HostileScriptedClient client;
    client.m_snapshot.generation = 8;
    client.m_snapshot.revision = 4;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    const auto member = floorValidDescriptor(8, 3, QStringLiteral("real metadata"));
    setSnapshotEntries(client.m_snapshot, {member});

    ClipboardAppletController controller(&client, true, true);
    controller.setSearchQuery(QStringLiteral("metadata"));
    const quint64 requestId = client.m_nextRequestId;

    SearchOutcome fabricated;
    fabricated.matches = {
        floorValidDescriptor(8, 4, QStringLiteral("fabricated metadata"))
    };
    client.emitSearchReply(requestId, fabricated);
    QCOMPARE(controller.searchResultCount(), 0);
    QCOMPARE(controller.entryCount(), 0);
}

void TstClipboardAppletFencing::testSearchReplyFreshnessWithUnorderedIds()
{
    UnorderedFakeClient client;
    client.m_snapshot.generation = 7;
    client.m_snapshot.revision = 1;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    const auto alpha = floorValidDescriptor(7, 3, QStringLiteral("alpha result"));
    const auto beta = floorValidDescriptor(7, 4, QStringLiteral("beta result"));
    const auto gamma = floorValidDescriptor(7, 5, QStringLiteral("gamma result"));
    setSnapshotEntries(client.m_snapshot, {alpha, beta, gamma});

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

    auto matchWith = [](const ClipboardEntryDescriptor &descriptor) {
        SearchOutcome outcome;
        outcome.matches.append(descriptor);
        return outcome;
    };

    // Late reply for the superseded "alpha" query must be dropped even
    // though its id (902) is numerically larger than the live query's (101).
    client.deliverSearchReply(alphaId, matchWith(alpha));
    QCOMPARE(controller.searchQuery(), QStringLiteral("beta"));
    QCOMPARE(controller.searchResultCount(), 0);

    // The live "beta" reply must be accepted.
    client.deliverSearchReply(betaId, matchWith(beta));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));

    // A duplicated or replayed stale reply must not replace current results.
    client.deliverSearchReply(alphaId, matchWith(alpha));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));

    // A third query supersedes "beta": its late reply is dropped after the
    // new dispatch, and only the newest reply updates the rows.
    controller.setSearchQuery(QStringLiteral("gamma"));
    QCOMPARE(client.m_issuedSearchRequestIds.size(), 3);
    const quint64 gammaId = client.m_issuedSearchRequestIds.last();
    client.deliverSearchReply(betaId, matchWith(beta));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("beta result"));
    client.deliverSearchReply(gammaId, matchWith(gamma));
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma result"));
}
void TstClipboardAppletFencing::testHostileSynchronousFlushCannotDisplaySupersededReply()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.revision = 1;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    const auto alpha = floorValidDescriptor(3, 7, QStringLiteral("alpha stale secret"));
    const auto gamma = floorValidDescriptor(3, 8, QStringLiteral("gamma live result"));
    const auto delta = floorValidDescriptor(3, 9, QStringLiteral("delta sync result"));
    setSnapshotEntries(client.m_snapshot, {alpha, gamma, delta});

    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

    auto outcomeWith = [](const ClipboardEntryDescriptor &descriptor) {
        SearchOutcome outcome;
        outcome.matches.append(descriptor);
        return outcome;
    };

    // Dispatch "alpha" (request id 501); the hostile seam holds its reply.
    controller.setSearchQuery(QStringLiteral("alpha"));
    const quint64 alphaId = 501;

    // Queue the stale alpha reply and arm the flush: issuing "gamma" (id 502)
    // will re-entrantly emit the alpha reply inside requestSearch().
    client.queueSearchReply(alphaId, outcomeWith(alpha));
    client.m_flushQueuedReplyDuringNextSearch = true;
    controller.setSearchQuery(QStringLiteral("gamma"));

    // The flushed superseded reply must not display anything.
    QCOMPARE(controller.searchQuery(), QStringLiteral("gamma"));
    QCOMPARE(controller.searchResultCount(), 0);

    // The real gamma reply — async, correct id — is accepted.
    const quint64 gammaId = 502;
    client.emitSearchReply(gammaId, outcomeWith(gamma));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma live result"));

    // Replaying the stale alpha reply afterwards still changes nothing.
    client.emitSearchReply(alphaId, outcomeWith(alpha));
    QCOMPARE(controller.searchResultCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("gamma live result"));

    // Benign variant: a seam that ALSO answers the new request synchronously
    // inside the call still attributes correctly by id.
    client.m_answerSearchSynchronously = true;
    client.m_scriptedSearchOutcome = outcomeWith(delta);
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
    setSnapshotEntries(client.m_snapshot,
                       {floorValidDescriptor(3, 7, QStringLiteral("entry"))});

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
    setSnapshotEntries(client.m_snapshot,
                       {floorValidDescriptor(3, 7, QStringLiteral("entry"))});

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
    client.m_snapshot.revision = 1;
    setSnapshotEntries(client.m_snapshot, {desc});

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
    client.m_snapshot.revision = 2;
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
    setSnapshotEntries(
        client.m_snapshot,
        {floorValidDescriptor(3, 7, QStringLiteral("first")),
         floorValidDescriptor(3, 8, QStringLiteral("second"))});

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
