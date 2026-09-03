// SPDX-License-Identifier: LGPL-3.0-or-later

// Hostile-snapshot admission rows for the Clipboard applet controller: the
// descriptor-floor/collection-bound gate, (generation, revision) monotonicity,
// owner lineage fencing, fail-closed rejection presentation, mismatched
// completion rejection, and promote-tick exhaustion. Reply-attribution
// fencing lives in tst_clipboard_applet_fencing.cpp; lifecycle and capability
// gating in tst_clipboard_applet_controller.cpp.

#include <QtTest/QtTest>
#include <limits>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

namespace {

// Scripted seam with attacker-controlled snapshot/state/owner delivery. Every
// fixture it serves is floor-valid unless a test deliberately corrupts it, so
// a rejection always proves the specific fence under test, not the floor.
class AdmissionFakeClient final : public ClipboardClientInterface {
public:
    ClientState state = ClientState::Ready;
    QString ownerName = QStringLiteral("owner-A");
    bool ownerAvailable = true;
    bool locked = false;
    HistorySnapshot current;
    quint64 nextRequestId = 1;
    quint64 lastRequestId = 0;
    quint64 lastPromoteTick = 0;
    int promoteDispatches = 0;

    [[nodiscard]] ClientState clientState() const noexcept override { return state; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return ownerName; }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return ownerAvailable; }
    [[nodiscard]] bool isLocked() const noexcept override { return locked; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return current; }

    quint64 requestPromote(EntryId, quint32, quint64 tick) override
    {
        ++promoteDispatches;
        lastPromoteTick = tick;
        return lastRequestId = nextRequestId++;
    }
    quint64 requestRemove(EntryId, quint32) override { return lastRequestId = nextRequestId++; }
    quint64 requestSetPinned(EntryId, bool, quint32) override
    {
        return lastRequestId = nextRequestId++;
    }
    quint64 requestClear(ClearScope, quint32) override { return lastRequestId = nextRequestId++; }
    quint64 requestSearch(const QString &, quint32, int) override
    {
        return lastRequestId = nextRequestId++;
    }

    void publishSnapshot(const HistorySnapshot &snapshot)
    {
        current = snapshot;
        Q_EMIT snapshotChanged(snapshot);
    }

    void publishState(ClientState nextState, bool available, const QString &nextOwner)
    {
        state = nextState;
        ownerAvailable = available;
        ownerName = nextOwner;
        Q_EMIT stateChanged(state, reasonCode());
    }

    void complete(quint64 requestId, const OperationOutcome &outcome)
    {
        Q_EMIT operationCompleted(requestId, outcome);
    }
};

// A floor-valid single-entry snapshot: 32-byte fingerprint, canonical storable
// media, sanitized bounded label/preview, generation-tagged identity.
HistorySnapshot floorValidSnapshot(quint32 generation, quint32 serial, const QString &preview,
                                   quint64 revision = 1)
{
    HistorySnapshot snapshot;
    snapshot.generation = generation;
    snapshot.revision = revision;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;
    snapshot.totalPayloadBytes = 1;
    ClipboardEntryDescriptor descriptor;
    descriptor.id = { generation, serial };
    descriptor.preview = preview;
    descriptor.sourceLabel = QStringLiteral("source");
    descriptor.formats = { { QStringLiteral("text/plain"), 1 } };
    descriptor.fingerprint = QByteArray(32, 'a');
    snapshot.entries = { descriptor };
    return snapshot;
}

} // namespace

class TstClipboardAppletAdmission : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testStaleGenerationSnapshotIsRejectedWhole();
    void testStaleRevisionWithinGenerationIsRejected();
    void testHostileMetadataIsRejectedWhole();
    void testForgedMediaClassesAreRejected();
    void testOversizedCollectionIsRejectedWhole();
    void testAggregateByteClaimIsRejected();
    void testEntryLineageMismatchIsRejected();
    void testRejectedStateRecoversOnFreshValidSnapshot();
    void testOwnerReplacementNeverRedisclosesOwnerAContent();
    void testFreshOwnerBaselineMustBeContentEmpty();
    void testMismatchedCompletionIsRejectedAndMarkerStays();
    void testPromoteTickExhaustionFailsClosed();
};

