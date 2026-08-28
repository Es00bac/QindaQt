// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include "support/clipboard_test_data.h"

#include <limits>

#include <QtTest>

using namespace QindaQt::Services::ClipboardModel;

// Lineage, authority, and gated-read semantics: stale-generation fencing,
// fixed-width counter exhaustion, constructor sanitization, purge-on-
// privacy-loss and disable behavior, and the gated bounded metadata search.
// Core admission/eviction/dedup semantics live in
// tst_clipboard_history.cpp.

class ClipboardHistoryLineageTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void staleGenerationIsRejectedEverywhere();
    void counterExhaustionFailsClosed();
    void constructorSanitizesWidenedLimits();
    void metadataSearchIsBoundedAndGated();
    void privacyLossPurgesAndRaisesGeneration();
    void disablePurgesAndRaisesGeneration();
};

void ClipboardHistoryLineageTests::staleGenerationIsRejectedEverywhere()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("s"), 1)
                .accepted());
    const quint32 currentGeneration = model.generation();
    const quint32 staleGeneration = currentGeneration - 1;

    QCOMPARE(model.admit(ClipboardTest::fixtureBeta(), staleGeneration,
                         QStringLiteral("s"), 2)
                 .error,
             ClipboardError::StaleGeneration);
    QCOMPARE(model.promote(model.snapshot().entries.first().id, staleGeneration, 3).error,
             ClipboardError::StaleGeneration);
    QCOMPARE(model.removeEntry(model.snapshot().entries.first().id, staleGeneration).error,
             ClipboardError::StaleGeneration);
    QCOMPARE(model.setPinned(model.snapshot().entries.first().id, true, staleGeneration)
                 .error,
             ClipboardError::StaleGeneration);
    QCOMPARE(model.clear(ClearScope::All, staleGeneration).error,
             ClipboardError::StaleGeneration);
    // None of the stale calls mutated anything.
    QCOMPARE(model.snapshot().entries.size(), 1);
    QCOMPARE(model.generation(), currentGeneration);
}

void ClipboardHistoryLineageTests::counterExhaustionFailsClosed()
{
    // Generation exhaustion: a purge at the 32-bit ceiling destroys content
    // (a privacy purge is never refused) but pins the counter and refuses
    // every further content operation instead of wrapping to zero.
    ClipboardHistoryModel genModel = ClipboardTest::enabledModel();
    const HistoryCounters maxGeneration { std::numeric_limits<quint32>::max(), 1, 0 };
    genModel = ClipboardHistoryModel(HistoryLimits {}, maxGeneration);
    genModel.setHistoryEnabled(true);
    genModel.setPrivacyAllowed(true);
    QCOMPARE(genModel.generation(), std::numeric_limits<quint32>::max());
    QVERIFY(genModel
                .admit(ClipboardTest::fixtureAlpha(), maxGeneration.generation,
                       QStringLiteral("s"), 1)
                .accepted());
    genModel.setPrivacyAllowed(false);
    QCOMPARE(genModel.snapshot().entries.size(), 0);
    QCOMPARE(genModel.generation(), std::numeric_limits<quint32>::max());
    genModel.setPrivacyAllowed(true);
    const AdmitOutcome afterPurge =
        genModel.admit(ClipboardTest::fixtureAlpha(), maxGeneration.generation,
                       QStringLiteral("s"), 2);
    QCOMPARE(afterPurge.error, ClipboardError::LineageExhausted);
    QCOMPARE(genModel.snapshot().entries.size(), 0);

    // Serial exhaustion: the final serial of a generation is still
    // assignable; the next admission then refuses without forging an
    // identity, while operations that need no serial keep working.
    const HistoryCounters lastSerial { 1, std::numeric_limits<quint32>::max(), 0 };
    ClipboardHistoryModel serialModel(HistoryLimits {}, lastSerial);
    serialModel.setHistoryEnabled(true);
    serialModel.setPrivacyAllowed(true);
    const AdmitOutcome finalAdmit = serialModel.admit(
        ClipboardTest::fixtureAlpha(), lastSerial.generation, QStringLiteral("s"), 1);
    QVERIFY(finalAdmit.accepted());
    QCOMPARE(finalAdmit.entry.id.serial, std::numeric_limits<quint32>::max());
    const AdmitOutcome overflowAdmit = serialModel.admit(
        ClipboardTest::fixtureBeta(), lastSerial.generation, QStringLiteral("s"), 2);
    QCOMPARE(overflowAdmit.error, ClipboardError::LineageExhausted);
    QCOMPARE(serialModel.promote(finalAdmit.entry.id, lastSerial.generation, 3).error,
             ClipboardError::None);

    // Revision exhaustion: content changes refuse once the 64-bit revision
    // lineage is spent; reads and authority transitions stay honest.
    const HistoryCounters maxRevision { 1, 1, std::numeric_limits<quint64>::max() };
    ClipboardHistoryModel revModel(HistoryLimits {}, maxRevision);
    revModel.setHistoryEnabled(true);
    revModel.setPrivacyAllowed(true);
    QCOMPARE(revModel.revision(), std::numeric_limits<quint64>::max());
    const AdmitOutcome noRoom =
        revModel.admit(ClipboardTest::fixtureAlpha(), maxRevision.generation,
                       QStringLiteral("s"), 1);
    QCOMPARE(noRoom.error, ClipboardError::LineageExhausted);
    QCOMPARE(revModel.snapshot().revision, std::numeric_limits<quint64>::max());
    // Disabling still purges; only content operations are fenced.
    revModel.setHistoryEnabled(false);
    QCOMPARE(revModel.snapshot().entries.size(), 0);
}

