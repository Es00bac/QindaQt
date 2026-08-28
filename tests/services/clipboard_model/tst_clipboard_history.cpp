// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include "support/clipboard_test_data.h"

#include <QtTest>

using namespace QindaQt::Services::ClipboardModel;

// Core history semantics: fail-closed defaults, gate and refusal ordering,
// value admission rules, deterministic eviction/capacity atomicity, dedup,
// pinning, clearing, and byte-total accounting. Lineage, authority, and
// gated-read coverage lives in tst_clipboard_history_lineage.cpp.

class ClipboardHistoryTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsFailClosed();
    void refusesAdmissionWhenDisabledOrPrivate();
    void admissionRefusalOrderIsDeterministic();
    void rejectsEmptyOversizedAndDuplicateValues();
    void snapshotOrdersMostRecentFirst();
    void evictionIsDeterministicAndSparesPins();
    void capacityRefusalNeverMutates();
    void capacityRefusalIsFullyAtomic();
    void dedupMovesToFrontAndKeepsPin();
    void promoteReturnsCopyAndRefreshesRecency();
    void mixedClassRefusalsAreOrderIndependent();
    void pinLimitAndScopeClearsAreDeterministic();
    void byteTotalsTrackEveryTransition();
};

void ClipboardHistoryTests::startsFailClosed()
{
    ClipboardHistoryModel model;
    QCOMPARE(model.isHistoryEnabled(), false);
    QCOMPARE(model.privacyState(), PrivacyState::Denied);
    QCOMPARE(model.generation(), quint32 { 1 });
    QCOMPARE(model.revision(), quint64 { 0 });

    const HistorySnapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.entries.size(), 0);
    QCOMPARE(snapshot.historyEnabled, false);
    QCOMPARE(snapshot.privacyAllowed, false);
    QCOMPARE(snapshot.totalPayloadBytes, qint64 { 0 });
    QCOMPARE(snapshot.generation, quint32 { 1 });
}

void ClipboardHistoryTests::refusesAdmissionWhenDisabledOrPrivate()
{
    ClipboardHistoryModel model;
    model.setPrivacyAllowed(true);
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                         QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::HistoryDisabled);

    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(false);
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                         QStringLiteral("fixture-source"), 2)
                 .error,
             ClipboardError::PrivacyDenied);

    // Promote, remove, pin, and clear are fenced by the same gate.
    model.setHistoryEnabled(false);
    const AdmitOutcome never = model.admit(ClipboardTest::fixtureAlpha(),
                                           ClipboardTest::kGen,
                                           QStringLiteral("fixture-source"), 3);
    QCOMPARE(never.error, ClipboardError::HistoryDisabled);
    const EntryId missing { 1, 1 };
    QCOMPARE(model.promote(missing, ClipboardTest::kGen, 4).error,
             ClipboardError::HistoryDisabled);
    QCOMPARE(model.removeEntry(missing, ClipboardTest::kGen).error,
             ClipboardError::HistoryDisabled);
    QCOMPARE(model.setPinned(missing, true, ClipboardTest::kGen).error,
             ClipboardError::HistoryDisabled);
    QCOMPARE(model.clear(ClearScope::All, ClipboardTest::kGen).error,
             ClipboardError::HistoryDisabled);
    QCOMPARE(model.revision(), quint64 { 0 });
}

