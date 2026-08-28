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
    void testOwnerFencing();
    void testGenerationFencing();
    void testIntentOperations();
    void testSearchLifecycle();
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

    // Unlock session
    adapter.setLocked(false);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.isLocked(), false);
    QCOMPARE(controller.entryCount(), 1);
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

    // Rapid toggle lock
    for (int i = 0; i < 10; ++i) {
        adapter.setLocked(i % 2 == 1);
        QCOMPARE(controller.isLocked(), i % 2 == 1);
        if (controller.isLocked()) {
            QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
            QCOMPARE(controller.entryCount(), 0);
            QVERIFY(!controller.selectEntry(admitted.entry.id.generation, admitted.entry.id.serial));
        } else {
            QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
            QCOMPARE(controller.entryCount(), 1);
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
