// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_model.h>
#include <qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

namespace {

// Floor-valid descriptor baseline (valid identity, canonical storable media,
// exact 32-byte fingerprint, sanitized bounded metadata). Hostile rows below
// corrupt exactly one field from this baseline so a rejection proves the
// specific floor rule, never an accidental second violation.
ClipboardEntryDescriptor floorValid(quint32 generation, quint32 serial,
                                    const QString &preview)
{
    ClipboardEntryDescriptor desc;
    desc.id = { generation, serial };
    desc.preview = preview;
    desc.sourceLabel = QStringLiteral("source");
    desc.formats = { { QStringLiteral("text/plain"), qint64(qMax(1, preview.size())) } };
    desc.fingerprint = QByteArray(32, 'a');
    return desc;
}

} // namespace

class TstClipboardAppletModel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testPhaseTransitions();
    void testFormatByteSize();
    void testFormatSummary();
    void testAccessibleName();
    void testRowProjection();
    void testPresentationBounds();
    void testPinnedFirstPartitionOrdering();
    void testSearchProjection();
    void testEmptyStates();
    void testHostileDescriptorFloorRejectsWholeSnapshot();
    void testHostileCollectionAndAggregateBoundsRejectWholeSnapshot();
    void testHostileSearchMatchesRejectProjection();
    void testHostileLineageAndExhaustion();
    void testHostileFormatCombinations();
};

void TstClipboardAppletModel::testPhaseTransitions()
{
    HistorySnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;

    // 1. Ready state
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Ready);
        QVERIFY(proj.phaseReasonText.isEmpty());
    }

    // 2. Unavailable when owner lost
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Ready, {}, false, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Unavailable);
        QVERIFY(proj.entryRows.isEmpty());
    }

    // 3. Unavailable when client state is Unavailable
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Unavailable, QStringLiteral("daemon-exited"), true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Unavailable);
        QVERIFY(proj.phaseReasonText.contains(QLatin1String("daemon-exited")));
    }

    // 4. Loading when client starting/stopped
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Starting, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Loading);
    }

    // 5. Locked when session is locked (registered reason names the lock)
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Ready, {}, true, true, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Locked);
        QVERIFY(proj.entryRows.isEmpty());
        QVERIFY(proj.phaseReasonText.contains(QLatin1String("locked")));
    }

    // 5a. Same withheld phase for an authority denial, but the registered
    // reason must distinguish privacy from a session lock.
    {
        HistorySnapshot deniedSnap = snapshot;
        deniedSnap.privacyAllowed = false;
        const auto proj = ClipboardAppletModel::project(
            deniedSnap, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Locked);
        QVERIFY(proj.entryRows.isEmpty());
        QVERIFY(proj.phaseReasonText.contains(QLatin1String("privacy")));
        QVERIFY(!proj.phaseReasonText.contains(QLatin1String("locked")));
    }

    // 6. Locked when privacy is denied in snapshot
    {
        HistorySnapshot lockedSnap = snapshot;
        lockedSnap.privacyAllowed = false;
        const auto proj = ClipboardAppletModel::project(
            lockedSnap, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Locked);
        QVERIFY(proj.entryRows.isEmpty());
    }

    // 7. Disabled when history is disabled
    {
        HistorySnapshot disabledSnap = snapshot;
        disabledSnap.historyEnabled = false;
        const auto proj = ClipboardAppletModel::project(
            disabledSnap, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Disabled);
        QVERIFY(proj.entryRows.isEmpty());
    }

    // 8. Degraded
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Degraded, QStringLiteral("pipe-overflow"), true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Degraded);
        QVERIFY(proj.phaseReasonText.contains(QLatin1String("pipe-overflow")));
    }
}

