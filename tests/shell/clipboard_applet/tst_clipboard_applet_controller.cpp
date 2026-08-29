// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

// Scripted client seam issuing deliberately unique-but-unordered request ids
// and delivering replies in caller-chosen order. AGENT-GUARD: the public seam
// promises id uniqueness only — these ids reproduce the exact disorder a real
// async transport can produce, which the controller must fence with its own
// monotonic query generation rather than id arithmetic.
class UnorderedFakeClient final : public ClipboardClientInterface {
public:
    quint64 m_nextSearchRequestId = 0;
    QList<quint64> m_issuedSearchRequestIds;

    HistorySnapshot m_snapshot;
    bool m_locked = false;

    [[nodiscard]] ClientState clientState() const noexcept override { return ClientState::Ready; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return QStringLiteral("fake"); }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return true; }
    [[nodiscard]] bool isLocked() const noexcept override { return m_locked; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return m_snapshot; }

    quint64 requestPromote(EntryId, quint32, quint64) override { return 0; }
    quint64 requestRemove(EntryId, quint32) override { return 0; }
    quint64 requestSetPinned(EntryId, bool, quint32) override { return 0; }
    quint64 requestClear(ClearScope, quint32) override { return 0; }

    quint64 requestSearch(const QString &, quint32, int) override
    {
        // Unique, strictly unordered: even requests descend from 902,
        // odd requests ascend from 101, so consecutive ids are never
        // monotonically comparable in either direction.
        const quint64 id = (m_nextSearchRequestId % 2 == 0)
            ? quint64(902 - 100 * (m_nextSearchRequestId / 2))
            : quint64(101 + 100 * (m_nextSearchRequestId / 2));
        ++m_nextSearchRequestId;
        m_issuedSearchRequestIds.append(id);
        return id;
    }

    void deliverSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        Q_EMIT searchCompleted(requestId, outcome);
    }

    void deliverLock(bool locked)
    {
        m_locked = locked;
        Q_EMIT lockStateChanged(locked);
    }
};

// Hostile/async scripted seam for synchronous-flush and completion-injection
// attacks. Every request id is unique; replies and completions are delivered
// exactly when the test chooses, including re-entrantly inside dispatch calls
// (the P1 vector: flushing a queued superseded reply inside requestSearch()).
class HostileScriptedClient final : public ClipboardClientInterface {
public:
    struct RecordedOperation {
        OperationKind kind = OperationKind::Promote;
        quint64 requestId = 0;
        quint64 tick = 0;
        EntryId id;
    };

    quint64 m_nextRequestId = 500; // deliberately unrelated to search ids
    QList<RecordedOperation> m_operations;
    quint64 m_queuedSearchRequestId = 0;
    SearchOutcome m_queuedSearchOutcome;
    bool m_flushQueuedReplyDuringNextSearch = false;
    bool m_answerSearchSynchronously = false;
    SearchOutcome m_scriptedSearchOutcome;
    bool m_completeOperationsSynchronously = false;
    OperationOutcome m_scriptedCompletion;

    HistorySnapshot m_snapshot;
    bool m_locked = false;

    [[nodiscard]] ClientState clientState() const noexcept override { return ClientState::Ready; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return QStringLiteral("hostile-fake"); }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return true; }
    [[nodiscard]] bool isLocked() const noexcept override { return m_locked; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return m_snapshot; }

    quint64 requestPromote(EntryId id, quint32, quint64 tick) override
    {
        return record(OperationKind::Promote, id, tick);
    }
    quint64 requestRemove(EntryId id, quint32) override
    {
        return record(OperationKind::Remove, id, 0);
    }
    quint64 requestSetPinned(EntryId id, bool, quint32) override
    {
        return record(OperationKind::SetPinned, id, 0);
    }
    quint64 requestClear(ClearScope, quint32) override
    {
        return record(OperationKind::Clear, {}, 0);
    }

    quint64 requestSearch(const QString &, quint32, int) override
    {
        const quint64 id = ++m_nextRequestId;
        if (m_flushQueuedReplyDuringNextSearch) {
            // Tarski's P1-2 vector: the stale queued reply is flushed
            // re-entrantly while the new request is being issued, before
            // the controller can know the new request id.
            m_flushQueuedReplyDuringNextSearch = false;
            Q_EMIT searchCompleted(m_queuedSearchRequestId, m_queuedSearchOutcome);
        }
        if (m_answerSearchSynchronously) {
            // Benign synchronous seam: answers with the id of the request
            // currently being issued, inside the call itself.
            Q_EMIT searchCompleted(id, m_scriptedSearchOutcome);
        }
        return id;
    }

    void emitSnapshot()
    {
        Q_EMIT snapshotChanged(m_snapshot);
    }

    void queueSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        m_queuedSearchRequestId = requestId;
        m_queuedSearchOutcome = outcome;
    }

    void emitSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        Q_EMIT searchCompleted(requestId, outcome);
    }

    void emitCompletion(quint64 requestId, const OperationOutcome &outcome)
    {
        Q_EMIT operationCompleted(requestId, outcome);
    }

