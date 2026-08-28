// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_protocol_test_data.h"

#include <qindaqt/services/power_protocol/power_codec.h>

#include <QtTest>

using namespace QindaQt::Power;

class PowerProtocolCodecTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void snapshotEncodingIsStableAndRoundTrips();
  void operationResultRoundTrips();
  void hostileSnapshotNeverReplacesDestination();
  void rejectsOversizedAndInvalidValuesBeforeEncoding();
  void identityReasonsAndBindingRoundTrip_data();
  void identityReasonsAndBindingRoundTrip();
};

void PowerProtocolCodecTests::snapshotEncodingIsStableAndRoundTrips() {
  const Snapshot expected = TestData::validSnapshot();
  const EncodeResult first = encodeSnapshot(expected);
  const EncodeResult second = encodeSnapshot(expected);
  QVERIFY2(first.succeeded(), qPrintable(first.reasonCode));
  QCOMPARE(first.payload, second.payload);
  QCOMPARE(first.payload.left(12).toHex(),
           QByteArray("515031530000000100000001"));

  Snapshot decoded;
  const DecodeResult result = decodeSnapshot(first.payload, decoded);
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(decoded, expected);
}

void PowerProtocolCodecTests::operationResultRoundTrips() {
  const OperationResult expected = TestData::validOperationResult();
  const EncodeResult encoded = encodeOperationResult(expected);
  QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));
  QCOMPARE(encoded.payload.left(8).toHex(), QByteArray("5150315200000001"));
  OperationResult decoded;
  const DecodeResult result = decodeOperationResult(encoded.payload, decoded);
  QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
  QCOMPARE(decoded, expected);

  OperationResult retained = expected;
  retained.observedRevision = 99;
  const OperationResult before = retained;
  QCOMPARE(
      decodeOperationResult(encoded.payload + QByteArray("trailing"), retained)
          .error,
      CodecError::InvalidValue);
  QCOMPARE(retained, before);
}

void PowerProtocolCodecTests::hostileSnapshotNeverReplacesDestination() {
  const Snapshot retained = TestData::validSnapshot();
  const QByteArray canonical = encodeSnapshot(retained).payload;
  const QList<QByteArray> hostile = {
      QByteArray{},
      canonical.left(canonical.size() - 1),
      QByteArray("BAD!") + canonical.mid(4),
      canonical + QByteArray("trailing"),
  };
  for (const QByteArray &payload : hostile) {
    Snapshot destination = retained;
    destination.revision = 99;
    const Snapshot before = destination;
    QVERIFY(!decodeSnapshot(payload, destination).succeeded());
    QCOMPARE(destination, before);
  }

  QByteArray oversized(kMaxSerializedBytes + 1, 'x');
  Snapshot destination = retained;
  const Snapshot before = destination;
  QCOMPARE(decodeSnapshot(oversized, destination).error,
           CodecError::PayloadTooLarge);
  QCOMPARE(destination, before);
}

void PowerProtocolCodecTests::rejectsOversizedAndInvalidValuesBeforeEncoding() {
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.reasonCode = QString(kMaxReasonCodeUtf8Bytes + 1, QLatin1Char('x'));
  QCOMPARE(encodeSnapshot(snapshot).error, CodecError::InvalidValue);

  snapshot = TestData::validSnapshot();
  snapshot.supplies[0].handle.epoch++;
  QCOMPARE(encodeSnapshot(snapshot).error, CodecError::InvalidValue);

  OperationResult result = TestData::validOperationResult();
  result.status = static_cast<OperationStatus>(99);
  QCOMPARE(encodeOperationResult(result).error, CodecError::InvalidValue);
}

void PowerProtocolCodecTests::identityReasonsAndBindingRoundTrip_data() {
  QTest::addColumn<BacklightReason>("reason");
  QTest::newRow("no-backlight") << BacklightReason::NoBacklight;
  QTest::newRow("ambiguous-backlight") << BacklightReason::AmbiguousBacklight;
  QTest::newRow("no-internal-connector")
      << BacklightReason::NoInternalConnector;
  QTest::newRow("ambiguous-internal-topology")
      << BacklightReason::AmbiguousInternalTopology;
}

void PowerProtocolCodecTests::identityReasonsAndBindingRoundTrip() {
  QFETCH(BacklightReason, reason);
  Snapshot snapshot = TestData::validSnapshot();
  snapshot.internalBacklights[0].status = BacklightStatus::Unavailable;
  snapshot.internalBacklights[0].reason = reason;
  const EncodeResult encoded = encodeSnapshot(snapshot);
  QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));
  Snapshot decoded;
  QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded());
  QCOMPARE(decoded.internalBacklights[0].reason, reason);
  QCOMPARE(decoded.waylandBinding, snapshot.waylandBinding);
}

QTEST_GUILESS_MAIN(PowerProtocolCodecTests)
#include "tst_power_protocol_codec.moc"
