// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_protocol_test_support.h"
#include <QtTest>
using namespace MediaTest;
class MediaHostileTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void malformedSnapshot_data() {
    QTest::addColumn<QByteArray>("bytes"); QTest::addColumn<int>("error");
    const auto valid = encodeSnapshot(snapshot()).payload;
    const auto o = firstRow(valid);
    auto add = [&](const char *name, QByteArray bytes, CodecError error) {
      QTest::newRow(name) << bytes << static_cast<int>(error);
    };
    auto patch = [&](const char *name, qsizetype offset, quint32 value, CodecError error) {
      auto bytes = valid; put32(bytes, offset, value); add(name, bytes, error);
    };
    auto bytes = valid; bytes[0] = 'X'; add("magic", bytes, CodecError::InvalidMagic);
    patch("codec-version", 4, 99, CodecError::UnsupportedCodecVersion);
    patch("message-kind", 8, 99, CodecError::WrongMessageKind);
    patch("protocol-version", 12, 99, CodecError::InvalidValue);
    patch("availability", snapshotCount(valid) - 4, 99, CodecError::InvalidValue);
    patch("row-allocation-count", snapshotCount(valid), 0xffffffffU, CodecError::PayloadTooLarge);
    patch("mount-allocation-count", o.rootsCount, 0xffffffffU, CodecError::PayloadTooLarge);
    patch("label-allocation-length", o.label - 4, 0xffffffffU, CodecError::PayloadTooLarge);
    bytes = valid; put32(bytes, o.label - 4, 256); bytes.truncate(o.label + 2);
    add("legal-length-but-truncated", bytes, CodecError::Truncated);
    for (const auto pair : {std::pair{"mount-state", o.mountState}, std::pair{"read-only", o.readOnly},
         std::pair{"disabled-reason", o.reason}, std::pair{"progress", o.progress},
         std::pair{"outcome", o.outcome}, std::pair{"diagnostic", o.diagnostic}})
      patch(pair.first, pair.second, 99, CodecError::InvalidValue);
    for (const auto pair : {std::pair{"encrypted-bool", o.encrypted}, std::pair{"locked-bool", o.locked},
         std::pair{"optical-bool", o.optical}, std::pair{"admission-bool", o.enabled}}) {
      bytes = valid; bytes[pair.second] = char(2); add(pair.first, bytes, CodecError::InvalidBoolean);
    }
    bytes = valid; bytes[o.enabled] = char(1); add("enabled-with-denial", bytes, CodecError::InvalidValue);
    bytes = valid; bytes[o.label] = char(0xc3); bytes[o.label + 1] = '(';
    add("invalid-utf8", bytes, CodecError::InvalidUtf8);
    bytes = valid; bytes[o.label + snapshot().rows[0].displayName.toUtf8().size() - 1] = char(0xc3);
    add("unfinished-utf8", bytes, CodecError::InvalidUtf8);
    bytes = valid; bytes[o.label] = char(0); add("embedded-nul", bytes, CodecError::InvalidUtf8);
    bytes = valid; bytes.append('x'); add("trailing-bytes", bytes, CodecError::TrailingBytes);
    Cursor c{valid, snapshotCount(valid) + 4}; const auto first = offsets(c); const auto second = offsets(c);
    bytes = valid; bytes[c.position] = char(2); add("pending-bool", bytes, CodecError::InvalidBoolean);
    bytes = valid; bytes.replace(second.volume, 8, valid.mid(first.volume, 8));
    add("duplicate-volume-id", bytes, CodecError::InvalidValue);
    bytes = valid; bytes.replace(second.attachment, 8, valid.mid(first.attachment, 8));
    add("duplicate-handle", bytes, CodecError::InvalidValue);
    bytes = valid; bytes[first.drive] = '/'; add("path-as-opaque-id", bytes, CodecError::InvalidValue);
    bytes = valid; bytes[20] = 'a'; add("non-unique-owner", bytes, CodecError::InvalidValue);
    add("oversized-envelope", QByteArray(kMaxSnapshotBytes + 1, 'x'), CodecError::PayloadTooLarge);
  }
  void malformedSnapshot() {
    QFETCH(QByteArray, bytes); QFETCH(int, error);
    const auto sentinel = snapshot(); auto destination = sentinel;
    const auto decoded = decodeSnapshot(bytes, destination);
    QCOMPARE(static_cast<int>(decoded.error), error); QVERIFY(destination == sentinel);
  }
  void everyTruncatedPrefixPreservesEveryDestination() {
    auto check = []<class Value>(const EncodeResult &encoded, Value value, auto decode) {
      QVERIFY(encoded.succeeded()); const auto sentinel = value;
      for (qsizetype end = 0; end < encoded.payload.size(); ++end) {
        const auto failure = decode(QByteArrayView(encoded.payload).first(end), value);
        QVERIFY(!failure.succeeded()); QVERIFY(value == sentinel);
      }
    };
    check(encodeSnapshot(snapshot()), snapshot(), decodeSnapshot);
    check(encodeActionRequest(request()), request(), decodeActionRequest);
    check(encodeActionAdmission(admission()), admission(), decodeActionAdmission);
    check(encodeOperationResult(result()), result(), decodeOperationResult);
  }
  void actionResultAndAdmissionRejectUnknownFields() {
    auto bytes = encodeActionRequest(request()).payload;
    put32(bytes, requestAction(bytes), 99);
    auto requestValue = request(); const auto oldRequest = requestValue;
    QCOMPARE(decodeActionRequest(bytes, requestValue).valueError, ValueError::InvalidEnum);
    QVERIFY(requestValue == oldRequest);
    bytes = encodeActionAdmission(admission()).payload;
    put32(bytes, requestAction(bytes) + 4, 99);
    auto admissionValue = admission(); const auto oldAdmission = admissionValue;
    QCOMPARE(decodeActionAdmission(bytes, admissionValue).valueError, ValueError::InvalidEnum);
    QVERIFY(admissionValue == oldAdmission);
    bytes = encodeOperationResult(result()).payload;
    Cursor c{bytes, requestAction(bytes) + 4}; c.text(); const auto status = c.position;
    auto resultValue = result(); const auto oldResult = resultValue;
    put32(bytes, status, 99);
    QCOMPARE(decodeOperationResult(bytes, resultValue).valueError, ValueError::InvalidEnum);
    QVERIFY(resultValue == oldResult);
    bytes = encodeOperationResult(result()).payload;
    bytes[bytes.size() - 4 - 8 - 1] = char(2);
    QCOMPARE(decodeOperationResult(bytes, resultValue).error, CodecError::InvalidBoolean);
    QVERIFY(resultValue == oldResult);
    bytes = encodeOperationResult(result()).payload; put32(bytes, bytes.size() - 4, 99);
    QCOMPARE(decodeOperationResult(bytes, resultValue).valueError, ValueError::InvalidEnum);
    QVERIFY(resultValue == oldResult);
    const QByteArray oversized(kMaxActionEnvelopeBytes + 1, 'x');
    QCOMPARE(decodeActionRequest(oversized, requestValue).error, CodecError::PayloadTooLarge);
    QCOMPARE(decodeActionAdmission(oversized, admissionValue).error, CodecError::PayloadTooLarge);
    QCOMPARE(decodeOperationResult(oversized, resultValue).error, CodecError::PayloadTooLarge);
    QVERIFY(requestValue == oldRequest && admissionValue == oldAdmission && resultValue == oldResult);
  }
  void envelopesCannotMasqueradeAsAnotherMessageKind() {
    const QList<QByteArray> payloads{encodeSnapshot(snapshot()).payload,
        encodeActionRequest(request()).payload, encodeActionAdmission(admission()).payload,
        encodeOperationResult(result()).payload};
    for (qsizetype i = 0; i < payloads.size(); ++i) {
      auto s = snapshot(); auto q = request(); auto a = admission(); auto o = result();
      if (i != 0) QCOMPARE(decodeSnapshot(payloads[i], s).error, CodecError::WrongMessageKind);
      if (i != 1) QCOMPARE(decodeActionRequest(payloads[i], q).error, CodecError::WrongMessageKind);
      if (i != 2) QCOMPARE(decodeActionAdmission(payloads[i], a).error, CodecError::WrongMessageKind);
      if (i != 3) QCOMPARE(decodeOperationResult(payloads[i], o).error, CodecError::WrongMessageKind);
      QVERIFY(s == snapshot() && q == request() && a == admission() && o == result());
    }
  }
  void oversizedDistinctDriveInventoryIsNotTruncated() {
    auto v = snapshot(); v.rows.clear();
    for (qsizetype i = 0; i <= kMaxDrives; ++i) {
      auto volume = row(QString::number(i));
      volume.driveDisplayId = QStringLiteral("drive-") + QString::number(i % kMaxDrives).rightJustified(2, '0');
      v.rows.append(volume);
    }
    auto bytes = encodeSnapshot(v).payload; QVERIFY(!bytes.isEmpty());
    Cursor c{bytes, snapshotCount(bytes) + 4}; RowOffsets last{};
    for (qsizetype i = 0; i < v.rows.size(); ++i) last = offsets(c);
    bytes.replace(last.drive, 8, QByteArray("drive-32"));
    auto destination = snapshot(); const auto sentinel = destination;
    QCOMPARE(decodeSnapshot(bytes, destination).valueError, ValueError::LimitExceeded);
    QVERIFY(destination == sentinel);
  }
};
QTEST_GUILESS_MAIN(MediaHostileTest)
#include "tst_media_hostile.moc"