void ClipboardHistoryLineageTests::constructorSanitizesWidenedLimits()
{
    // Release-path guarantee: a widened limits instance can never survive
    // construction, because fields are clamped rather than asserted.
    HistoryLimits hostile;
    hostile.maxEntries = kMaxEntries + 10;
    hostile.maxPinnedEntries = kMaxPinnedEntries + 10;
    hostile.maxFormatsPerItem = kMaxFormatsPerItem + 10;
    hostile.maxMediaTypeLength = kMaxMediaTypeLength + 10;
    hostile.maxPreviewCodeUnits = kMaxPreviewCodeUnits + 10;
    hostile.maxItemPayloadBytes = kMaxItemPayloadBytes * 4;
    hostile.maxTotalPayloadBytes = kMaxTotalPayloadBytes * 4;

    ClipboardHistoryModel model(hostile);
    const HistoryLimits sanitized = model.limits();
    QVERIFY(isValidLimits(sanitized));
    QCOMPARE(sanitized.maxEntries, kMaxEntries);
    QCOMPARE(sanitized.maxPinnedEntries, kMaxPinnedEntries);
    QCOMPARE(sanitized.maxFormatsPerItem, kMaxFormatsPerItem);
    QCOMPARE(sanitized.maxMediaTypeLength, kMaxMediaTypeLength);
    QCOMPARE(sanitized.maxPreviewCodeUnits, kMaxPreviewCodeUnits);
    QCOMPARE(sanitized.maxItemPayloadBytes, kMaxItemPayloadBytes);
    QCOMPARE(sanitized.maxTotalPayloadBytes, kMaxTotalPayloadBytes);

    // A zero lineage start is equally impossible through the seam.
    ClipboardHistoryModel zeroed(HistoryLimits {}, HistoryCounters { 0, 0, 7 });
    QCOMPARE(zeroed.generation(), quint32 { 1 });
    QCOMPARE(zeroed.revision(), quint64 { 7 });
}

