// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_codec.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include "support/clipboard_test_data.h"

#include <QtTest>
#include <QtEndian>

#include <limits>

using namespace QindaQt::Services::ClipboardModel;

namespace {

ClipboardHistoryModel enabledModel()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);
    return model;
}

QByteArray withFirstByteFlipped(const QByteArray &bytes)
{
    QByteArray copy = bytes;
    copy[0] = static_cast<char>(copy.at(0) ^ 0x20);
    return copy;
}

QByteArray truncated(const QByteArray &bytes, qsizetype keep)
{
    return bytes.left(keep);
}

QByteArray withAppendedByte(const QByteArray &bytes)
{
    QByteArray copy = bytes;
    copy.append('Z');
    return copy;
}

void appendLe16(QByteArray &bytes, quint16 value)
{
    char encoded[2];
    qToLittleEndian(value, encoded);
    bytes.append(encoded, 2);
}

void appendLe32(QByteArray &bytes, quint32 value)
{
    char encoded[4];
    qToLittleEndian(value, encoded);
    bytes.append(encoded, 4);
}

quint16 readLe16(const QByteArray &bytes, qsizetype offset)
{
    Q_ASSERT(offset >= 0 && bytes.size() - offset >= 2);
    return qFromLittleEndian<quint16>(bytes.constData() + offset);
}

void appendEncodedFormat(QByteArray &bytes, const QString &mediaType,
                         const QByteArray &payload)
{
    const QByteArray mediaBytes = mediaType.toUtf8();
    Q_ASSERT(mediaBytes.size()
             <= static_cast<qsizetype>(std::numeric_limits<quint16>::max()));
    Q_ASSERT(payload.size()
             <= static_cast<qsizetype>(std::numeric_limits<quint32>::max()));
    appendLe16(bytes, static_cast<quint16>(mediaBytes.size()));
    bytes.append(mediaBytes);
    appendLe32(bytes, static_cast<quint32>(payload.size()));
    bytes.append(payload);
}

} // namespace

class ClipboardCodecTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void valueRoundTrips();
    void valueEncodeIsSymmetricWithDecode();
    void valueDecodeRejectsHostileInput();
    void descriptorRoundTrips();
    void descriptorDecodeRejectsHostileInput();
    void descriptorListRoundTripsAndRejectsHostileInput();
    void encodedFormsNeverCarryPayloadMetadataMismatch();
};

void ClipboardCodecTests::valueRoundTrips()
{
    for (const ClipboardValue &fixture :
         { ClipboardTest::fixtureAlpha(), ClipboardTest::fixtureBeta(),
           ClipboardTest::fixturePngLike() }) {
        const EncodedValue encoded = encodeValue(fixture);
        QVERIFY2(encoded.accepted(), "encoding a valid fixture must succeed");
        const DecodedValue decoded = decodeValue(encoded.bytes);
        QVERIFY(decoded.accepted());
        QCOMPARE(decoded.value, fixture);
        // Canonical bytes are deterministic: encoding twice is identical.
        QCOMPARE(encodeValue(decoded.value).bytes, encoded.bytes);
    }
}

void ClipboardCodecTests::valueEncodeIsSymmetricWithDecode()
{
    // The encoder must refuse every form its decoder refuses — in
    // particular duplicates, which previously encoded cleanly and then
    // failed their own round-trip.
    ClipboardValue duplicateMedia;
    duplicateMedia.formats.append({ ClipboardTest::textFormat(),
                                    QByteArrayLiteral("fixture-a") });
    duplicateMedia.formats.append({ ClipboardTest::textFormat(),
                                    QByteArrayLiteral("fixture-b") });
    QCOMPARE(encodeValue(duplicateMedia).error, ClipboardError::DuplicateFormat);

    ClipboardValue exactDuplicate;
    exactDuplicate.formats.append({ ClipboardTest::htmlFormat(),
                                    QByteArrayLiteral("fixture") });
    exactDuplicate.formats.append({ ClipboardTest::htmlFormat(),
                                    QByteArrayLiteral("fixture") });
    QCOMPARE(encodeValue(exactDuplicate).error, ClipboardError::DuplicateFormat);

    // Non-canonical spellings refuse exactly like the decoder.
    ClipboardValue nonCanonical;
    nonCanonical.formats.append({ QStringLiteral("TEXT/PLAIN"),
                                  QByteArrayLiteral("fixture") });
    QCOMPARE(encodeValue(nonCanonical).error, ClipboardError::MediaTypeRejected);

    // Per-format and aggregate ceilings refuse before any byte is written:
    // an accepted encoding never exists for oversized input.
    ClipboardValue oversizedSingle;
    oversizedSingle.formats.append({ ClipboardTest::textFormat(),
                                     QByteArray(kMaxItemPayloadBytes + 1, 'x') });
    QCOMPARE(encodeValue(oversizedSingle).error, ClipboardError::OversizedValue);
    ClipboardValue oversizedAggregate;
    oversizedAggregate.formats.append({ ClipboardTest::textFormat(),
                                        QByteArrayLiteral("fixture-a") });
    oversizedAggregate.formats.append(
        { ClipboardTest::uriFormat(), QByteArray(kMaxItemPayloadBytes, 'y') });
    QCOMPARE(encodeValue(oversizedAggregate).error, ClipboardError::OversizedValue);
}