private:
    quint64 record(OperationKind kind, EntryId id, quint64 tick)
    {
        const quint64 requestId = ++m_nextRequestId;
        RecordedOperation recorded;
        recorded.kind = kind;
        recorded.requestId = requestId;
        recorded.tick = tick;
        recorded.id = id;
        m_operations.append(recorded);

        if (m_completeOperationsSynchronously) {
            OperationOutcome completion = m_scriptedCompletion;
            completion.id = id;
            Q_EMIT operationCompleted(requestId, completion);
        }
        return requestId;
    }
};

class TstClipboardAppletController : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitialState();
    void testLockGating();
    void testLockPurgesModelAndPreventsRedisclosure();
    void testLockPurgesActiveSearchState();
    void testOwnerFencing();
    void testGenerationFencing();
    void testIntentOperations();
    void testSearchLifecycle();
    void testSearchReplyFreshnessWithUnorderedIds();
    void testHostileSynchronousFlushCannotDisplaySupersededReply();
    void testSynchronousCompletionLeavesNoPendingRecord();
    void testUnknownAndDuplicateCompletionsAreIgnored();
    void testPromoteTicksAreStrictlyMonotonic();
    void testFeedbackHandling();
    void testPendingTracking();
    void testRapidStateAndLockTransitions();
    void testLineageExhaustionFailsClosed();
};

void TstClipboardAppletController::testInitialState()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "hello" } };
    const auto admitRes1 = model.admit(val, 1, QStringLiteral("Terminal"), 100);
    QVERIFY(admitRes1.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.isLocked(), false);
    QCOMPARE(controller.isHistoryEnabled(), true);
    QCOMPARE(controller.entryCount(), 1);
    QCOMPARE(controller.pinnedCount(), 0);
    QCOMPARE(controller.unpinnedCount(), 1);
    QCOMPARE(controller.isSearchActive(), false);
    QVERIFY(!controller.feedbackPresent());
}

void TstClipboardAppletController::testLockGating()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "secret" } };
    const auto admitRes2 = model.admit(val, 1, QStringLiteral("PassMgr"), 100);
    QVERIFY(admitRes2.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    // Lock session
    adapter.setLocked(true);
    QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
    QCOMPARE(controller.isLocked(), true);
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.entryRows().isEmpty());

    // Actions must fail closed while locked
    QVERIFY(!controller.selectEntry(1, 1));
    QVERIFY(controller.feedbackPresent());

    // Unlock session: the pre-lock entry was purged on lock and must never
    // reappear; the surface returns ready but empty.
    adapter.setLocked(false);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.isLocked(), false);
    QCOMPARE(controller.entryCount(), 0);
}

void TstClipboardAppletController::testLockPurgesModelAndPreventsRedisclosure()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "pre-lock-secret" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("PassMgr"), 100);
    QVERIFY(admitted.accepted());
    const quint32 generationBeforeLock = model.generation();

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);
    QCOMPARE(controller.entryCount(), 1);

    // The lock is an authority denial: model privacy flips to Denied, every
    // entry is purged, and the generation advances by exactly one so the
    // entire pre-lock lineage (ids, pending intents, snapshots) is fenced.
    adapter.setLocked(true);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);
    QCOMPARE(model.generation(), generationBeforeLock + 1);
    QVERIFY(model.snapshot().entries.isEmpty());
    QCOMPARE(controller.entryCount(), 0);

    // Unlock restores the authority flag only; purged content cannot return.
    adapter.setLocked(false);
    QCOMPARE(model.privacyState(), PrivacyState::Allowed);
    QCOMPARE(model.generation(), generationBeforeLock + 1);
    QCOMPARE(controller.entryCount(), 0);

    // A stale pre-lock id is refused even with the new generation number.
    const auto promoteRes = model.promote(admitted.entry.id, generationBeforeLock + 1, 200);
    QCOMPARE(promoteRes.error, ClipboardError::UnknownEntry);
}

void TstClipboardAppletController::testLockPurgesActiveSearchState()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "searchable secret" } };
    QVERIFY(model.admit(val, 1, QStringLiteral("Notes"), 100).accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    controller.setSearchQuery(QStringLiteral("searchable"));
    QCOMPARE(controller.isSearchActive(), true);
    QCOMPARE(controller.searchResultCount(), 1);

    // Locking must drop the query, the matched descriptors, and the
    // truncated flag — not merely hide them behind the locked phase.
    adapter.setLocked(true);
    QCOMPARE(controller.isSearchActive(), false);
    QCOMPARE(controller.searchQuery(), QString());
    QCOMPARE(controller.searchResultCount(), 0);
    QCOMPARE(controller.searchTruncated(), false);
    QCOMPARE(controller.entryCount(), 0);

    adapter.setLocked(false);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.isSearchActive(), false);
    QCOMPARE(controller.searchResultCount(), 0);
}