void ClipboardHistoryTests::admissionRefusalOrderIsDeterministic()
{
    ClipboardHistoryModel model;
    // Disabled outranks privacy and staleness.
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), 99,
                         QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::HistoryDisabled);
    model.setHistoryEnabled(true);
    // Privacy outranks staleness.
    QCOMPARE(model.admit(ClipboardTest::fixtureAlpha(), 99,
                         QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::PrivacyDenied);
    model.setPrivacyAllowed(true);
    // Staleness outranks value validity.
    const AdmitOutcome staleInvalid =
        model.admit(ClipboardValue {}, 99, QStringLiteral("fixture-source"), 1);
    QCOMPARE(staleInvalid.error, ClipboardError::StaleGeneration);
    QCOMPARE(model.admit(ClipboardValue {}, ClipboardTest::kGen,
                         QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::EmptyValue);
}

void ClipboardHistoryTests::rejectsEmptyOversizedAndDuplicateValues()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();

    QCOMPARE(model.admit(ClipboardValue {}, ClipboardTest::kGen,
                         QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::EmptyValue);
    // A value whose every format is zero-length is still empty.
    ClipboardValue allEmpty;
    allEmpty.formats.append({ ClipboardTest::textFormat(), QByteArray() });
    QCOMPARE(model.admit(allEmpty, ClipboardTest::kGen, QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::EmptyValue);

    ClipboardValue tooMany = ClipboardTest::fixtureAlpha();
    for (int i = 0; i < kMaxFormatsPerItem; ++i) {
        tooMany.formats.append({ ClipboardTest::uriFormat(),
                                  QByteArrayLiteral("fixture-uri") });
    }
    QCOMPARE(model.admit(tooMany, ClipboardTest::kGen, QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::TooManyFormats);

    ClipboardValue duplicated;
    duplicated.formats.append({ ClipboardTest::textFormat(),
                                QByteArrayLiteral("fixture-a") });
    duplicated.formats.append({ QStringLiteral("TEXT/PLAIN"),
                                QByteArrayLiteral("fixture-b") });
    QCOMPARE(model.admit(duplicated, ClipboardTest::kGen, QStringLiteral("fixture-source"), 1)
                 .error,
             ClipboardError::DuplicateFormat);

    HistoryLimits limits;
    limits.maxItemPayloadBytes = 16;
    limits.maxTotalPayloadBytes = 64;
    ClipboardHistoryModel smallModel = ClipboardTest::enabledModel(limits);
    ClipboardValue oversized = ClipboardTest::fixtureAlpha();
    oversized.formats.append({ ClipboardTest::uriFormat(),
                                QByteArrayLiteral("0123456789abcdef0123456789abcdef") });
    const AdmitOutcome refused = smallModel.admit(
        oversized, ClipboardTest::kGen, QStringLiteral("fixture-source"), 1);
    QCOMPARE(refused.error, ClipboardError::OversizedValue);
    QCOMPARE(smallModel.snapshot().entries.size(), 0);
}

void ClipboardHistoryTests::snapshotOrdersMostRecentFirst()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-a"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-b"), 2)
                .accepted());

    const HistorySnapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.entries.size(), 2);
    QCOMPARE(snapshot.generation, quint32 { 1 });
    QCOMPARE(snapshot.revision, quint64 { 2 });
    QCOMPARE(snapshot.historyEnabled, true);
    QCOMPARE(snapshot.privacyAllowed, true);
    // Most recent first: beta was admitted last.
    QCOMPARE(ClipboardTest::previewOrder(snapshot),
             (QList<QString> { QStringLiteral("fixture beta payload"),
                               QStringLiteral("fixture alpha payload") }));
    // Descriptors expose metadata but never payload bytes.
    for (const ClipboardEntryDescriptor &entry : snapshot.entries) {
        QVERIFY(entry.id.isValid());
        QCOMPARE(entry.fingerprint.size(), 32);
        QVERIFY(!entry.sourceLabel.isEmpty());
        for (const FormatInfo &format : entry.formats) {
            QVERIFY(format.payloadBytes >= 0);
        }
    }
    QCOMPARE(snapshot.entries.at(1).formats.first().mediaType, ClipboardTest::textFormat());
    QCOMPARE(snapshot.entries.at(0).formats.size(), 2);
}

void ClipboardHistoryTests::evictionIsDeterministicAndSparesPins()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel(ClipboardTest::limitsOf(3, kMaxTotalPayloadBytes));
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture a")),
                        ClipboardTest::kGen, QStringLiteral("s"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture b")),
                        ClipboardTest::kGen, QStringLiteral("s"), 2)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture c")),
                        ClipboardTest::kGen, QStringLiteral("s"), 3)
                .accepted());
    QCOMPARE(model.snapshot().entries.size(), 3);

    // Oldest (least recent) is evicted first.
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture d")),
                        ClipboardTest::kGen, QStringLiteral("s"), 4)
                .accepted());
    QVERIFY(!ClipboardTest::previewOrder(model.snapshot()).contains(QStringLiteral("fixture a")));
    QCOMPARE(model.snapshot().entries.size(), 3);

    // Pin the now-oldest entry; eviction must skip it.
    const EntryId oldest = model.snapshot().entries.last().id;
    QVERIFY(model.setPinned(oldest, true, ClipboardTest::kGen).accepted());
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture e")),
                        ClipboardTest::kGen, QStringLiteral("s"), 5)
                .accepted());
    const QList<QString> previews = ClipboardTest::previewOrder(model.snapshot());
    QVERIFY(previews.contains(QStringLiteral("fixture b")));
    QVERIFY(!previews.contains(QStringLiteral("fixture c")));
    QCOMPARE(model.snapshot().entries.last().pinned, true);
}