void ClipboardCodecTests::valueDecodeRejectsHostileInput()
{
    const EncodedValue encoded = encodeValue(ClipboardTest::fixtureBeta());
    QVERIFY(encoded.accepted());

    QCOMPARE(decodeValue(QByteArray()).error, ClipboardError::MalformedData);
    QCOMPARE(decodeValue(QByteArrayLiteral("nope")).error, ClipboardError::MalformedData);
    QCOMPARE(decodeValue(withFirstByteFlipped(encoded.bytes)).error,
             ClipboardError::MalformedData);
    // Shorter than the magic is malformed; magic without a version byte is
    // a version failure.
    QCOMPARE(decodeValue(truncated(encoded.bytes, 3)).error, ClipboardError::MalformedData);
    QCOMPARE(decodeValue(truncated(encoded.bytes, 4)).error, ClipboardError::UnsupportedVersion);

    // Wrong version byte (offset 4).
    QByteArray badVersion = encoded.bytes;
    badVersion[4] = 2;
    QCOMPARE(decodeValue(badVersion).error, ClipboardError::UnsupportedVersion);

    // Format count beyond the protocol bound.
    QByteArray tooManyFormats = encoded.bytes;
    tooManyFormats[5] = static_cast<char>(0xf9);
    tooManyFormats[6] = static_cast<char>(0x03); // little-endian 1017
    QCOMPARE(decodeValue(tooManyFormats).error, ClipboardError::TooManyFormats);

    // The exact seven-byte zero-count form is the decoder equivalent of an
    // empty ClipboardValue and therefore shares EmptyValue vocabulary.
    QByteArray zeroFormats("QCBV", 4);
    zeroFormats.append(char(1));
    appendLe16(zeroFormats, 0);
    QCOMPARE(zeroFormats.size(), 7);
    QCOMPARE(decodeValue(zeroFormats).error, ClipboardError::EmptyValue);

    QByteArray truncatedFormatCount("QCBV", 4);
    truncatedFormatCount.append(char(1));
    const DecodedValue truncatedCountRejected = decodeValue(truncatedFormatCount);
    QCOMPARE(truncatedCountRejected.error, ClipboardError::MalformedData);
    QVERIFY(!truncatedCountRejected.accepted());
    QVERIFY(truncatedCountRejected.value.formats.isEmpty());

    // Declared media length beyond the canonical bound.
    QByteArray longMedia = encoded.bytes;
    longMedia[7] = static_cast<char>(0xff);
    longMedia[8] = static_cast<char>(0x7f); // 32767 claimed, 9 present
    QCOMPARE(decodeValue(longMedia).error, ClipboardError::MediaTypeRejected);

    // Trailing byte after a complete canonical form.
    const DecodedValue trailingRejected = decodeValue(withAppendedByte(encoded.bytes));
    QCOMPARE(trailingRejected.error, ClipboardError::MalformedData);
    QVERIFY(trailingRejected.value.formats.isEmpty());

    // Encode side refuses values the model would refuse.
    QCOMPARE(encodeValue(ClipboardValue {}).error, ClipboardError::EmptyValue);
    ClipboardValue allEmpty;
    allEmpty.formats.append({ ClipboardTest::textFormat(), QByteArray() });
    QCOMPARE(encodeValue(allEmpty).error, ClipboardError::EmptyValue);
    ClipboardValue nonCanonical;
    nonCanonical.formats.append({ QStringLiteral("TEXT/PLAIN"),
                                  QByteArrayLiteral("fixture") });
    QCOMPARE(encodeValue(nonCanonical).error, ClipboardError::MediaTypeRejected);

    // Decode refuses duplicate media even when the byte framing is perfect:
    // both formats are 9-byte names, so the swap keeps every length field
    // valid and only the duplicate rule can catch the forgery.
    ClipboardValue swappable;
    swappable.formats.append({ ClipboardTest::htmlFormat(),
                               QByteArrayLiteral("fixture-a") });
    swappable.formats.append({ ClipboardTest::pngFormat(),
                               QByteArrayLiteral("fixture-b") });
    const EncodedValue validTwo = encodeValue(swappable);
    QVERIFY(validTwo.accepted());
    // Second media name begins at 4+1+2 + 2+9+4+9 + 2 = 33.
    QByteArray swappedToDuplicate = validTwo.bytes;
    swappedToDuplicate.replace(33, 9, "text/html");
    const DecodedValue duplicateRejected = decodeValue(swappedToDuplicate);
    QCOMPARE(duplicateRejected.error, ClipboardError::DuplicateFormat);
    QVERIFY(duplicateRejected.value.formats.isEmpty());

    // A framing-perfect multi-format value may not copy the payload that
    // crosses the aggregate ceiling. The truncated companion proves the
    // aggregate preflight happens before attempting that hostile read: the
    // deterministic error remains OversizedValue rather than MalformedData.
    const QByteArray firstPayload(kMaxItemPayloadBytes / 2 + 1, 'a');
    const QByteArray secondPayload(kMaxItemPayloadBytes / 2, 'b');
    QByteArray aggregateOverflow("QCBV", 4);
    aggregateOverflow.append(char(1));
    appendLe16(aggregateOverflow, 2);
    appendEncodedFormat(aggregateOverflow, ClipboardTest::textFormat(), firstPayload);
    appendEncodedFormat(aggregateOverflow, ClipboardTest::uriFormat(), secondPayload);
    const DecodedValue aggregateRejected = decodeValue(aggregateOverflow);
    QCOMPARE(aggregateRejected.error, ClipboardError::OversizedValue);
    QVERIFY(aggregateRejected.value.formats.isEmpty());
    QByteArray aggregateBeforeCopy = aggregateOverflow;
    aggregateBeforeCopy.chop(secondPayload.size());
    const DecodedValue aggregateTruncatedRejected = decodeValue(aggregateBeforeCopy);
    QCOMPARE(aggregateTruncatedRejected.error, ClipboardError::OversizedValue);
    QVERIFY(aggregateTruncatedRejected.value.formats.isEmpty());
}