void TstClipboardAppletAdmission::testStaleGenerationSnapshotIsRejectedWhole()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current-generation"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    // A snapshot from a generation below the accepted high-water is stale or
    // replayed: rejected whole — nothing of it may be presented, not even a
    // sanitized fragment.
    client.publishSnapshot(floorValidSnapshot(4, 1, QStringLiteral("stale-generation-secret")));
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.entryRows().isEmpty());
    QVERIFY(controller.projection().entryRows.isEmpty());
    QCOMPARE(controller.unpinnedCount(), 0);
    const QString stalePreview =
        controller.entryRows().value(0).value<ClipboardEntryRow>().preview;
    QVERIFY(!stalePreview.contains(QLatin1String("stale")));

    // Rejection fails closed into the registered unavailable reason until a
    // fresh valid snapshot arrives.
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.phaseReasonText(),
             QStringLiteral("Clipboard service unavailable: invalid-snapshot"));
}

void TstClipboardAppletAdmission::testStaleRevisionWithinGenerationIsRejected()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(6, 1, QStringLiteral("current"), 4);
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    // Same generation, regressed revision: out-of-order or replayed.
    client.publishSnapshot(floorValidSnapshot(6, 1, QStringLiteral("older-revision-secret"), 3));
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));

    // Re-stating the accepted (generation, revision) is an idempotent
    // re-delivery and stays acceptable.
    client.publishSnapshot(floorValidSnapshot(6, 1, QStringLiteral("current"), 4));
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);
}

void TstClipboardAppletAdmission::testHostileMetadataIsRejectedWhole()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    // Overlong label with a newline and an unpaired surrogate, overlong
    // preview with a bidi format character: every one of these fails the C0
    // descriptor floor, so the snapshot is refused rather than sanitized.
    HistorySnapshot hostile = floorValidSnapshot(6, 1, QString());
    ClipboardEntryDescriptor &entry = hostile.entries.first();
    entry.sourceLabel = QString(4096, QLatin1Char('L')) + QLatin1Char('\n')
        + QChar(0xd800);
    entry.preview = QString(4096, QLatin1Char('P')) + QChar(0x202e);
    client.publishSnapshot(hostile);
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.projection().entryRows.isEmpty());
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));

    // A forged truncation flag on an empty preview fails the floor too.
    HistorySnapshot forgedTruncation = floorValidSnapshot(7, 1, QString());
    forgedTruncation.entries.first().preview.clear();
    forgedTruncation.entries.first().previewTruncated = true;
    client.publishSnapshot(forgedTruncation);
    QCOMPARE(controller.entryCount(), 0);
}

void TstClipboardAppletAdmission::testForgedMediaClassesAreRejected()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    struct Row {
        const char *mediaType;
    };
    const Row rows[] = {
        { "application/x-qindaqt-secret" },
        { "x-kde-passwordmanagerhint" },
        { "application/x-qindaqt-one-time" },
        { "application/x-qindaqt-unknown-future-type" },
    };
    for (const auto &row : rows) {
        HistorySnapshot forged = floorValidSnapshot(6, 1, QStringLiteral("forged media"));
        forged.entries.first().formats.first().mediaType =
            QString::fromLatin1(row.mediaType);
        client.publishSnapshot(forged);
        QCOMPARE(controller.entryCount(), 0);
        QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    }
}

