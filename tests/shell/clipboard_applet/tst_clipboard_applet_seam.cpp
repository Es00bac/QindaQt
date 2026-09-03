// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

#include <limits>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

class TstClipboardAppletSeam : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testClientSeamContracts();
    void testFailClosedLocking();
    void testLockPurgesContentAndFencesGeneration();
    void testLockPurgeAtGenerationCeilingIsValidButRequiresRestart();
    void testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase();
    void testIndependentHostDenialSurvivesUnlock();
    void testOverlappingHostDenialDuringLockSurvivesUnlock();
    void testSearchThroughSeam();
};

void TstClipboardAppletSeam::testClientSeamContracts()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);

    QCOMPARE(adapter.clientState(), ClientState::Ready);
    QCOMPARE(adapter.owner(), QStringLiteral("org.qindaqt.ClipboardService"));
    QCOMPARE(adapter.isOwnerAvailable(), true);
    QCOMPARE(adapter.isLocked(), false);

    // Verify snapshot passes metadata without exposing raw data
    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "Confidential data" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("Terminal"), 100);
    QVERIFY(admitted.accepted());

    const auto snapshot = adapter.snapshot();
    QCOMPARE(snapshot.entries.size(), 1);
    QCOMPARE(snapshot.entries.first().id, admitted.entry.id);
    QCOMPARE(snapshot.entries.first().preview, QStringLiteral("Confidential data"));

    // Pinning via seam
    quint64 reqId = adapter.requestSetPinned(admitted.entry.id, true, 1);
    QVERIFY(reqId > 0);

    const auto pinnedSnap = adapter.snapshot();
    QCOMPARE(pinnedSnap.entries.first().pinned, true);

    // Deletion via seam
    reqId = adapter.requestRemove(admitted.entry.id, 1);
    QVERIFY(reqId > 0);

    const auto emptySnap = adapter.snapshot();
    QCOMPARE(emptySnap.entries.size(), 0);
}

void TstClipboardAppletSeam::testFailClosedLocking()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "Secret" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("App"), 100);
    QVERIFY(admitted.accepted());

    ClipboardModelClientAdapter adapter(&model);

    // Lock
    adapter.setLocked(true);
    QCOMPARE(adapter.isLocked(), true);

    // Snapshot while locked must be empty
    const auto lockedSnap = adapter.snapshot();
    QVERIFY(lockedSnap.entries.isEmpty());
    QCOMPARE(lockedSnap.privacyAllowed, false);

    // Requests while locked must fail closed
    const quint64 reqId = adapter.requestPromote(admitted.entry.id, 1, 100);
    QVERIFY(reqId > 0);
}

void TstClipboardAppletSeam::testLockPurgesContentAndFencesGeneration()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "pre-lock-secret" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("App"), 100);
    QVERIFY(admitted.accepted());
    const quint32 generationBeforeLock = model.generation();

    ClipboardModelClientAdapter adapter(&model);
    QCOMPARE(adapter.snapshot().entries.size(), 1);

    // AGENT-GUARD: the lock must purge the model behind the seam and fence
    // the generation before the lock is observable, so no later unlock can
    // redisclose pre-lock content through this seam.
    QSignalSpy lockSpy(&adapter, &ClipboardClientInterface::lockStateChanged);
    adapter.setLocked(true);
    QCOMPARE(lockSpy.size(), 1);

    QCOMPARE(model.privacyState(), PrivacyState::Denied);
    QCOMPARE(model.generation(), generationBeforeLock + 1);
    QVERIFY(model.snapshot().entries.isEmpty());
    QVERIFY(adapter.snapshot().entries.isEmpty());

    // Unlock restores only the authority the lock removed.
    adapter.setLocked(false);
    QCOMPARE(model.privacyState(), PrivacyState::Allowed);
    QCOMPARE(model.generation(), generationBeforeLock + 1);
    QVERIFY(adapter.snapshot().entries.isEmpty());

    // Pre-lock ids never resolve again, even against the new generation.
    const auto promoteRes = model.promote(admitted.entry.id, model.generation(), 200);
    QCOMPARE(promoteRes.error, ClipboardError::UnknownEntry);
}

void TstClipboardAppletSeam::testLockPurgeAtGenerationCeilingIsValidButRequiresRestart()
{
    // AGENT-NOTE (P2-1 fourth-round regression): 3823b7c classified C0's
    // valid ceiling purge as invalid-snapshot and permanently poisoned the
    // controller. The purge must project locked while authority is denied,
    // then a distinct restart-required unavailable state for this exhausted
    // owner after unlock.
    const HistoryCounters counters {
        std::numeric_limits<quint32>::max(), 1, 9
    };
    ClipboardHistoryModel model(HistoryLimits {}, counters);
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue value;
    value.formats = { { QStringLiteral("text/plain"), "ceiling-secret" } };
    const auto admitted = model.admit(
        value, counters.generation, QStringLiteral("App"), 100);
    QVERIFY(admitted.accepted());
    QCOMPARE(model.revision(), quint64(10));

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    adapter.setLocked(true);
    QCOMPARE(model.generation(), std::numeric_limits<quint32>::max());
    QCOMPARE(model.revision(), quint64(10));
    QVERIFY(model.snapshot().entries.isEmpty());
    QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
    QCOMPARE(controller.entryCount(), 0);

    adapter.setLocked(false);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.phaseReasonText(),
             QStringLiteral("Clipboard service unavailable: lineage-exhausted-restart-required"));
    QVERIFY(!controller.phaseReasonText().contains(QLatin1String("invalid-snapshot")));

    const auto afterPurge = model.admit(
        value, counters.generation, QStringLiteral("App"), 101);
    QCOMPARE(afterPurge.error, ClipboardError::LineageExhausted);
}

