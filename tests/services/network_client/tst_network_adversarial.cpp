// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <QtTest>

#include <limits>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::TestData;

namespace {

class HostileTransport final : public NetworkTransport {
  Q_OBJECT
public:
  bool start(QString *error = nullptr) override {
    ++startCalls;
    if (startFails) {
      if (error != nullptr) {
        *error = QStringLiteral("password=\"start secret\" failed");
      }
      return false;
    }
    running = true;
    return true;
  }
  void stop() override { running = false; }
  void requestSnapshot(quint64 token, const QString &owner) override {
    if (running && !payload.isEmpty()) {
      Q_EMIT snapshotReceived(token, owner, payload);
    }
  }
  void requestOperation(quint64, const QString &, quint64, quint64,
                        OperationKind, const QVariantMap &) override {}

  void setSnapshot(const Snapshot &snapshot) {
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    payload = encoded.payload;
  }
  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }

  QByteArray payload;
  int startCalls = 0;
  bool startFails = false;
  bool running = false;
};

ClientTiming hostileTiming() {
  ClientTiming timing;
  timing.requestTimeoutMilliseconds = 500;
  timing.retryMilliseconds = {500};
  return timing;
}

} // namespace

class NetworkAdversarialTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rejectsPayloadOwnerMismatch();
  void preservesEpochHighWaterAcrossOwnerCycle();
  void rejectsUnboundedScanLease();
  void enforcesDiagnosticByteCap();
  void redactsQuotedCredentialValues();
  void hidesUnicodeFormattingControls();
  void rejectsWireInvalidValues();
  void retriesTransportStartAfterFailure();
};

void NetworkAdversarialTests::rejectsPayloadOwnerMismatch() {
  HostileTransport transport;
  Snapshot payload = validSnapshot();
  payload.owner = QStringLiteral(":1.payload-b");
  payload.epoch = 50;
  payload.revision = 1;
  transport.setSnapshot(payload);
  NetworkClient client(transport, [] { return qint64(1'000); },
                       hostileTiming());
  QVERIFY(client.start());
  transport.announceOwner(QStringLiteral(":1.signal-a"));
  QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Degraded, 1'000);
  QVERIFY(!client.model().snapshot().has_value());
}

void NetworkAdversarialTests::preservesEpochHighWaterAcrossOwnerCycle() {
  HostileTransport transport;
  Snapshot ownerA = validSnapshot();
  ownerA.owner = QStringLiteral(":1.a");
  transport.setSnapshot(ownerA);
  NetworkClient client(transport, [] { return qint64(1'000); },
                       hostileTiming());
  QVERIFY(client.start());
  transport.announceOwner(ownerA.owner);
  QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Ready, 1'000);

  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.b");
  ownerB.epoch = 42;
  ownerB.revision = 1;
  transport.setSnapshot(ownerB);
  transport.announceOwner(ownerB.owner);
  QTRY_VERIFY_WITH_TIMEOUT(client.model().lineage().has_value()
                               && client.model().lineage()->owner == ownerB.owner,
                           1'000);

  ownerA.revision = 500;
  transport.setSnapshot(ownerA);
  transport.announceOwner(ownerA.owner);
  QTRY_VERIFY_WITH_TIMEOUT(client.state() == ClientState::Degraded, 1'000);
  QVERIFY(!client.model().lineage().has_value());
  QCOMPARE(client.model().lineageHighWater()->epoch, quint64(42));
}

void NetworkAdversarialTests::rejectsUnboundedScanLease() {
  Snapshot leased = leasedSnapshot(QStringLiteral("lease-forever"));
  leased.scanLease.durationMilliseconds =
      std::numeric_limits<qint64>::max();
  QVERIFY(!validateSnapshot(leased).accepted);
}

void NetworkAdversarialTests::enforcesDiagnosticByteCap() {
  const QString output =
      redactDiagnostic(QString(kMaxDiagnosticUtf8Bytes * 2, u'x'));
  QVERIFY(output.toUtf8().size() <= kMaxDiagnosticUtf8Bytes);
}

void NetworkAdversarialTests::redactsQuotedCredentialValues() {
  const QString output = redactDiagnostic(
      QStringLiteral("connect failed: password=\"hunter 2\" ssid=Home"));
  QVERIFY(!output.contains(QStringLiteral("hunter")));
  QVERIFY(!output.contains(QStringLiteral(" 2\"")));
}

void NetworkAdversarialTests::hidesUnicodeFormattingControls() {
  const QByteArray bidi = QByteArray::fromHex("e280ae") + QByteArray("spoof");
  const SsidIdentity identity = normalizeSsid(bidi);
  QVERIFY(identity.valid);
  QVERIFY(identity.hidden);
}

void NetworkAdversarialTests::rejectsWireInvalidValues() {
  Snapshot snapshot = validSnapshot();
  snapshot.wireValid = false;
  QVERIFY(!validateSnapshot(snapshot).accepted);

  OperationResult result = validOperationResult();
  result.wireValid = false;
  QVERIFY(!validateOperationResult(result).accepted);
}

void NetworkAdversarialTests::retriesTransportStartAfterFailure() {
  HostileTransport transport;
  transport.startFails = true;
  NetworkClient client(transport, [] { return qint64(1'000); },
                       hostileTiming());
  QString error;
  QVERIFY(!client.start(&error));
  QVERIFY(!error.contains(QStringLiteral("start secret")));
  transport.startFails = false;
  QVERIFY(client.start());
  QCOMPARE(transport.startCalls, 2);
  QVERIFY(transport.running);
}

QTEST_MAIN(NetworkAdversarialTests)
#include "tst_network_adversarial.moc"