void ClipboardHistoryTests::capacityRefusalNeverMutates()
{
    HistoryLimits limits = ClipboardTest::limitsOf(2, 64);
    limits.maxPinnedEntries = 2;
    ClipboardHistoryModel model = ClipboardTest::enabledModel(limits);
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture a")),
                        ClipboardTest::kGen, QStringLiteral("s"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("fixture b")),
                        ClipboardTest::kGen, QStringLiteral("s"), 2)
                .accepted());
    QVERIFY(model.setPinned(model.snapshot().entries.first().id, true,
                            ClipboardTest::kGen)
                .accepted());
    QVERIFY(model.setPinned(model.snapshot().entries.last().id, true,
                            ClipboardTest::kGen)
                .accepted());

    const quint64 revisionBefore = model.revision();
    const AdmitOutcome refused =
        model.admit(ClipboardTest::textValue(QStringLiteral("fixture c")),
                    ClipboardTest::kGen, QStringLiteral("s"), 3);
    QCOMPARE(refused.error, ClipboardError::CapacityRefused);
    QCOMPARE(model.snapshot().entries.size(), 2);
    QCOMPARE(model.revision(), revisionBefore);
    QCOMPARE(model.totalPayloadBytes(), qint64 { 2 * int(QStringLiteral("fixture a").size()) });

    // Total-byte pressure with no evictable entry refuses cleanly: a pinned
    // sole entry can never be evicted, and identical content would dedup
    // instead of consuming budget.
    HistoryLimits byteLimits = ClipboardTest::limitsOf(kMaxEntries, 1);
    byteLimits.maxItemPayloadBytes = 1;
    ClipboardHistoryModel tiny = ClipboardTest::enabledModel(byteLimits);
    ClipboardValue oneByte;
    oneByte.formats.append({ ClipboardTest::textFormat(), QByteArrayLiteral("x") });
    QVERIFY(tiny.admit(oneByte, ClipboardTest::kGen, QStringLiteral("s"), 1).accepted());
    QVERIFY(tiny.setPinned(tiny.snapshot().entries.first().id, true, ClipboardTest::kGen)
                .accepted());
    ClipboardValue otherByte;
    otherByte.formats.append({ ClipboardTest::textFormat(), QByteArrayLiteral("y") });
    QCOMPARE(tiny.admit(otherByte, ClipboardTest::kGen, QStringLiteral("s"), 2).error,
             ClipboardError::CapacityRefused);
    QCOMPARE(tiny.snapshot().entries.size(), 1);
}

void ClipboardHistoryTests::dedupMovesToFrontAndKeepsPin()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    const AdmitOutcome first = model.admit(ClipboardTest::fixtureAlpha(),
                                           ClipboardTest::kGen,
                                           QStringLiteral("fixture-source-a"), 1);
    QVERIFY(first.accepted());
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), ClipboardTest::kGen,
                        QStringLiteral("fixture-source-b"), 2)
                .accepted());
    QVERIFY(model.setPinned(first.entry.id, true, ClipboardTest::kGen).accepted());

    // Re-admitting identical content dedups: same identity, moved to front,
    // pin kept, byte totals unchanged.
    const quint64 revisionBefore = model.revision();
    const qint64 bytesBefore = model.totalPayloadBytes();
    const AdmitOutcome dedup = model.admit(ClipboardTest::fixtureAlpha(),
                                           ClipboardTest::kGen,
                                           QStringLiteral("fixture-source-again"), 3);
    QVERIFY(dedup.accepted());
    QCOMPARE(dedup.entry.id, first.entry.id);
    QCOMPARE(dedup.entry.pinned, true);
    QCOMPARE(ClipboardTest::previewOrder(model.snapshot()).first(), QStringLiteral("fixture alpha payload"));
    QCOMPARE(model.snapshot().entries.size(), 2);
    QCOMPARE(model.totalPayloadBytes(), bytesBefore);
    QCOMPARE(model.revision(), revisionBefore + 1);

    // Distinct content with identical canonical payload but different media
    // list is not deduped.
    ClipboardValue otherMedia;
    otherMedia.formats.append({ ClipboardTest::htmlFormat(),
                                QByteArrayLiteral("fixture alpha payload") });
    const AdmitOutcome distinct = model.admit(otherMedia, ClipboardTest::kGen,
                                              QStringLiteral("fixture-source"), 4);
    QVERIFY(distinct.accepted());
    QVERIFY(distinct.entry.id != first.entry.id);
}