void TstClipboardAppletSeam::testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase()
{
    // AGENT-NOTE (P2-1 fifth-round regression): 28308f0 let the terminal
    // exhaustion latch outrank C0's independent privacy-authority bit. The
    // registered privacy-denied phase must remain visible until that authority
    // returns; only then does restart-required exhaustion become actionable.
    const HistoryCounters counters {
        std::numeric_limits<quint32>::max(), 1, 9
    };
    ClipboardHistoryModel model(HistoryLimits {}, counters);
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue value;
    value.formats = { { QStringLiteral("text/plain"), "host-denial-secret" } };
    const auto admitted = model.admit(
        value, counters.generation, QStringLiteral("App"), 100);
    QVERIFY(admitted.accepted());

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    adapter.setHostPrivacyDenied(true);
    QCOMPARE(adapter.isLocked(), false);
    QCOMPARE(model.generation(), std::numeric_limits<quint32>::max());
    QCOMPARE(model.revision(), quint64(10));
    QVERIFY(model.snapshot().entries.isEmpty());
    QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
    QCOMPARE(controller.phaseReasonText(),
             QStringLiteral("Clipboard history is withheld by privacy policy."));
    QCOMPARE(controller.entryCount(), 0);

    adapter.setHostPrivacyDenied(false);
    QCOMPARE(model.privacyState(), PrivacyState::Allowed);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.phaseReasonText(),
             QStringLiteral("Clipboard service unavailable: lineage-exhausted-restart-required"));
}

void TstClipboardAppletSeam::testIndependentHostDenialSurvivesUnlock()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);

    // The host denies privacy independently of the lock; the lock must not
    // claim that denial, so unlock must not silently restore authority.
    model.setPrivacyAllowed(false);
    QCOMPARE(model.generation(), 2u);

    adapter.setLocked(true);
    QCOMPARE(model.generation(), 2u); // already denied: no second purge
    QCOMPARE(model.privacyState(), PrivacyState::Denied);

    adapter.setLocked(false);
    QCOMPARE(model.privacyState(), PrivacyState::Denied); // host denial survives
    QCOMPARE(adapter.isLocked(), false);
}

void TstClipboardAppletSeam::testOverlappingHostDenialDuringLockSurvivesUnlock()
{
    // AGENT-GUARD (P1 regression): the adapter used to record only a Boolean
    // "denied by lock", so a host denial arriving WHILE the lock denial was
    // active was indistinguishable and unlock silently re-granted privacy.
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);

    // Lock first: the adapter itself denies privacy for the lock.
    adapter.setLocked(true);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);

    // An independent host denial arrives WHILE the lock denial is active...
    adapter.setHostPrivacyDenied(true);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);

    // ...so unlock must not restore authority: the host denial still stands.
    adapter.setLocked(false);
    QCOMPARE(adapter.isLocked(), false);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);

    // Only when the host re-allows does the adapter restore exactly the
    // authority it removed.
    adapter.setHostPrivacyDenied(false);
    QCOMPARE(model.privacyState(), PrivacyState::Allowed);

    // A host re-allow while locked never bypasses the lock either.
    adapter.setHostPrivacyDenied(true);
    adapter.setLocked(true);
    adapter.setHostPrivacyDenied(false);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);
    adapter.setLocked(false);
    QCOMPARE(model.privacyState(), PrivacyState::Allowed);
}

void TstClipboardAppletSeam::testSearchThroughSeam()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val1;
    val1.formats = { { QStringLiteral("text/plain"), "Target substring" } };
    const auto admitRes = model.admit(val1, 1, QStringLiteral("Editor"), 100);
    QVERIFY(admitRes.accepted());

    ClipboardModelClientAdapter adapter(&model);

    bool searchReceived = false;
    SearchOutcome receivedOutcome;
    connect(&adapter, &ClipboardClientInterface::searchCompleted,
            [&](quint64, const SearchOutcome &outcome) {
                searchReceived = true;
                receivedOutcome = outcome;
            });

    const quint64 reqId = adapter.requestSearch(QStringLiteral("Target"), 1, 10);
    QVERIFY(reqId > 0);
    QVERIFY(searchReceived);
    QVERIFY(receivedOutcome.accepted());
    QCOMPARE(receivedOutcome.matches.size(), 1);
    QCOMPARE(receivedOutcome.matches.first().preview, QStringLiteral("Target substring"));
}

QTEST_MAIN(TstClipboardAppletSeam)
#include "tst_clipboard_applet_seam.moc"
