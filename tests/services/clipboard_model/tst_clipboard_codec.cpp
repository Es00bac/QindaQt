// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_codec.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include "support/clipboard_test_data.h"

#include <QtTest>

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

} // namespace

class ClipboardCodecTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void valueRoundTrips();
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

    // Declared media length beyond the canonical bound.
    QByteArray longMedia = encoded.bytes;
    longMedia[7] = static_cast<char>(0xff);
    longMedia[8] = static_cast<char>(0x7f); // 32767 claimed, 9 present
    QCOMPARE(decodeValue(longMedia).error, ClipboardError::MediaTypeRejected);

    // Trailing byte after a complete canonical form.
    QCOMPARE(decodeValue(withAppendedByte(encoded.bytes)).error, ClipboardError::MalformedData);

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
    unknownFlags[flagsOffset] = char(0x80);
    QCOMPARE(decodeDescriptor(unknownFlags).error, ClipboardError::MalformedData);

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

    QCOMPARE(decodeDescriptorList(QByteArray()).error, ClipboardError::MalformedData);
    QCOMPARE(decodeDescriptorList(withAppendedByte(encoded.bytes)).error,
             ClipboardError::MalformedData);
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