void ClipboardHistoryTests::promoteReturnsCopyAndRefreshesRecency()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen,
                        QStringLiteral("s-a"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), ClipboardTest::kGen, QStringLiteral("s-b"),
                        2)
                .accepted());

    const EntryId alphaId = model.snapshot().entries.last().id;
    PromoteOutcome promoted = model.promote(alphaId, ClipboardTest::kGen, 3);
    QVERIFY(promoted.accepted());
    QCOMPARE(promoted.value, ClipboardTest::fixtureAlpha());
    QCOMPARE(promoted.entry.id, alphaId);
    QCOMPARE(promoted.entry.lastUsedTick, quint64 { 3 });
    QCOMPARE(ClipboardTest::previewOrder(model.snapshot()).first(), QStringLiteral("fixture alpha payload"));

    // The returned value is a copy; mutating it cannot touch stored state.
    promoted.value.formats.first().payload.fill('x');
    PromoteOutcome again = model.promote(alphaId, ClipboardTest::kGen, 4);
    QCOMPARE(again.value, ClipboardTest::fixtureAlpha());

    QCOMPARE(model.promote(EntryId { 1, 999 }, ClipboardTest::kGen, 5).error,
             ClipboardError::UnknownEntry);
    // Privacy loss outranks staleness in the documented gate order, and the
    // purge itself leaves the old id unresolvable even after privacy
    // returns.
    model.setPrivacyAllowed(false);
    QCOMPARE(model.promote(alphaId, ClipboardTest::kGen, 6).error,
             ClipboardError::PrivacyDenied);
    model.setPrivacyAllowed(true);
    QCOMPARE(model.promote(alphaId, ClipboardTest::kGen, 7).error,
             ClipboardError::StaleGeneration);
    QCOMPARE(model.promote(alphaId, model.generation(), 7).error, ClipboardError::UnknownEntry);
}

void ClipboardHistoryTests::pinLimitAndScopeClearsAreDeterministic()
{
    HistoryLimits limits = ClipboardTest::limitsOf(kMaxEntries, kMaxTotalPayloadBytes);
    limits.maxPinnedEntries = 2;
    ClipboardHistoryModel model = ClipboardTest::enabledModel(limits);
    for (int i = 0; i < 3; ++i) {
        QVERIFY(model
                    .admit(ClipboardTest::textValue(QStringLiteral("fixture %1").arg(i)),
                           ClipboardTest::kGen, QStringLiteral("s"), quint64(i + 1))
                    .accepted());
    }
    const QList<ClipboardEntryDescriptor> initial = model.snapshot().entries;
    QVERIFY(model.setPinned(initial.at(0).id, true, ClipboardTest::kGen).accepted());
    QVERIFY(model.setPinned(initial.at(1).id, true, ClipboardTest::kGen).accepted());
    QCOMPARE(model.setPinned(initial.at(2).id, true, ClipboardTest::kGen).error,
             ClipboardError::PinnedLimitReached);

    // UnpinnedOnly keeps the two pins.
    QVERIFY(model.clear(ClearScope::UnpinnedOnly, ClipboardTest::kGen).accepted());
    QCOMPARE(model.snapshot().entries.size(), 2);
    QVERIFY(model.snapshot().entries.first().pinned);
    QVERIFY(model.snapshot().entries.last().pinned);

    // Clearing nothing again is a successful no-op without revision churn.
    const quint64 revisionBefore = model.revision();
    QVERIFY(model.clear(ClearScope::UnpinnedOnly, ClipboardTest::kGen).accepted());
    QCOMPARE(model.revision(), revisionBefore);

    // All clears pins as well but keeps the generation stable.
    const quint32 generationBefore = model.generation();
    QVERIFY(model.clear(ClearScope::All, ClipboardTest::kGen).accepted());
    QCOMPARE(model.snapshot().entries.size(), 0);
    QCOMPARE(model.generation(), generationBefore);

    // Unknown ids fail closed on every mutation.
    QCOMPARE(model.removeEntry(EntryId { 1, 42 }, ClipboardTest::kGen).error,
             ClipboardError::UnknownEntry);
    QCOMPARE(model.setPinned(EntryId { 1, 42 }, false, ClipboardTest::kGen).error,
             ClipboardError::UnknownEntry);
}