void TstClipboardAppletController::testOwnerFencing()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

    // Owner lost
    adapter.setOwner(QStringLiteral("org.qindaqt.ClipboardService"), false);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.entryCount(), 0);

    // Actions rejected
    QVERIFY(!controller.deleteEntry(1, 1));

    // Owner recovered
    adapter.setOwner(QStringLiteral("org.qindaqt.ClipboardService"), true);
    adapter.setClientState(ClientState::Ready);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
}

void TstClipboardAppletController::testGenerationFencing()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "item1" } };
    const auto admitRes3 = model.admit(val, 1, QStringLiteral("Kate"), 100);
    QVERIFY(admitRes3.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    // Purge model to raise generation to 2
    model.setPrivacyAllowed(false);
    model.setPrivacyAllowed(true);
    adapter.notifyModelChanged();

    QCOMPARE(model.generation(), 2u);

    // Calling action with stale generation 1 must be rejected locally
    const bool accepted = controller.selectEntry(1, 1);
    QVERIFY(!accepted);
    QVERIFY(controller.feedbackPresent());
    QVERIFY(controller.feedback().contains(QLatin1String("changed")));
}

void TstClipboardAppletController::testIntentOperations()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val1;
    val1.formats = { { QStringLiteral("text/plain"), "item1" } };
    const auto outcome1 = model.admit(val1, 1, QStringLiteral("App1"), 100);

    ClipboardValue val2;
    val2.formats = { { QStringLiteral("text/plain"), "item2" } };
    const auto outcome2 = model.admit(val2, 1, QStringLiteral("App2"), 110);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QCOMPARE(controller.entryCount(), 2);

    // 1. Toggle pin
    QVERIFY(controller.togglePin(outcome1.entry.id.generation, outcome1.entry.id.serial));
    QCOMPARE(controller.pinnedCount(), 1);

    // 2. Select / promote item 1 (moves to top)
    QVERIFY(controller.selectEntry(outcome1.entry.id.generation, outcome1.entry.id.serial));
    QCOMPARE(controller.entryCount(), 2);

    // 3. Clear unpinned (removes item 2, retains item 1)
    QVERIFY(controller.clearHistory(true));
    QCOMPARE(controller.entryCount(), 1);
    QCOMPARE(controller.pinnedCount(), 1);

    // 4. Delete item 1
    QVERIFY(controller.deleteEntry(outcome1.entry.id.generation, outcome1.entry.id.serial));
    QCOMPARE(controller.entryCount(), 0);
}

void TstClipboardAppletController::testSearchLifecycle()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val1;
    val1.formats = { { QStringLiteral("text/plain"), "Alpha beta gamma" } };
    const auto admitRes4 = model.admit(val1, 1, QStringLiteral("Notes"), 100);
    QVERIFY(admitRes4.accepted());

    ClipboardValue val2;
    val2.formats = { { QStringLiteral("text/plain"), "Delta epsilon" } };
    const auto admitRes5 = model.admit(val2, 1, QStringLiteral("Browser"), 110);
    QVERIFY(admitRes5.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QCOMPARE(controller.entryCount(), 2);

    // Search for "Alpha"
    controller.setSearchQuery(QStringLiteral("Alpha"));
    QCOMPARE(controller.isSearchActive(), true);
    QCOMPARE(controller.searchQuery(), QStringLiteral("Alpha"));
    QCOMPARE(controller.entryCount(), 1);

    // Clear search
    controller.clearSearch();
    QCOMPARE(controller.isSearchActive(), false);
    QCOMPARE(controller.entryCount(), 2);

    // Query clamping test
    const QString hugeQuery(200, QLatin1Char('x'));
    controller.setSearchQuery(hugeQuery);
    QVERIFY(controller.searchQuery().length() <= kMaxSearchQueryLength);
}

