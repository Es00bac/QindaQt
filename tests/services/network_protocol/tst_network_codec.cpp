// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::TestData;

class NetworkCodecTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void roundTripsValidSnapshot();
  void roundTripsValidOperationResult();
  void producesCanonicalBytes();
  void rejectsInvalidValuesBeforeEncoding();
  void rejectsHostileSnapshotPayloads();
  void rejectsHostileOperationResultPayloads();
  void rejectsNonCanonicalBooleanAtomically();
  void decodeFailureLeavesDestinationUntouched();
};

void NetworkCodecTests::roundTripsValidSnapshot() {
  Snapshot snapshot = validSnapshot();
  snapshot.accessPoints = {AccessPoint{
      QStringLiteral("wlan0"), QStringLiteral("Cafe"), false,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa3Personal, 5'180,
      72}};
  snapshot.activeConnections = {
      ActiveConnection{QStringLiteral("enp3s0"), cafeNetwork().id}};
  snapshot.reasonCode = QStringLiteral("ready");
  snapshot.diagnostic = QStringLiteral("all subsystems nominal");

  const EncodeResult encoded = encodeSnapshot(snapshot);
  QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));

  Snapshot decoded;
  const DecodeResult result = decodeSnapshot(encoded.payload, decoded);
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(decoded, snapshot);
}

void NetworkCodecTests::roundTripsValidOperationResult() {
  const OperationResult value = validOperationResult(
      OperationKind::ConnectKnownNetwork, OperationStatus::Rejected);
  const EncodeResult encoded = encodeOperationResult(value);
  QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));

  OperationResult decoded;
  QVERIFY2(decodeOperationResult(encoded.payload, decoded).succeeded(), "decode");
  QCOMPARE(decoded, value);
}

void NetworkCodecTests::producesCanonicalBytes() {
  const Snapshot first = validSnapshot();
  Snapshot second = validSnapshot();
  QCOMPARE(encodeSnapshot(first).payload, encodeSnapshot(second).payload);

  Snapshot reordered = validSnapshot();
  reordered.radios = {Radio{RadioKind::Wifi, true, true, true}};
  QCOMPARE(encodeSnapshot(reordered).payload, encodeSnapshot(first).payload);
}

void NetworkCodecTests::rejectsInvalidValuesBeforeEncoding() {
  Snapshot invalid = validSnapshot();
  invalid.epoch = 0;
  const EncodeResult rejected = encodeSnapshot(invalid);
  QCOMPARE(rejected.error, CodecError::InvalidValue);
  QVERIFY(!rejected.reasonCode.isEmpty());
  QVERIFY(rejected.payload.isEmpty());

  OperationResult invalidResult = validOperationResult();
  invalidResult.initiatingRevision = 0;
  QCOMPARE(encodeOperationResult(invalidResult).error, CodecError::InvalidValue);

  invalid.wireValid = false;
  QCOMPARE(encodeSnapshot(invalid).error, CodecError::InvalidValue);

  invalidResult = validOperationResult();
  invalidResult.wireValid = false;
  QCOMPARE(encodeOperationResult(invalidResult).error,
           CodecError::InvalidValue);
}

void NetworkCodecTests::rejectsNonCanonicalBooleanAtomically() {
  const Snapshot shape = validSnapshot();
  QByteArray hostile = encodeSnapshot(shape).payload;
  QVERIFY(!hostile.isEmpty());

  qsizetype radioCountOffset = 4 + 4 + 4;
  radioCountOffset += 4 + shape.owner.toUtf8().size();
  radioCountOffset += 8 + 8 + 4 + 4 + 4;
  radioCountOffset += 4 + shape.reasonCode.toUtf8().size();
  radioCountOffset += 4 + shape.diagnostic.toUtf8().size();
  const qsizetype radioPresentOffset = radioCountOffset + 4 + 4;
  QVERIFY(radioPresentOffset < hostile.size());
  hostile[radioPresentOffset] = char(2);

  Snapshot destination = validSnapshot();
  destination.revision = 99;
  const Snapshot before = destination;
  const DecodeResult rejected = decodeSnapshot(hostile, destination);
  QCOMPARE(rejected.error, CodecError::InvalidValue);
  QCOMPARE(destination, before);
}