void ClipboardHistoryLineageTests::metadataSearchIsBoundedAndGated()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-alpha"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-beta"), 2)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixturePngLike(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-png"), 3)
                .accepted());

    // Case-insensitive metadata search, most recent first.
    const SearchOutcome labelHit =
        model.search(QStringLiteral("SOURCE-BETA"), ClipboardTest::kGen, kMaxEntries);
    QVERIFY(labelHit.accepted());
    QCOMPARE(labelHit.matches.size(), 1);
    QCOMPARE(labelHit.matches.first().sourceLabel, QStringLiteral("fixture-source-beta"));
    QVERIFY(!labelHit.truncated);

    const SearchOutcome previewHit =
        model.search(QStringLiteral("fixture beta"), ClipboardTest::kGen, kMaxEntries);
    QVERIFY(previewHit.accepted());
    QCOMPARE(previewHit.matches.size(), 1);
    QCOMPARE(previewHit.matches.first().preview, QStringLiteral("fixture beta payload"));

    // The cap is honored and reported through truncated. All three labels
    // contain "source" (the PNG-like fixture has an empty preview), and
    // matches stay most-recent-first.
    const SearchOutcome capped =
        model.search(QStringLiteral("source"), ClipboardTest::kGen, 2);
    QVERIFY(capped.accepted());
    QCOMPARE(capped.matches.size(), 2);
    QVERIFY(capped.truncated);
    QCOMPARE(capped.matches.first().sourceLabel, QStringLiteral("fixture-source-png"));
    QCOMPARE(capped.matches.at(1).sourceLabel, QStringLiteral("fixture-source-beta"));

    // maxResults is sanitized, never trusted.
    const SearchOutcome zeroMax = model.search(QStringLiteral("source"), ClipboardTest::kGen, 0);
    QVERIFY(zeroMax.accepted());
    QCOMPARE(zeroMax.matches.size(), 1);
    QVERIFY(zeroMax.truncated);
    QCOMPARE(zeroMax.matches.first().sourceLabel, QStringLiteral("fixture-source-png"));

    // Empty and oversized queries refuse without touching state.
    QCOMPARE(model.search(QString(), ClipboardTest::kGen, kMaxEntries).error,
             ClipboardError::EmptyValue);
    QCOMPARE(model.search(QString(kMaxPreviewCodeUnits + 1, QLatin1Char('x')),
                          ClipboardTest::kGen, kMaxEntries)
                 .error,
             ClipboardError::OversizedValue);

    // Staleness, privacy, and the opt-in gate all fence search reads.
    QCOMPARE(model.search(QStringLiteral("fixture"), ClipboardTest::kGen + 7, kMaxEntries)
                 .error,
             ClipboardError::StaleGeneration);
    model.setPrivacyAllowed(false);
    const SearchOutcome denied =
        model.search(QStringLiteral("fixture"), model.generation(), kMaxEntries);
    QCOMPARE(denied.error, ClipboardError::PrivacyDenied);
    QVERIFY(denied.matches.isEmpty());
    model.setHistoryEnabled(false);
    QCOMPARE(model.search(QStringLiteral("fixture"), model.generation(), kMaxEntries)
                 .error,
             ClipboardError::HistoryDisabled);
}

void ClipboardHistoryLineageTests::privacyLossPurgesAndRaisesGeneration()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("s"), 1)
                .accepted());
    const quint32 generationBefore = model.generation();

    model.setPrivacyAllowed(false);
    QCOMPARE(model.generation(), generationBefore + 1);
    QCOMPARE(model.snapshot().entries.size(), 0);
    QCOMPARE(model.totalPayloadBytes(), qint64 { 0 });

    // Re-stating the denied state is a no-op, not a purge.
    model.setPrivacyAllowed(false);
    QCOMPARE(model.generation(), generationBefore + 1);

    model.setPrivacyAllowed(true);
    QCOMPARE(model.snapshot().entries.size(), 0);
    // A decision made before the privacy transition is refused even though
    // privacy is allowed again: the generation moved twice.
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), generationBefore,
                         QStringLiteral("s"), 2)
                 .error,
             ClipboardError::StaleGeneration);
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), model.generation(),
                        QStringLiteral("s"), 2)
                .accepted());
}

void ClipboardHistoryLineageTests::disablePurgesAndRaisesGeneration()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("s"), 1)
                .accepted());
    const quint32 generationBefore = model.generation();

    // Enabling twice changes nothing.
    model.setHistoryEnabled(true);
    QCOMPARE(model.generation(), generationBefore);

    model.setHistoryEnabled(false);
    QCOMPARE(model.generation(), generationBefore + 1);
    QCOMPARE(model.snapshot().entries.size(), 0);

    // With privacy allowed but history disabled, admission is refused as
    // disabled — the opt-in contract — and stays refused after re-enable
    // with a fresh generation.
    model.setPrivacyAllowed(true);
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), generationBefore,
                         QStringLiteral("s"), 2)
                 .error,
             ClipboardError::HistoryDisabled);
    model.setHistoryEnabled(true);
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), generationBefore,
                         QStringLiteral("s"), 2)
                 .error,
             ClipboardError::StaleGeneration);
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), model.generation(),
                        QStringLiteral("s"), 2)
                .accepted());
}

QTEST_MAIN(ClipboardHistoryLineageTests)
#include "tst_clipboard_history_lineage.moc"