void TstClipboardAppletController::testHostileSynchronousFlushCannotDisplaySupersededReply()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;

    ClipboardAppletController controller(&client);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

    auto outcomeWithPreview = [](const char *preview) {
        SearchOutcome outcome;
        ClipboardEntryDescriptor desc;
        desc.id = { 3, 7 };
        desc.preview = QString::fromLatin1(preview);
        desc.formats = { { QStringLiteral("text/plain"), 4 } };
        outcome.matches.append(desc);
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

void TstClipboardAppletController::testSynchronousCompletionLeavesNoPendingRecord()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    ClipboardEntryDescriptor desc;
    desc.id = { 3, 7 };
    desc.preview = QStringLiteral("entry");
    desc.formats = { { QStringLiteral("text/plain"), 5 } };
    client.m_snapshot.entries.append(desc);

    client.m_completeOperationsSynchronously = true;
    client.m_scriptedCompletion.code = OperationErrorCode::None;

    ClipboardAppletController controller(&client);

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

void TstClipboardAppletController::testUnknownAndDuplicateCompletionsAreIgnored()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    ClipboardEntryDescriptor desc;
    desc.id = { 3, 7 };
    desc.preview = QStringLiteral("entry");
    desc.formats = { { QStringLiteral("text/plain"), 5 } };
    client.m_snapshot.entries.append(desc);

    ClipboardAppletController controller(&client);

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

void TstClipboardAppletController::testPromoteTicksAreStrictlyMonotonic()
{
    HostileScriptedClient client;
    client.m_snapshot.generation = 3;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;
    ClipboardEntryDescriptor desc;
    desc.id = { 3, 7 };
    desc.preview = QStringLiteral("entry");
    desc.admittedTick = 999998;
    desc.lastUsedTick = 1000000;
    desc.formats = { { QStringLiteral("text/plain"), 5 } };
    client.m_snapshot.entries.append(desc);

    client.m_completeOperationsSynchronously = true;
    client.m_scriptedCompletion.code = OperationErrorCode::None;

    ClipboardAppletController controller(&client);

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

void TstClipboardAppletController::testFeedbackHandling()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    QVERIFY(!controller.feedbackPresent());

    // Try deleting a nonexistent entry to trigger feedback
    controller.deleteEntry(1, 999);
    QVERIFY(controller.feedbackPresent());
    QVERIFY(!controller.feedback().isEmpty());

    // Dismiss feedback
    controller.clearFeedback();
    QVERIFY(!controller.feedbackPresent());
    QVERIFY(controller.feedback().isEmpty());
}

void TstClipboardAppletController::testPendingTracking()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "item" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("Kate"), 100);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    // Dispatch select
    QVERIFY(controller.selectEntry(admitted.entry.id.generation, admitted.entry.id.serial));
}

void TstClipboardAppletController::testRapidStateAndLockTransitions()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "item" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("Kate"), 100);
    QVERIFY(admitted.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    quint32 expectedGeneration = model.generation();
    // Rapid lock/unlock toggles: each lock purges content and fences the
    // lineage; once locked, the entry can never reappear on later unlocks.
    for (int i = 0; i < 10; ++i) {
        adapter.setLocked(i % 2 == 1);
        QCOMPARE(controller.isLocked(), i % 2 == 1);
        if (controller.isLocked()) {
            expectedGeneration += 1;
            QCOMPARE(model.generation(), expectedGeneration);
            QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
            QCOMPARE(controller.entryCount(), 0);
            QVERIFY(!controller.selectEntry(admitted.entry.id.generation, admitted.entry.id.serial));
        } else {
            QCOMPARE(model.generation(), expectedGeneration);
            QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
            // Only the initial unlocked state still holds the entry; every
            // unlocked iteration after the first lock sees purged content.
            QCOMPARE(controller.entryCount(), i == 0 ? 1 : 0);
        }
    }
}

void TstClipboardAppletController::testSearchReplyFreshnessWithUnorderedIds()
{
    UnorderedFakeClient client;
    client.m_snapshot.generation = 7;
    client.m_snapshot.historyEnabled = true;
    client.m_snapshot.privacyAllowed = true;

    ClipboardAppletController controller(&client);
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
        ClipboardEntryDescriptor desc;
        desc.id = { 7, 3 };
        desc.preview = QString::fromLatin1(preview);
        desc.formats = { { QStringLiteral("text/plain"), 4 } };
        outcome.matches.append(desc);
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

void TstClipboardAppletController::testLineageExhaustionFailsClosed()
{
    // Model initialized at generation ceiling
    HistoryCounters counters;
    counters.generation = std::numeric_limits<quint32>::max();
    counters.nextSerial = 1;
    counters.revision = 1;

    ClipboardHistoryModel model(HistoryLimits{}, counters);
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    // Trigger a purge at ceiling to latch generationExhausted
    model.setPrivacyAllowed(false);
    model.setPrivacyAllowed(true);
    adapter.notifyModelChanged();

    // Dispatch intent - model seam will refuse due to LineageExhausted and emit error feedback
    controller.selectEntry(counters.generation, 1);
    QVERIFY(controller.feedbackPresent());
    QCOMPARE(controller.feedbackStatus(), QStringLiteral("error"));
}

QTEST_MAIN(TstClipboardAppletController)
#include "tst_clipboard_applet_controller.moc"