void TstClipboardAppletAdmission::testOversizedCollectionIsRejectedWhole()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    HistorySnapshot oversized;
    oversized.generation = 7;
    oversized.revision = 1;
    oversized.historyEnabled = true;
    oversized.privacyAllowed = true;
    oversized.totalPayloadBytes = 5000;
    for (quint32 serial = 1; serial <= 5000; ++serial) {
        ClipboardEntryDescriptor descriptor = floorValidSnapshot(7, 1, QString()).entries.first();
        descriptor.id.serial = serial;
        oversized.entries.append(descriptor);
    }
    client.publishSnapshot(oversized);
    // The C0 kMaxEntries bound (64) refuses the collection before the 32-row
    // presentation cap is even consulted; no count may leak the 5000 claims.
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.unpinnedCount(), 0);
    QCOMPARE(controller.totalPayloadBytes(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
}

void TstClipboardAppletAdmission::testAggregateByteClaimIsRejected()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    HistorySnapshot inflated = floorValidSnapshot(6, 1, QStringLiteral("inflated"));
    inflated.totalPayloadBytes = kMaxTotalPayloadBytes + 1;
    client.publishSnapshot(inflated);
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.totalPayloadBytes(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
}

void TstClipboardAppletAdmission::testEntryLineageMismatchIsRejected()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    // The C0 model purges on generation change, so a snapshot can never
    // legitimately mix entry generations: an entry id from an older
    // generation is stale lineage.
    HistorySnapshot mixed = floorValidSnapshot(6, 1, QStringLiteral("fresh lineage"));
    ClipboardEntryDescriptor stale = mixed.entries.first();
    stale.id = { 4, 9 };
    mixed.entries.append(stale);
    client.publishSnapshot(mixed);
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
}

void TstClipboardAppletAdmission::testRejectedStateRecoversOnFreshValidSnapshot()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(5, 1, QStringLiteral("current"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    // Violation drops all presentation and pending bookkeeping...
    client.publishSnapshot(floorValidSnapshot(4, 1, QStringLiteral("stale")));
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.pendingOperationCount(), 0);

    // ...but a fresh valid snapshot above the surviving high-water fence is
    // accepted again: rejection is per-snapshot, not a permanent latch.
    client.publishSnapshot(floorValidSnapshot(6, 1, QStringLiteral("recovered")));
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("recovered"));
}

void TstClipboardAppletAdmission::testOwnerReplacementNeverRedisclosesOwnerAContent()
{
    AdmissionFakeClient client;
    // Owner A presents real content first — the fencing target must start
    // with owner-A content, not an empty history.
    client.current = floorValidSnapshot(5, 2, QStringLiteral("owner-A-secret"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 1);

    // Owner lost: the accepted baseline is voided immediately.
    client.publishState(ClientState::Unavailable, false, QStringLiteral("owner-A"));
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.entryRows().isEmpty());

    // Owner B ready without any snapshot under B: the applet waits for B's
    // initial snapshot (loading), and owner-A content must not reappear.
    client.publishState(ClientState::Ready, true, QStringLiteral("owner-B"));
    QVERIFY(controller.phaseText() != QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.entryRows().isEmpty());

    // The seam replaying owner-A's content as the new owner's first snapshot
    // is foreign content: volatile history starts empty per owner.
    client.publishSnapshot(floorValidSnapshot(5, 2, QStringLiteral("owner-A-secret")));
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));

    // B's genuine fresh baseline is empty and becomes presentable...
    client.publishState(ClientState::Ready, true, QStringLiteral("owner-B"));
    HistorySnapshot bBaseline;
    bBaseline.generation = 1;
    bBaseline.revision = 0;
    bBaseline.historyEnabled = true;
    bBaseline.privacyAllowed = true;
    client.publishSnapshot(bBaseline);
    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.entryCount(), 0);

    // ...and B's own content then arrives through monotonic snapshots.
    client.publishSnapshot(floorValidSnapshot(1, 1, QStringLiteral("owner-B-own"), 1));
    QCOMPARE(controller.entryCount(), 1);
    QCOMPARE(controller.projection().entryRows.first().preview, QStringLiteral("owner-B-own"));

    // A stale owner-A replay below B's established lineage is refused whole:
    // the surface fails closed and presents nothing rather than the replayed
    // foreign content or a stale fragment of B's history.
    client.publishSnapshot(floorValidSnapshot(1, 1, QStringLiteral("owner-A-secret"), 0));
    QCOMPARE(controller.entryCount(), 0);
    QVERIFY(controller.projection().entryRows.isEmpty());
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
}

