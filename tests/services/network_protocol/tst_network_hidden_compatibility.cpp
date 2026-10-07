// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_validation.h>
using namespace QindaQt::Network;
class HiddenCompatibilityTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void oldConnectGolden();
};
void HiddenCompatibilityTests::oldConnectGolden() {
  const OperationResult result{OperationKind::ConnectKnownNetwork,
                               OperationStatus::Succeeded,
                               99,
                               2,
                               {},
                               {}};
  const QByteArray golden =
      QByteArray::fromHex("514e315200000001000000010000000000000000000000630000"
                          "0000000000020000000000000000");
  QCOMPARE(encodeOperationResult(result).payload, golden);
  OperationResult old;
  QVERIFY(decodeOperationResult(golden, old).succeeded());
  QCOMPARE(old, result);
  // Strict old domains remain closed: optional method does not add kind5.
  OperationResult unknown = result;
  unknown.kind = OperationKind(5);
  QVERIFY(!validateOperationResult(unknown).accepted);
}
QTEST_GUILESS_MAIN(HiddenCompatibilityTests)
#include "tst_network_hidden_compatibility.moc"