void TstClipboardAppletModel::testFormatByteSize()
{
    QCOMPARE(ClipboardAppletModel::formatByteSize(-10), QStringLiteral("0 B"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(0), QStringLiteral("0 B"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(512), QStringLiteral("512 B"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(1024), QStringLiteral("1.0 KB"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(1536), QStringLiteral("1.5 KB"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(1024 * 1024), QStringLiteral("1.0 MB"));
    QCOMPARE(ClipboardAppletModel::formatByteSize(5 * 1024 * 1024), QStringLiteral("5.0 MB"));
}

void TstClipboardAppletModel::testFormatSummary()
{
    QCOMPARE(ClipboardAppletModel::formatSummary({}, 0), QStringLiteral("No formats"));

    QList<FormatInfo> single = { { QStringLiteral("text/plain"), 120 } };
    QCOMPARE(ClipboardAppletModel::formatSummary(single, 120), QStringLiteral("text/plain (120 B)"));

    QList<FormatInfo> multi = {
        { QStringLiteral("text/plain"), 120 },
        { QStringLiteral("text/html"), 450 },
    };
    QCOMPARE(ClipboardAppletModel::formatSummary(multi, 570),
             QStringLiteral("text/plain (+1 formats, 570 B)"));
}

void TstClipboardAppletModel::testAccessibleName()
{
    ClipboardEntryDescriptor desc;
    desc.id = { 1, 5 };
    desc.preview = QStringLiteral("Hello world");
    desc.sourceLabel = QStringLiteral("Kate");
    desc.pinned = true;
    desc.formats = { { QStringLiteral("text/plain"), 11 } };

    const QString name = ClipboardAppletModel::accessibleNameForRow(0, desc);
    QVERIFY(name.contains(QLatin1String("Entry 1")));
    QVERIFY(name.contains(QLatin1String("text/plain")));
    QVERIFY(name.contains(QLatin1String("pinned")));
    QVERIFY(name.contains(QLatin1String("from Kate")));
    QVERIFY(name.contains(QLatin1String("Hello world")));

    // P2 regression: in-flight mutations must be announced to assistive
    // technology, mirroring the busy presentation of the action buttons.
    QVERIFY(!name.contains(QLatin1String("pending")));
    const QString pendingName = ClipboardAppletModel::accessibleNameForRow(0, desc, true);
    QVERIFY(pendingName.contains(QLatin1String("operation pending")));

    // The projection carries the flag into the projected row's name.
    const ClipboardEntryRow pendingRow = ClipboardAppletModel::projectRow(0, desc, true);
    QVERIFY(pendingRow.accessibleName.contains(QLatin1String("operation pending")));
}

void TstClipboardAppletModel::testRowProjection()
{
    ClipboardEntryDescriptor desc;
    desc.id = { 2, 10 };
    desc.preview = QStringLiteral("https://qindaqt.org");
    desc.previewTruncated = false;
    desc.sourceLabel = QStringLiteral("Browser");
    desc.pinned = false;
    desc.admittedTick = 1000;
    desc.lastUsedTick = 1050;
    desc.formats = { { QStringLiteral("text/uri-list"), 21 } };

    const ClipboardEntryRow row = ClipboardAppletModel::projectRow(0, desc, true);
    QCOMPARE(row.generation, 2u);
    QCOMPARE(row.serial, 10u);
    QCOMPARE(row.idString, QStringLiteral("2:10"));
    QCOMPARE(row.preview, QStringLiteral("https://qindaqt.org"));
    QCOMPARE(row.sourceLabel, QStringLiteral("Browser"));
    QCOMPARE(row.pinned, false);
    QCOMPARE(row.isUriList, true);
    QCOMPARE(row.isText, false);
    QCOMPARE(row.isImage, false);
    QCOMPARE(row.pending, true);
    QCOMPARE(row.totalBytes, 21);

    // Image classification check
    ClipboardEntryDescriptor imgDesc;
    imgDesc.id = { 2, 11 };
    imgDesc.formats = { { QStringLiteral("image/png"), 4096 } };
    const ClipboardEntryRow imgRow = ClipboardAppletModel::projectRow(1, imgDesc, false);
    QCOMPARE(imgRow.isImage, true);
    QCOMPARE(imgRow.isText, false);
}

void TstClipboardAppletModel::testPresentationBounds()
{
    HistorySnapshot snapshot;
    snapshot.generation = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;

    // Populate with 50 entries (exceeding kMaxPresentedEntries = 32)
    for (quint32 i = 1; i <= 50; ++i) {
        auto desc = floorValid(1, i, QString::asprintf("Item %u", i));
        desc.pinned = (i <= 3);
        snapshot.entries.append(desc);
    }
    snapshot.totalPayloadBytes = 50 * 10;

    const auto proj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, false, {}, {}, false, {});

    QCOMPARE(proj.phase, Phase::Ready);
    QCOMPARE(proj.pinnedCount, 3);
    QCOMPARE(proj.unpinnedCount, 47);
    QCOMPARE(proj.entryRows.size(), kMaxPresentedEntries);
    QCOMPARE(proj.entryRows.first().serial, 1u);
}

void TstClipboardAppletModel::testPinnedFirstPartitionOrdering()
{
    HistorySnapshot snapshot;
    snapshot.generation = 4;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;

    // MRU order as stored: index 0 is most recent. Pinned entries sit at
    // unsorted positions so raw MRU and the documented partition disagree.
    auto addEntry = [&snapshot](quint32 serial, bool pinned, const QString &preview) {
        auto desc = floorValid(4, serial, preview);
        desc.pinned = pinned;
        snapshot.entries.append(desc);
    };
    addEntry(1, false, QStringLiteral("most recent unpinned"));
    addEntry(2, true, QStringLiteral("second item, pinned"));
    addEntry(3, false, QStringLiteral("third item"));
    addEntry(4, true, QStringLiteral("oldest item, pinned"));
    snapshot.totalPayloadBytes = 32;

    const auto proj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, false, {}, {}, false, {});

    // AGENT-GUARD: projection order is a stable partition — all pinned first,
    // then all unpinned, each class preserving the snapshot's most-recent-
    // first order. Raw MRU (1,2,3,4) must never leak into rows.
    QCOMPARE(proj.entryRows.size(), 4);
    QCOMPARE(proj.entryRows.at(0).serial, 2u);
    QCOMPARE(proj.entryRows.at(1).serial, 4u);
    QCOMPARE(proj.entryRows.at(2).serial, 1u);
    QCOMPARE(proj.entryRows.at(3).serial, 3u);
    QCOMPARE(proj.entryRows.at(0).pinned, true);
    QCOMPARE(proj.entryRows.at(1).pinned, true);
    QCOMPARE(proj.entryRows.at(2).pinned, false);
    QCOMPARE(proj.entryRows.at(3).pinned, false);

    // The partition also governs search-result projections: matches keep the
    // model's most-recent-first reply order within each pin class.
    QList<ClipboardEntryDescriptor> matches;
    matches.append(snapshot.entries.at(0)); // unpinned, most recent match
    matches.append(snapshot.entries.at(3)); // pinned, older match
    const auto searchProj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, true, QStringLiteral("item"),
        matches, false, {});
    QCOMPARE(searchProj.entryRows.size(), 2);
    QCOMPARE(searchProj.entryRows.at(0).serial, 4u); // pinned first
    QCOMPARE(searchProj.entryRows.at(1).serial, 1u);
}

void TstClipboardAppletModel::testSearchProjection()
{
    HistorySnapshot snapshot;
    snapshot.generation = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;

    QList<ClipboardEntryDescriptor> searchResults;
    searchResults.append(floorValid(1, 42, QStringLiteral("Match result")));

    const auto proj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, true, QStringLiteral("Match"), searchResults, false, {});

    QCOMPARE(proj.phase, Phase::Ready);
    QCOMPARE(proj.isSearchActive, true);
    QCOMPARE(proj.searchQuery, QStringLiteral("Match"));
    QCOMPARE(proj.searchResultCount, 1);
    QCOMPARE(proj.entryRows.size(), 1);
    QCOMPARE(proj.entryRows.first().serial, 42u);
}

void TstClipboardAppletModel::testEmptyStates()
{
    HistorySnapshot snapshot;
    snapshot.generation = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;

    // Normal empty
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QVERIFY(proj.entryRows.isEmpty());
        QCOMPARE(proj.emptyReasonText, QStringLiteral("Clipboard history is empty."));
    }

    // Search empty
    {
        const auto proj = ClipboardAppletModel::project(
            snapshot, ClientState::Ready, {}, true, false, true, QStringLiteral("Nonexistent"), {}, false, {});
        QVERIFY(proj.entryRows.isEmpty());
        QVERIFY(proj.emptyReasonText.contains(QLatin1String("Nonexistent")));
    }
}