void TstClipboardAppletAdmission::testFreshOwnerBaselineMustBeContentEmpty()
{
    AdmissionFakeClient client;
    client.current = floorValidSnapshot(3, 1, QStringLiteral("owner-A-secret"));
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    // Direct replacement without an intermediate loss event: the baseline is
    // voided on the owner change itself, at the latest when the next snapshot
    // is attributed to the new owner.
    client.publishState(ClientState::Ready, true, QStringLiteral("owner-B"));
    client.publishSnapshot(floorValidSnapshot(3, 1, QStringLiteral("owner-A-secret")));
    QCOMPARE(controller.entryCount(), 0);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QVERIFY(controller.projection().entryRows.isEmpty());
}

void TstClipboardAppletAdmission::testMismatchedCompletionIsRejectedAndMarkerStays()
{
    // AGENT-GUARD (P2 regression): a completion whose entry id disagrees with
    // the request's recorded lineage used to erase the request but clear the
    // marker of the UNTRUSTED id — stranding the initiating entry's pending
    // marker forever. It must be rejected whole: pending record and marker
    // stay, so only the genuine completion (or a retry after it) resolves it.
    AdmissionFakeClient client;
    HistorySnapshot snapshot = floorValidSnapshot(8, 1, QStringLiteral("entry-A"));
    ClipboardEntryDescriptor second = snapshot.entries.first();
    second.id.serial = 2;
    second.preview = QStringLiteral("entry-B");
    snapshot.entries.append(second);
    client.current = snapshot;
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 2);

    QVERIFY(controller.selectEntry(8, 1));
    QCOMPARE(controller.pendingOperationCount(), 1);

    OperationOutcome mismatched;
    mismatched.code = OperationErrorCode::None;
    mismatched.id = { 8, 2 }; // foreign entry lineage
    client.complete(client.lastRequestId, mismatched);

    // Rejected whole: the request is still pending and entry (8,1) is still
    // in flight, so a duplicate intent is refused rather than stranded.
    QCOMPARE(controller.pendingOperationCount(), 1);
    QVERIFY(!controller.selectEntry(8, 1));
    QVERIFY(!controller.feedbackPresent());

    // The genuine completion resolves the stored request's marker.
    OperationOutcome genuine;
    genuine.code = OperationErrorCode::None;
    genuine.id = { 8, 1 };
    client.complete(client.lastRequestId, genuine);
    QCOMPARE(controller.pendingOperationCount(), 0);
    QVERIFY(controller.selectEntry(8, 1));
}

void TstClipboardAppletAdmission::testPromoteTickExhaustionFailsClosed()
{
    // AGENT-GUARD (P2 regression): a hostile snapshot claiming the maximum
    // fixed-width tick used to wrap the next issued promote tick to zero — a
    // non-monotonic tick the model trusts for recency. Exhaustion must fail
    // closed: the promote is refused with feedback and nothing is dispatched.
    AdmissionFakeClient client;
    HistorySnapshot snapshot = floorValidSnapshot(9, 1, QStringLiteral("max-tick"));
    snapshot.entries.first().lastUsedTick = std::numeric_limits<quint64>::max();
    client.current = snapshot;
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    QVERIFY(!controller.selectEntry(9, 1));
    QCOMPARE(client.promoteDispatches, 0);
    QCOMPARE(client.lastPromoteTick, quint64(0));
    QCOMPARE(controller.pendingOperationCount(), 0);
    QVERIFY(controller.feedbackPresent());
    QCOMPARE(controller.feedbackStatus(), QStringLiteral("error"));
}

QTEST_MAIN(TstClipboardAppletAdmission)
#include "tst_clipboard_applet_admission.moc"