void ClipboardHistoryTests::capacityRefusalIsFullyAtomic()
{
    // Exact reproduction from the failed review: a ten-byte total limit, a
    // pinned seven-byte item, and an unpinned two-byte item. Admitting four
    // bytes needs more than the unpinned eviction can free, so the
    // admission must refuse WITHOUT deleting the two-byte item or moving
    // any counter.
    HistoryLimits limits;
    limits.maxEntries = 8;
    limits.maxPinnedEntries = 1;
    limits.maxItemPayloadBytes = 10;
    limits.maxTotalPayloadBytes = 10;
    ClipboardHistoryModel model = ClipboardTest::enabledModel(limits);

    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("1234567")),
                        ClipboardTest::kGen, QStringLiteral("s"), 1)
                .accepted());
    QVERIFY(model.setPinned(model.snapshot().entries.first().id, true, ClipboardTest::kGen)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("89")),
                        ClipboardTest::kGen, QStringLiteral("s"), 2)
                .accepted());
    QCOMPARE(model.totalPayloadBytes(), qint64 { 9 });
    const quint64 revisionBefore = model.revision();
    const quint32 generationBefore = model.generation();

    const AdmitOutcome refused =
        model.admit(ClipboardTest::textValue(QStringLiteral("abcd")),
                    ClipboardTest::kGen, QStringLiteral("s"), 3);
    QCOMPARE(refused.error, ClipboardError::CapacityRefused);
    // Nothing at all changed: both entries survive, bytes and revision are
    // untouched, and no eviction is left half-applied.
    QCOMPARE(model.snapshot().entries.size(), 2);
    QCOMPARE(model.totalPayloadBytes(), qint64 { 9 });
    QCOMPARE(model.revision(), revisionBefore);
    QCOMPARE(model.generation(), generationBefore);
    QVERIFY(!model.snapshot().entries.first().pinned);
    QVERIFY(model.snapshot().entries.last().pinned);
    QCOMPARE(model.snapshot().entries.first().preview, QStringLiteral("89"));

    // The success path still frees exactly the unpinned entry and keeps the
    // pin, proving the atomic refusal was a decision, not a fallback.
    QVERIFY(model.admit(ClipboardTest::textValue(QStringLiteral("abc")),
                        ClipboardTest::kGen, QStringLiteral("s"), 4)
                .accepted());
    QCOMPARE(model.snapshot().entries.size(), 2);
    QCOMPARE(model.totalPayloadBytes(), qint64 { 10 });
    QCOMPARE(model.snapshot().entries.last().pinned, true);
    QCOMPARE(model.snapshot().entries.last().preview, QStringLiteral("1234567"));
}

