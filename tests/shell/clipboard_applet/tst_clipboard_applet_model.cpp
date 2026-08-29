// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_model.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

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
    void testHostileAndCorruptedDescriptors();
    void testHostileLineageAndExhaustion();
    void testHostileUnicodeAndControlChars();
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
        ClipboardEntryDescriptor desc;
        desc.id = { 1, i };
        desc.preview = QString::asprintf("Item %u", i);
        desc.pinned = (i <= 3);
        desc.formats = { { QStringLiteral("text/plain"), 10 } };
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
        ClipboardEntryDescriptor desc;
        desc.id = { 4, serial };
        desc.preview = preview;
        desc.pinned = pinned;
        desc.formats = { { QStringLiteral("text/plain"), 8 } };
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
    ClipboardEntryDescriptor match;
    match.id = { 1, 42 };
    match.preview = QStringLiteral("Match result");
    match.formats = { { QStringLiteral("text/plain"), 12 } };
    searchResults.append(match);

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

void TstClipboardAppletModel::testHostileAndCorruptedDescriptors()
{
    ClipboardEntryDescriptor corrupted;
    corrupted.id = { 0, 0 }; // Invalid id
    corrupted.formats = { { QStringLiteral("corrupted/format"), -500 } }; // Negative bytes

    const ClipboardEntryRow row = ClipboardAppletModel::projectRow(0, corrupted, false);
    QCOMPARE(row.totalBytes, 0); // Negative bytes clamped to 0
    QCOMPARE(row.idString, QStringLiteral("0:0"));
    QCOMPARE(row.formatsSummary, QStringLiteral("corrupted/format (0 B)"));
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

void TstClipboardAppletModel::testHostileUnicodeAndControlChars()
{
    ClipboardEntryDescriptor hostile;
    hostile.id = { 1, 1 };
    // Previews with control characters, tabs, newlines, and unicode bidirectionals
    hostile.preview = QStringLiteral("Line 1\nLine 2\t\x1b[31mRed\x1b[0m\u202Ereversed");
    hostile.sourceLabel = QStringLiteral("App\u0000Hidden");
    hostile.formats = { { QStringLiteral("text/plain"), 64 } };

    const ClipboardEntryRow row = ClipboardAppletModel::projectRow(0, hostile, false);
    QVERIFY(!row.preview.isEmpty());
    QVERIFY(!row.accessibleName.isEmpty());
    QVERIFY(!row.accessibleDescription.isEmpty());
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