void NetworkCodecTests::rejectsHostileSnapshotPayloads() {
  const QByteArray valid = encodeSnapshot(validSnapshot()).payload;

  Snapshot decoded;
  QCOMPARE(decodeSnapshot(QByteArray(), decoded).error, CodecError::Truncated);
  QCOMPARE(decodeSnapshot(valid.left(valid.size() - 1), decoded).error,
           CodecError::Truncated);

  QByteArray badMagic = valid;
  badMagic[0] = 'X';
  QCOMPARE(decodeSnapshot(badMagic, decoded).error, CodecError::InvalidMagic);

  QByteArray badCodecVersion = valid;
  badCodecVersion[5] = 0x09;
  QCOMPARE(decodeSnapshot(badCodecVersion, decoded).error,
           CodecError::UnsupportedCodecVersion);

  QByteArray trailing = valid;
  trailing.append('\0');
  QCOMPARE(decodeSnapshot(trailing, decoded).error, CodecError::InvalidValue);

  const QByteArray oversized(kMaxSerializedBytes + 1, 'x');
  QCOMPARE(decodeSnapshot(oversized, decoded).error, CodecError::PayloadTooLarge);

  // A declared list count beyond the cap must be rejected before allocation.
  // The radios count sits after the fixed header and the three bounded texts.
  const Snapshot shape = validSnapshot();
  qsizetype countOffset = 4 + 4 + 4;
  countOffset += 4 + shape.owner.toUtf8().size();
  countOffset += 8 + 8 + 4 + 4 + 4;
  countOffset += 4 + shape.reasonCode.toUtf8().size();
  countOffset += 4 + shape.diagnostic.toUtf8().size();
  QByteArray capped = valid;
  const quint32 hostileCount = 0xFFFF'FFFFU;
  capped[countOffset] = static_cast<char>(hostileCount & 0xFFU);
  capped[countOffset + 1] = static_cast<char>((hostileCount >> 8U) & 0xFFU);
  capped[countOffset + 2] = static_cast<char>((hostileCount >> 16U) & 0xFFU);
  capped[countOffset + 3] = static_cast<char>((hostileCount >> 24U) & 0xFFU);
  QCOMPARE(decodeSnapshot(capped, decoded).error, CodecError::PayloadTooLarge);
}

void NetworkCodecTests::rejectsHostileOperationResultPayloads() {
  const QByteArray valid =
      encodeOperationResult(validOperationResult()).payload;

  OperationResult decoded;
  QCOMPARE(decodeOperationResult(QByteArray(), decoded).error,
           CodecError::Truncated);
  QCOMPARE(decodeOperationResult(valid.left(valid.size() - 2), decoded).error,
           CodecError::Truncated);

  QByteArray badMagic = valid;
  badMagic[1] = 'X';
  QCOMPARE(decodeOperationResult(badMagic, decoded).error,
           CodecError::InvalidMagic);

  QByteArray trailing = valid;
  trailing.append('x');
  QCOMPARE(decodeOperationResult(trailing, decoded).error,
           CodecError::InvalidValue);
}

void NetworkCodecTests::decodeFailureLeavesDestinationUntouched() {
  Snapshot accepted;
  QVERIFY(decodeSnapshot(encodeSnapshot(validSnapshot()).payload, accepted)
              .succeeded());

  // The encoder refuses invalid values, so build a structurally complete
  // payload whose protocol-version field is semantically hostile instead.
  QByteArray semanticallyInvalid = encodeSnapshot(validSnapshot()).payload;
  QVERIFY(!semanticallyInvalid.isEmpty());
  const qsizetype protocolVersionOffset = 4 + 4;  // magic + codec version
  semanticallyInvalid[protocolVersionOffset + 3] = '\x02';

  Snapshot before = accepted;
  QVERIFY(!decodeSnapshot(QByteArray("junk"), accepted).succeeded());
  QCOMPARE(accepted, before);
  QVERIFY(!decodeSnapshot(semanticallyInvalid, accepted).succeeded());
  QCOMPARE(accepted, before);
}

QTEST_MAIN(NetworkCodecTests)
#include "tst_network_codec.moc"