void ClipboardHistoryTests::mixedClassRefusalsAreOrderIndependent()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel();

    const auto admit = [&model](const ClipboardValue &value) {
        return model.admit(value, ClipboardTest::kGen, QStringLiteral("fixture-source"), 1)
            .error;
    };

    // Sensitive must win no matter where it appears in the format list.
    ClipboardValue unknownThenSensitive;
    unknownThenSensitive.formats.append(
        { ClipboardTest::unknownMediaType(), QByteArrayLiteral("fixture") });
    unknownThenSensitive.formats.append(
        { ClipboardTest::sensitiveMarker(), QByteArrayLiteral("1") });
    QCOMPARE(admit(unknownThenSensitive), ClipboardError::SensitiveRefused);

    ClipboardValue oneTimeThenSensitive;
    oneTimeThenSensitive.formats.append(
        { ClipboardTest::oneTimeMarker(), QByteArrayLiteral("1") });
    oneTimeThenSensitive.formats.append(
        { ClipboardTest::sensitiveMarker(), QByteArrayLiteral("2") });
    QCOMPARE(admit(oneTimeThenSensitive), ClipboardError::SensitiveRefused);

    // One-time must beat non-storable regardless of order.
    ClipboardValue unknownThenOneTime;
    unknownThenOneTime.formats.append(
        { ClipboardTest::unknownMediaType(), QByteArrayLiteral("fixture") });
    unknownThenOneTime.formats.append(
        { ClipboardTest::oneTimeMarker(), QByteArrayLiteral("1") });
    QCOMPARE(admit(unknownThenOneTime), ClipboardError::OneTimeRefused);

    ClipboardValue oneTimeThenUnknown;
    oneTimeThenUnknown.formats.append(
        { ClipboardTest::oneTimeMarker(), QByteArrayLiteral("1") });
    oneTimeThenUnknown.formats.append(
        { ClipboardTest::unknownMediaType(), QByteArrayLiteral("fixture") });
    QCOMPARE(admit(oneTimeThenUnknown), ClipboardError::OneTimeRefused);

    // Any non-storable member refuses a mixed storable value.
    ClipboardValue storableThenUnknown;
    storableThenUnknown.formats.append({ ClipboardTest::textFormat(),
                                         QByteArrayLiteral("fixture") });
    storableThenUnknown.formats.append(
        { ClipboardTest::unknownMediaType(), QByteArrayLiteral("fixture") });
    QCOMPARE(admit(storableThenUnknown), ClipboardError::NonStorableRefused);

    ClipboardValue unknownThenStorable;
    unknownThenStorable.formats.append(
        { ClipboardTest::unknownMediaType(), QByteArrayLiteral("fixture") });
    unknownThenStorable.formats.append({ ClipboardTest::textFormat(),
                                         QByteArrayLiteral("fixture") });
    QCOMPARE(admit(unknownThenStorable), ClipboardError::NonStorableRefused);

    // Nothing leaked into the history across the permutations.
    QCOMPARE(model.snapshot().entries.size(), 0);
    QCOMPARE(model.revision(), quint64 { 0 });
}

void ClipboardHistoryTests::byteTotalsTrackEveryTransition()
{
    ClipboardHistoryModel model = ClipboardTest::enabledModel(ClipboardTest::limitsOf(8, 1024));
    const qint64 alphaBytes = ClipboardTest::fixtureAlpha().formats.first().payload.size();
    const qint64 betaBytes =
        ClipboardTest::fixtureBeta().formats.first().payload.size()
        + ClipboardTest::fixtureBeta().formats.last().payload.size();

    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen, QStringLiteral("s"),
                        1)
                .accepted());
    QCOMPARE(model.totalPayloadBytes(), alphaBytes);
    QCOMPARE(model.snapshot().totalPayloadBytes, alphaBytes);
    const AdmitOutcome betaAdmit =
        model.admit(ClipboardTest::fixtureBeta(), ClipboardTest::kGen, QStringLiteral("s"), 2);
    QVERIFY(betaAdmit.accepted());
    QCOMPARE(model.totalPayloadBytes(), alphaBytes + betaBytes);

    // Dedup must not double-count.
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), ClipboardTest::kGen, QStringLiteral("s"),
                        3)
                .accepted());
    QCOMPARE(model.totalPayloadBytes(), alphaBytes + betaBytes);

    // Removing by identity removes exactly that entry's bytes.
    QVERIFY(model.removeEntry(betaAdmit.entry.id, ClipboardTest::kGen).accepted());
    QCOMPARE(model.totalPayloadBytes(), alphaBytes);

    QVERIFY(model.clear(ClearScope::All, ClipboardTest::kGen).accepted());
    QCOMPARE(model.totalPayloadBytes(), qint64 { 0 });
}

QTEST_MAIN(ClipboardHistoryTests)
#include "tst_clipboard_history.moc"