void ClipboardCodecTests::descriptorRoundTrips()
{
    ClipboardHistoryModel model = enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), model.generation(),
                        QStringLiteral("fixture-source-b"), 7)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixturePngLike(), model.generation(),
                        QStringLiteral("fixture-source-p"), 8)
                .accepted());
    QVERIFY(model.setPinned(model.snapshot().entries.first().id, true, model.generation())
                .accepted());
    const HistorySnapshot snapshot = model.snapshot();
    QVERIFY(!snapshot.entries.isEmpty());

    for (const ClipboardEntryDescriptor &entry : snapshot.entries) {
        const EncodedDescriptor encoded = encodeDescriptor(entry);
        QVERIFY2(encoded.accepted(), "descriptor encode of a model entry must succeed");
        const DecodedDescriptor decoded = decodeDescriptor(encoded.bytes);
        QVERIFY(decoded.accepted());
        QCOMPARE(decoded.descriptor, entry);
        QCOMPARE(encodeDescriptor(decoded.descriptor).bytes, encoded.bytes);
    }
}

void ClipboardCodecTests::descriptorDecodeRejectsHostileInput()
{
    ClipboardHistoryModel model = enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), model.generation(),
                        QStringLiteral("fixture-source"), 1)
                .accepted());
    const EncodedDescriptor encoded =
        encodeDescriptor(model.snapshot().entries.first());
    QVERIFY(encoded.accepted());

    QCOMPARE(decodeDescriptor(QByteArray()).error, ClipboardError::MalformedData);
    QCOMPARE(decodeDescriptor(withFirstByteFlipped(encoded.bytes)).error,
             ClipboardError::MalformedData);
    QCOMPARE(decodeDescriptor(truncated(encoded.bytes, 10)).error,
             ClipboardError::MalformedData);

    QByteArray badVersion = encoded.bytes;
    badVersion[4] = 9;
    QCOMPARE(decodeDescriptor(badVersion).error, ClipboardError::UnsupportedVersion);

    // Unknown flag bits are refused so future extensions cannot be silently
    // interpreted by an older decoder.
    QByteArray unknownFlags = encoded.bytes;
    const qsizetype flagsOffset = 4 + 1 + 4 + 4 + 8 + 8;
    const qsizetype metadataOffset = flagsOffset + 1;
    const qsizetype sourceLength = static_cast<qsizetype>(readLe16(encoded.bytes,
                                                                   metadataOffset));
    const qsizetype sourceOffset = metadataOffset + 2;
    const qsizetype previewLengthOffset = sourceOffset + sourceLength;
    const qsizetype previewLength = static_cast<qsizetype>(readLe16(encoded.bytes,
                                                                    previewLengthOffset));
    const qsizetype previewOffset = previewLengthOffset + 2;
    const qsizetype formatCountOffset = previewOffset + previewLength;
    QVERIFY(sourceLength > 0);
    QVERIFY(previewLength > 0);
    unknownFlags[flagsOffset] = char(0x80);
    QCOMPARE(decodeDescriptor(unknownFlags).error, ClipboardError::MalformedData);

    const DecodedDescriptor fiveByteDescriptor = decodeDescriptor(encoded.bytes.left(5));
    QCOMPARE(fiveByteDescriptor.error, ClipboardError::MalformedData);
    QVERIFY(!fiveByteDescriptor.accepted());
    const DecodedDescriptor noFormatCount =
        decodeDescriptor(encoded.bytes.left(formatCountOffset));
    QCOMPARE(noFormatCount.error, ClipboardError::MalformedData);
    QVERIFY(!noFormatCount.accepted());

    // A zero generation/serial pair is not a valid identity.
    QByteArray zeroId = encoded.bytes;
    zeroId[5] = 0;
    zeroId[6] = 0;
    zeroId[7] = 0;
    zeroId[8] = 0;
    const DecodedDescriptor zeroRejected = decodeDescriptor(zeroId);
    QCOMPARE(zeroRejected.error, ClipboardError::MalformedData);

    QCOMPARE(decodeDescriptor(withAppendedByte(encoded.bytes)).error,
             ClipboardError::MalformedData);

    // Missing or oversized fingerprints are rejected on encode.
    ClipboardEntryDescriptor broken = model.snapshot().entries.first();
    broken.fingerprint = broken.fingerprint.left(16);
    QCOMPARE(encodeDescriptor(broken).error, ClipboardError::MalformedData);
    broken.fingerprint = QByteArray(33, '\x11');
    QCOMPARE(encodeDescriptor(broken).error, ClipboardError::MalformedData);
    broken.fingerprint = model.snapshot().entries.first().fingerprint;
    broken.formats.append(FormatInfo { ClipboardTest::textFormat(), 1 });
    QCOMPARE(encodeDescriptor(broken).error, ClipboardError::DuplicateFormat);

    // A zero-format descriptor is an empty value on both sides.
    ClipboardEntryDescriptor noFormats = model.snapshot().entries.first();
    noFormats.formats.clear();
    QCOMPARE(encodeDescriptor(noFormats).error, ClipboardError::EmptyValue);

    // A format list that only claims zero bytes is equally empty. Pin this on
    // encode and on a framing-perfect decode mutation.
    ClipboardEntryDescriptor allZero = model.snapshot().entries.first();
    for (FormatInfo &format : allZero.formats) {
        format.payloadBytes = 0;
    }
    QCOMPARE(encodeDescriptor(allZero).error, ClipboardError::EmptyValue);
    QByteArray zeroClaim = encoded.bytes;
    const qsizetype onlySizeOffset = zeroClaim.size() - 32 - 4;
    zeroClaim.replace(onlySizeOffset, 4, QByteArray(4, '\0'));
    QCOMPARE(decodeDescriptor(zeroClaim).error, ClipboardError::EmptyValue);

    // Negative claimed bytes are structurally impossible, not merely large.
    ClipboardEntryDescriptor negative = model.snapshot().entries.first();
    negative.formats.first().payloadBytes = -1;
    QCOMPARE(encodeDescriptor(negative).error, ClipboardError::MalformedData);

    // Aggregate claimed bytes above the per-item ceiling are refused even
    // when every individual format claims a legal size.
    ClipboardEntryDescriptor aggregate = model.snapshot().entries.first();
    aggregate.formats.clear();
    aggregate.formats.append(FormatInfo { ClipboardTest::textFormat(),
                                          kMaxItemPayloadBytes / 2 + 1 });
    aggregate.formats.append(FormatInfo { ClipboardTest::uriFormat(),
                                          kMaxItemPayloadBytes / 2 + 1 });
    QCOMPARE(encodeDescriptor(aggregate).error, ClipboardError::OversizedValue);

    // And decode rejects an aggregate claim that is only visible across
    // formats: a legal two-format blob whose last claimed size is patched
    // to exactly the per-item ceiling. The size field sits immediately
    // before the 32-byte fingerprint, so the framing stays perfect and only
    // the aggregate rule can catch it.
    ClipboardEntryDescriptor betaDescriptor;
    betaDescriptor.id = EntryId { 1, 1 };
    betaDescriptor.formats.append(FormatInfo { ClipboardTest::textFormat(), 20 });
    betaDescriptor.formats.append(FormatInfo { ClipboardTest::htmlFormat(), 19 });
    betaDescriptor.fingerprint = QByteArray(32, '\x2f');
    const EncodedDescriptor twoFormats = encodeDescriptor(betaDescriptor);
    QVERIFY(twoFormats.accepted());
    QByteArray patched = twoFormats.bytes;
    const qsizetype lastSizeOffset = patched.size() - 32 - 4;
    patched[lastSizeOffset + 2] = '\x10';
    const DecodedDescriptor aggregateRejected = decodeDescriptor(patched);
    QCOMPARE(aggregateRejected.error, ClipboardError::OversizedValue);

    // Producer metadata must already satisfy the model sanitization
    // contract: control and format characters are refused on both sides.
    ClipboardEntryDescriptor hostileLabel = model.snapshot().entries.first();
    hostileLabel.sourceLabel = QStringLiteral("bad\nlabel");
    QCOMPARE(encodeDescriptor(hostileLabel).error, ClipboardError::MalformedData);
    ClipboardEntryDescriptor hostilePreview = model.snapshot().entries.first();
    hostilePreview.preview = QStringLiteral("bad\bpreview");
    QCOMPARE(encodeDescriptor(hostilePreview).error, ClipboardError::MalformedData);
    const QString loneSurrogate(1, QChar(0xd800));
    QVERIFY(QString::fromUtf8(loneSurrogate.toUtf8()) != loneSurrogate);
    ClipboardEntryDescriptor nonUtf8Label = model.snapshot().entries.first();
    nonUtf8Label.sourceLabel = loneSurrogate;
    QCOMPARE(encodeDescriptor(nonUtf8Label).error, ClipboardError::MalformedData);
    ClipboardEntryDescriptor nonUtf8Preview = model.snapshot().entries.first();
    nonUtf8Preview.preview = loneSurrogate;
    QCOMPARE(encodeDescriptor(nonUtf8Preview).error, ClipboardError::MalformedData);
    ClipboardEntryDescriptor truncatedEmpty = model.snapshot().entries.first();
    truncatedEmpty.preview.clear();
    truncatedEmpty.previewTruncated = true;
    QCOMPARE(encodeDescriptor(truncatedEmpty).error, ClipboardError::MalformedData);

    // Invalid UTF-8 must not be accepted through QString's replacement-
    // character conversion. These same-length mutations leave all framing
    // intact but cannot re-encode to their original bytes.
    QByteArray invalidLabel = encoded.bytes;
    invalidLabel[sourceOffset] = char(0xff);
    const QByteArray invalidLabelBytes = invalidLabel.mid(sourceOffset, sourceLength);
    QVERIFY(QString::fromUtf8(invalidLabelBytes).toUtf8() != invalidLabelBytes);
    QCOMPARE(decodeDescriptor(invalidLabel).error, ClipboardError::MalformedData);

    QByteArray invalidPreview = encoded.bytes;
    invalidPreview[previewOffset] = char(0xff);
    const QByteArray invalidPreviewBytes = invalidPreview.mid(previewOffset, previewLength);
    QVERIFY(QString::fromUtf8(invalidPreviewBytes).toUtf8() != invalidPreviewBytes);
    QCOMPARE(decodeDescriptor(invalidPreview).error, ClipboardError::MalformedData);
}

