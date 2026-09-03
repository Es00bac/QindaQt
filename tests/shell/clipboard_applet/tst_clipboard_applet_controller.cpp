// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

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
    void testFeedbackHandling();
    void testPendingTracking();
    void testRapidStateAndLockTransitions();
    void testLineageExhaustionFailsClosed();
    void testReadDenialWithholdsObservation();
    void testWriteDenialKeepsBrowsingButRefusesMutation();
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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);
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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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





void TstClipboardAppletController::testFeedbackHandling()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

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
    ClipboardAppletController controller(&adapter, true, true);

    // Trigger a purge at ceiling to latch generationExhausted
    model.setPrivacyAllowed(false);
    model.setPrivacyAllowed(true);
    adapter.notifyModelChanged();

    // Dispatch intent - model seam will refuse due to LineageExhausted and emit error feedback
    controller.selectEntry(counters.generation, 1);
    QVERIFY(controller.feedbackPresent());
    QCOMPARE(controller.feedbackStatus(), QStringLiteral("error"));
}

void TstClipboardAppletController::testReadDenialWithholdsObservation()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "granted-secret" } };
    QVERIFY(model.admit(val, 1, QStringLiteral("Notes"), 100).accepted());

    ClipboardModelClientAdapter adapter(&model);
    // clipboard.read denied, clipboard.write granted: observation must fail
    // closed even though mutation authority exists.
    ClipboardAppletController controller(&adapter, false, true);

    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.phaseReasonText(),
             QStringLiteral("Clipboard service unavailable: clipboard-read-not-granted"));
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.entryRows().isEmpty());
    QVERIFY(!controller.clipboardReadGranted());
    QVERIFY(controller.clipboardWriteGranted());

    // Late snapshots must not stock the withheld projection.
    adapter.notifyModelChanged();
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));

    // Search is a read: it must not reach the seam at all.
    controller.setSearchQuery(QStringLiteral("granted"));
    QCOMPARE(controller.isSearchActive(), false);

    // Mutations are refused locally with honest feedback, before dispatch.
    QVERIFY(!controller.deleteEntry(1, 1));
    QVERIFY(controller.feedbackPresent());
}

void TstClipboardAppletController::testWriteDenialKeepsBrowsingButRefusesMutation()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "readonly item" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("Kate"), 100);
    QVERIFY(admitted.accepted());

    ClipboardModelClientAdapter adapter(&model);
    // clipboard.read granted, clipboard.write denied: browsing and search
    // stay live; every mutating intent is refused before dispatch.
    ClipboardAppletController controller(&adapter, true, false);

    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    controller.setSearchQuery(QStringLiteral("readonly"));
    QCOMPARE(controller.searchResultCount(), 1);

    const quint32 generation = admitted.entry.id.generation;
    const quint32 serial = admitted.entry.id.serial;
    QVERIFY(!controller.selectEntry(generation, serial));
    QVERIFY(!controller.deleteEntry(generation, serial));
    QVERIFY(!controller.togglePin(generation, serial));
    QVERIFY(!controller.clearHistory(false));
    QVERIFY(controller.feedbackPresent());

    // Nothing reached the model: the entry is intact and nothing is pending.
    QCOMPARE(model.snapshot().entries.size(), 1);
    QCOMPARE(controller.pendingOperationCount(), 0);
    QCOMPARE(controller.entryCount(), 1);
}


QTEST_MAIN(TstClipboardAppletController)
#include "tst_clipboard_applet_controller.moc"