void TstClipboardAppletModel::testHostileDescriptorFloorRejectsWholeSnapshot()
{
    // Baseline: a floor-valid snapshot projects ready.
    HistorySnapshot snapshot;
    snapshot.generation = 9;
    snapshot.revision = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;
    snapshot.totalPayloadBytes = 5;
    snapshot.entries.append(floorValid(9, 1, QStringLiteral("clean")));
    const auto baseline = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
    QCOMPARE(baseline.phase, Phase::Ready);
    QCOMPARE(baseline.entryRows.size(), 1);

    // Every hostile variant must fail closed with the exact refusal: the gate
    // names the violated rule and the projection withholds EVERYTHING — no
    // sanitized copy, no row count, no metadata fragment.
    const auto expectRejection = [](ClipboardEntryDescriptor hostile,
                                    SnapshotGateDecision decision) {
        QCOMPARE(assessDescriptorList({ hostile }, 9), decision);
        HistorySnapshot poisoned;
        poisoned.generation = 9;
        poisoned.revision = 2;
        poisoned.historyEnabled = true;
        poisoned.privacyAllowed = true;
        poisoned.totalPayloadBytes = 5;
        poisoned.entries.append(hostile);
        const auto proj = ClipboardAppletModel::project(
            poisoned, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
        QCOMPARE(proj.phase, Phase::Unavailable);
        QVERIFY(proj.entryRows.isEmpty());
        QCOMPARE(proj.pinnedCount, 0);
        QCOMPARE(proj.unpinnedCount, 0);
        QCOMPARE(proj.phaseReasonText,
                 QStringLiteral("Clipboard history data was refused."));
    };

    // Invalid identity: serial zero is not a real entry.
    expectRejection(floorValid(9, 0, QStringLiteral("no serial")),
                    SnapshotGateDecision::RejectDescriptorFloor);

    // Negative claimed byte count.
    {
        auto hostile = floorValid(9, 2, QStringLiteral("negative bytes"));
        hostile.formats.first().payloadBytes = -500;
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Control characters, escape sequences, and bidi format characters in
    // the preview: never reprojected, not even "cleaned up".
    {
        auto hostile = floorValid(9, 3, QStringLiteral("Line 1\nLine 2\t\x1b[31mRed\x1b[0m\u202eREVERSED"));
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Embedded NUL and unpaired surrogate in the source label.
    {
        auto hostile = floorValid(9, 4, QStringLiteral("label"));
        hostile.sourceLabel = QStringLiteral("App\0Hidden") + QChar(0xd800);
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Overlong label/preview fields.
    {
        auto hostile = floorValid(9, 5, QStringLiteral("bounds"));
        hostile.sourceLabel = QString(kMaxSourceLabelCodeUnits + 1, QLatin1Char('L'));
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Forged truncation flag on an empty preview.
    {
        auto hostile = floorValid(9, 6, QString());
        hostile.preview.clear();
        hostile.previewTruncated = true;
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Wrong fingerprint width.
    {
        auto hostile = floorValid(9, 7, QStringLiteral("fingerprint"));
        hostile.fingerprint = QByteArray(31, 'a');
        expectRejection(hostile, SnapshotGateDecision::RejectDescriptorFloor);
    }

    // Forged sensitive and one-time media classes.
    {
        auto hostile = floorValid(9, 8, QStringLiteral("secret"));
        hostile.formats.first().mediaType = QStringLiteral("application/x-qindaqt-secret");
        expectRejection(hostile, SnapshotGateDecision::RejectSensitiveMedia);
    }
    {
        auto hostile = floorValid(9, 9, QStringLiteral("one shot"));
        hostile.formats.first().mediaType = QStringLiteral("x-qindaqt-one-time");
        expectRejection(hostile, SnapshotGateDecision::RejectOneTimeMedia);
    }

    // Entry lineage from another generation inside this snapshot.
    expectRejection(floorValid(8, 1, QStringLiteral("foreign lineage")),
                    SnapshotGateDecision::RejectEntryLineage);
}

void TstClipboardAppletModel::testHostileCollectionAndAggregateBoundsRejectWholeSnapshot()
{
    HistorySnapshot snapshot;
    snapshot.generation = 9;
    snapshot.revision = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;
    snapshot.totalPayloadBytes = 1;
    snapshot.entries.append(floorValid(9, 1, QStringLiteral("clean")));

    // Above the C0 kMaxEntries protocol ceiling: the collection is refused
    // whole; no count may leak the claimed size.
    HistorySnapshot oversized = snapshot;
    for (quint32 serial = 2; oversized.entries.size() <= kMaxEntries; ++serial) {
        oversized.entries.append(floorValid(9, serial, QStringLiteral("bulk")));
    }
    QCOMPARE(oversized.entries.size(), kMaxEntries + 1);
    QCOMPARE(assessSnapshot(oversized), SnapshotGateDecision::RejectCollectionBound);
    const auto proj = ClipboardAppletModel::project(
        oversized, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
    QCOMPARE(proj.phase, Phase::Unavailable);
    QVERIFY(proj.entryRows.isEmpty());
    QCOMPARE(proj.unpinnedCount, 0);

    // Aggregate byte claim above the C0 total ceiling.
    HistorySnapshot inflated = snapshot;
    inflated.totalPayloadBytes = kMaxTotalPayloadBytes + 1;
    QCOMPARE(assessSnapshot(inflated), SnapshotGateDecision::RejectAggregateBytes);
    const auto inflatedProj = ClipboardAppletModel::project(
        inflated, ClientState::Ready, {}, true, false, false, {}, {}, false, {});
    QCOMPARE(inflatedProj.phase, Phase::Unavailable);
    QVERIFY(inflatedProj.entryRows.isEmpty());
}

void TstClipboardAppletModel::testHostileSearchMatchesRejectProjection()
{
    HistorySnapshot snapshot;
    snapshot.generation = 9;
    snapshot.revision = 1;
    snapshot.historyEnabled = true;
    snapshot.privacyAllowed = true;
    snapshot.totalPayloadBytes = 5;
    snapshot.entries.append(floorValid(9, 1, QStringLiteral("clean")));

    // A hostile descriptor smuggled through a search reply is refused by the
    // same floor: the projection fails closed rather than displaying it.
    auto hostileMatch = floorValid(9, 2, QStringLiteral("evil\u202e"));
    QCOMPARE(assessDescriptorList({ hostileMatch }, 9),
             SnapshotGateDecision::RejectDescriptorFloor);
    const auto proj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, true, QStringLiteral("evil"),
        { hostileMatch }, false, {});
    QCOMPARE(proj.phase, Phase::Unavailable);
    QVERIFY(proj.entryRows.isEmpty());
    QCOMPARE(proj.searchResultCount, 0);

    // A match carrying another generation's lineage is equally refused.
    const auto foreignMatch = floorValid(3, 1, QStringLiteral("foreign"));
    QCOMPARE(assessDescriptorList({ foreignMatch }, 9),
             SnapshotGateDecision::RejectEntryLineage);
    const auto foreignProj = ClipboardAppletModel::project(
        snapshot, ClientState::Ready, {}, true, false, true, QStringLiteral("foreign"),
        { foreignMatch }, false, {});
    QCOMPARE(foreignProj.phase, Phase::Unavailable);
    QVERIFY(foreignProj.entryRows.isEmpty());
}

void TstClipboardAppletModel::testHostileLineageAndExhaustion()
{
    // Max values (lineage ceiling)
    ClipboardEntryDescriptor extreme;
    extreme.id = { std::numeric_limits<quint32>::max(), std::numeric_limits<quint32>::max() };
    extreme.formats = { { QStringLiteral("text/plain"), std::numeric_limits<qint64>::max() } };
    extreme.preview = QStringLiteral("Ceiling item");
    extreme.sourceLabel = QStringLiteral("Extreme");

    const ClipboardEntryRow row = ClipboardAppletModel::projectRow(0, extreme, false);
    QCOMPARE(row.generation, std::numeric_limits<quint32>::max());
    QCOMPARE(row.serial, std::numeric_limits<quint32>::max());
    QCOMPARE(row.idString, QStringLiteral("4294967295:4294967295"));
    QVERIFY(!row.formatsSummary.isEmpty());
    QVERIFY(!row.accessibleName.isEmpty());
    QVERIFY(!row.accessibleDescription.isEmpty());

    // Zero lineage
    ClipboardEntryDescriptor zero;
    zero.id = { 0, 0 };
    const ClipboardEntryRow zeroRow = ClipboardAppletModel::projectRow(0, zero, false);
    QCOMPARE(zeroRow.generation, 0u);
    QCOMPARE(zeroRow.serial, 0u);
    QCOMPARE(zeroRow.primaryMediaType, QStringLiteral("application/octet-stream"));
}

void TstClipboardAppletModel::testHostileFormatCombinations()
{
    // Multiple formats with duplicate types and zero bytes
    ClipboardEntryDescriptor multi;
    multi.id = { 1, 2 };
    multi.formats = {
        { QStringLiteral("text/uri-list"), 0 },
        { QStringLiteral("text/plain"), 100 },
        { QStringLiteral("image/png"), 200 }
    };

    const ClipboardEntryRow row = ClipboardAppletModel::projectRow(0, multi, false);
    QCOMPARE(row.totalBytes, 300);
    QCOMPARE(row.isUriList, true);
    QCOMPARE(row.isText, false);
    QCOMPARE(row.isImage, false);
    QVERIFY(row.formatsSummary.contains(QLatin1String("+2")));
}

QTEST_MAIN(TstClipboardAppletModel)
#include "tst_clipboard_applet_model.moc"