void ClipboardCodecTests::descriptorListRoundTripsAndRejectsHostileInput()
{
    ClipboardHistoryModel model = enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureAlpha(), model.generation(),
                        QStringLiteral("s-a"), 1)
                .accepted());
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), model.generation(), QStringLiteral("s-b"),
                        2)
                .accepted());
    const HistorySnapshot snapshot = model.snapshot();

    const EncodedDescriptorList encoded = encodeDescriptorList(snapshot.entries);
    QVERIFY(encoded.accepted());
    const DecodedDescriptorList decoded = decodeDescriptorList(encoded.bytes);
    QVERIFY(decoded.accepted());
    QCOMPARE(decoded.descriptors, snapshot.entries);
    QCOMPARE(encodeDescriptorList(decoded.descriptors).bytes, encoded.bytes);

    // Empty list is a valid form.
    const EncodedDescriptorList empty = encodeDescriptorList({});
    QVERIFY(empty.accepted());
    QVERIFY(decodeDescriptorList(empty.bytes).accepted());

    QByteArray truncatedCount("QCDL", 4);
    truncatedCount.append(char(1));
    const DecodedDescriptorList truncatedCountRejected =
        decodeDescriptorList(truncatedCount);
    QCOMPARE(truncatedCountRejected.error, ClipboardError::MalformedData);
    QVERIFY(!truncatedCountRejected.accepted());
    QVERIFY(truncatedCountRejected.descriptors.isEmpty());

    // Entry-count overflow is TooManyEntries, not the format error.
    QList<ClipboardEntryDescriptor> flood;
    flood.reserve(kMaxEntries + 1);
    for (int i = 0; i <= kMaxEntries; ++i) {
        ClipboardEntryDescriptor entry;
        entry.id = EntryId { 1, static_cast<quint32>(i + 1) };
        entry.formats.append(FormatInfo { ClipboardTest::textFormat(), 1 });
        entry.fingerprint = QByteArray(32, static_cast<char>(i));
        flood.append(entry);
    }
    QCOMPARE(flood.size(), kMaxEntries + 1);
    QCOMPARE(encodeDescriptorList(flood).error, ClipboardError::TooManyEntries);

    QCOMPARE(decodeDescriptorList(QByteArray()).error, ClipboardError::MalformedData);
    const DecodedDescriptorList trailingRejected =
        decodeDescriptorList(withAppendedByte(encoded.bytes));
    QCOMPARE(trailingRejected.error, ClipboardError::MalformedData);
    QVERIFY(trailingRejected.descriptors.isEmpty());
    QCOMPARE(decodeDescriptorList(truncated(encoded.bytes, 8)).error,
             ClipboardError::MalformedData);
    // Flipping a magic byte anywhere in the list must fail the whole form.
    QCOMPARE(decodeDescriptorList(withFirstByteFlipped(encoded.bytes)).error,
             ClipboardError::MalformedData);
}

void ClipboardCodecTests::encodedFormsNeverCarryPayloadMetadataMismatch()
{
    // The descriptor form is metadata-only. The bounded text/plain preview
    // excerpt is intentionally present; complete payloads of every other
    // format must never be, and the encoded size must stay far below the
    // payload bound even at maximum format count.
    ClipboardHistoryModel model = enabledModel();
    QVERIFY(model.admit(ClipboardTest::fixtureBeta(), model.generation(),
                        QStringLiteral("fixture-source"), 1)
                .accepted());
    const EncodedDescriptor encoded =
        encodeDescriptor(model.snapshot().entries.first());
    QVERIFY(encoded.accepted());
    QVERIFY(encoded.bytes.size() < 512);
    // The 96-code-unit preview bound keeps any single format's full payload
    // out: "fixture beta payload" is present only as the bounded preview,
    // while the complete html payload is absent entirely.
    QVERIFY(!encoded.bytes.contains(QByteArrayLiteral("<p>fixture beta</p>")));
    QVERIFY(!encoded.bytes.contains(QByteArrayLiteral("PNG\r\n")));
}

QTEST_MAIN(ClipboardCodecTests)
#include "tst_clipboard_codec.moc"
