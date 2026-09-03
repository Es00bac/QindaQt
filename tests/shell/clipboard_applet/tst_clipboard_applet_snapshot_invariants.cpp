// SPDX-License-Identifier: GPL-3.0-or-later

// Whole-snapshot C0 invariant checks for the Clipboard applet admission gate.
// Search attribution has its own fencing suite; descriptor-shape hostility has
// the existing admission suite. This file owns contradictions that can only be
// recognized from a complete snapshot.

#include <QtTest/QtTest>

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h>

#include "clipboard_applet_test_fakes.h"

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::ShellClipboardApplet::Tests;
using namespace QindaQt::Services::ClipboardModel;

namespace {

HistorySnapshot snapshotWithEntry(quint32 generation, quint64 revision)
{
    HistorySnapshot snapshot;
    snapshot.generation = generation;
    snapshot.revision = revision;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;
    const auto entry = floorValidDescriptor(generation, 1, QStringLiteral("secret"));
    snapshot.entries = {entry};
    snapshot.totalPayloadBytes = entry.formats.first().payloadBytes;
    return snapshot;
}

class SnapshotFakeClient final : public ClipboardClientInterface {
public:
    HistorySnapshot current = snapshotWithEntry(7, 1);

    [[nodiscard]] ClientState clientState() const noexcept override { return ClientState::Ready; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return QStringLiteral("snapshot-owner"); }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return true; }
    [[nodiscard]] bool isLocked() const noexcept override { return false; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return current; }

    quint64 requestPromote(EntryId, quint32, quint64) override { return 1; }
    quint64 requestRemove(EntryId, quint32) override { return 2; }
    quint64 requestSetPinned(EntryId, bool, quint32) override { return 3; }
    quint64 requestClear(ClearScope, quint32) override { return 4; }
    quint64 requestSearch(const QString &, quint32, int) override { return 5; }

    void publish(const HistorySnapshot &snapshot)
    {
        current = snapshot;
        Q_EMIT snapshotChanged(snapshot);
    }
};

} // namespace

class TstClipboardAppletSnapshotInvariants : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testImpossibleC0SnapshotsAreRejectedWhole();
    void testPrivacyDeniedContentCannotArmSameLineageReallow();
};

void TstClipboardAppletSnapshotInvariants::testImpossibleC0SnapshotsAreRejectedWhole()
{
    // AGENT-NOTE (P1-2): e3e2dba validated descriptors and only the numeric
    // aggregate ceiling. These impossible whole-snapshot combinations were
    // accepted as C0 truth and could arm later presentation.
    HistorySnapshot zeroGeneration;
    zeroGeneration.historyEnabled = true;
    zeroGeneration.privacyAllowed = true;
    QCOMPARE(assessSnapshot(zeroGeneration), SnapshotGateDecision::RejectZeroGeneration);

    const HistorySnapshot valid = snapshotWithEntry(7, 1);

    HistorySnapshot privacyContradiction = valid;
    privacyContradiction.privacyAllowed = false;
    QCOMPARE(assessSnapshot(privacyContradiction),
             SnapshotGateDecision::RejectAuthorityContent);

    HistorySnapshot capabilityContradiction = valid;
    capabilityContradiction.historyEnabled = false;
    QCOMPARE(assessSnapshot(capabilityContradiction),
             SnapshotGateDecision::RejectAuthorityContent);

    HistorySnapshot aggregateMismatch = valid;
    aggregateMismatch.totalPayloadBytes -= 1;
    QCOMPARE(assessSnapshot(aggregateMismatch),
             SnapshotGateDecision::RejectAggregateMismatch);

    HistorySnapshot duplicateIdentity = valid;
    duplicateIdentity.entries.append(duplicateIdentity.entries.first());
    duplicateIdentity.totalPayloadBytes *= 2;
    QCOMPARE(assessSnapshot(duplicateIdentity),
             SnapshotGateDecision::RejectDuplicateEntry);

    HistorySnapshot tooManyPins;
    tooManyPins.generation = 9;
    tooManyPins.revision = 1;
    tooManyPins.historyEnabled = true;
    tooManyPins.privacyAllowed = true;
    for (quint32 serial = 1; serial <= quint32(kMaxPinnedEntries + 1); ++serial) {
        auto entry = floorValidDescriptor(9, serial, QStringLiteral("pin"));
        entry.pinned = true;
        tooManyPins.totalPayloadBytes += entry.formats.first().payloadBytes;
        tooManyPins.entries.append(entry);
    }
    QCOMPARE(assessSnapshot(tooManyPins), SnapshotGateDecision::RejectPinnedBound);
}

void TstClipboardAppletSnapshotInvariants::testPrivacyDeniedContentCannotArmSameLineageReallow()
{
    // AGENT-NOTE (P1-2): e3e2dba retained a denied-with-content snapshot and
    // disclosed it when only privacyAllowed flipped at the same lineage.
    SnapshotFakeClient client;
    ClipboardAppletController controller(&client, true, true);
    QCOMPARE(controller.entryCount(), 1);

    HistorySnapshot denied = snapshotWithEntry(7, 2);
    denied.privacyAllowed = false;
    client.publish(denied);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.entryCount(), 0);

    HistorySnapshot sameLineageReallow = denied;
    sameLineageReallow.privacyAllowed = true;
    client.publish(sameLineageReallow);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.entryCount(), 0);

    // A higher revision cannot repair a missing privacy purge: C0 advances
    // generation when authority is withdrawn, even when the purged list is
    // already empty.
    HistorySnapshot sameGenerationReallow = snapshotWithEntry(7, 3);
    client.publish(sameGenerationReallow);
    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.entryCount(), 0);

    HistorySnapshot purged;
    purged.generation = 8;
    purged.revision = 0;
    purged.historyEnabled = true;
    purged.privacyAllowed = false;
    client.publish(purged);
    QCOMPARE(controller.phaseText(), QStringLiteral("locked"));
    QCOMPARE(controller.entryCount(), 0);

    SnapshotFakeClient emptyDenialClient;
    ClipboardAppletController emptyDenialController(&emptyDenialClient, true, true);
    HistorySnapshot emptyDenial;
    emptyDenial.generation = 7;
    emptyDenial.revision = 2;
    emptyDenial.historyEnabled = true;
    emptyDenial.privacyAllowed = false;
    emptyDenialClient.publish(emptyDenial);
    QCOMPARE(emptyDenialController.phaseText(), QStringLiteral("unavailable"));

    emptyDenialClient.publish(snapshotWithEntry(7, 3));
    QCOMPARE(emptyDenialController.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(emptyDenialController.entryCount(), 0);
}

QTEST_MAIN(TstClipboardAppletSnapshotInvariants)
#include "tst_clipboard_applet_snapshot_invariants.moc"
