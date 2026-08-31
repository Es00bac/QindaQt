// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>

#include <QtTest>

#include <optional>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::TestData;

namespace {

class AdmissionTransport final : public NetworkTransport {
  Q_OBJECT

public:
  struct OperationCall final {
    quint64 token = 0;
    QString owner;
  };

  bool start(QString *error = nullptr) override {
    Q_UNUSED(error);
    m_running = true;
    return true;
  }

  void stop() override { m_running = false; }

  void requestSnapshot(const quint64 token, const QString &owner) override {
    snapshotRequests.append(qMakePair(token, owner));
    if (m_running && automaticSnapshot.has_value()) {
      Q_EMIT snapshotReceived(token, owner, *automaticSnapshot);
    }
  }

  void requestOperation(const quint64 token, const QString &owner,
                        quint64 epoch, quint64 revision, OperationKind kind,
                        const QVariantMap &parameters) override {
    Q_UNUSED(epoch);
    Q_UNUSED(revision);
    Q_UNUSED(kind);
    Q_UNUSED(parameters);
    operations.append({token, owner});
  }

  void setAutomaticSnapshot(const Snapshot &snapshot) {
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    automaticSnapshot = encoded.payload;
  }

  void clearAutomaticSnapshot() { automaticSnapshot.reset(); }
  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void invalidate(const QString &owner) { Q_EMIT snapshotInvalidated(owner); }

  void finishOperation(const OperationResult &result) {
    Q_ASSERT(!operations.isEmpty());
    const EncodeResult encoded = encodeOperationResult(result);
    Q_ASSERT(encoded.succeeded());
    const OperationCall &call = operations.constLast();
    Q_EMIT operationReceived(call.token, call.owner, encoded.payload);
  }

  void finishLatestSnapshot(const Snapshot &snapshot) {
    Q_ASSERT(!snapshotRequests.isEmpty());
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    const auto &request = snapshotRequests.constLast();
    Q_EMIT snapshotReceived(request.first, request.second, encoded.payload);
  }

  QList<QPair<quint64, QString>> snapshotRequests;
  QList<OperationCall> operations;
  std::optional<QByteArray> automaticSnapshot;

private:
  bool m_running = false;
};

ClientTiming fastTiming() {
  ClientTiming timing;
  timing.requestTimeoutMilliseconds = 100;
  timing.retryMilliseconds = {100};
  return timing;
}

} // namespace

class NetworkClientAdmissionTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void closesAdmissionUntilAuthoritativeRefreshSettles();
};

void NetworkClientAdmissionTest::
    closesAdmissionUntilAuthoritativeRefreshSettles() {
  AdmissionTransport transport;
  transport.setAutomaticSnapshot(validSnapshot());
  NetworkClient client(transport, [] { return qint64(1'000); }, fastTiming());
  QVERIFY(client.start());
  transport.announceOwner(QStringLiteral(":1.23"));
  QTRY_VERIFY_WITH_TIMEOUT(client.operationAdmissionReady(), 1'000);
  QSignalSpy admissionSpy(&client, &NetworkClient::operationAdmissionChanged);

  transport.clearAutomaticSnapshot();
  QVERIFY(client.requestScan(30'000));
  QVERIFY(!client.operationAdmissionReady());
  transport.finishOperation(validOperationResult(
      OperationKind::RequestScan, OperationStatus::Succeeded));
  QTRY_COMPARE_WITH_TIMEOUT(transport.snapshotRequests.size(), 2, 1'000);
  QVERIFY(client.state() == ClientState::Ready);
  QVERIFY(!client.operationInFlight());
  QVERIFY(!client.operationAdmissionReady());

  QString error;
  const qsizetype operationsBefore = transport.operations.size();
  QVERIFY(!client.connectKnownNetwork(cafeNetwork().id, &error));
  QCOMPARE(error, QStringLiteral("client-not-ready"));
  QCOMPARE(transport.operations.size(), operationsBefore);

  Snapshot refreshed = validSnapshot();
  refreshed.revision = 8;
  transport.finishLatestSnapshot(refreshed);
  QTRY_VERIFY_WITH_TIMEOUT(client.operationAdmissionReady(), 1'000);

  transport.clearAutomaticSnapshot();
  transport.invalidate(QStringLiteral(":1.23"));
  QVERIFY(!client.operationAdmissionReady());
  QTRY_COMPARE_WITH_TIMEOUT(transport.snapshotRequests.size(), 3, 1'000);
  QVERIFY(!client.operationAdmissionReady());
  refreshed.revision = 9;
  transport.finishLatestSnapshot(refreshed);
  QTRY_VERIFY_WITH_TIMEOUT(client.operationAdmissionReady(), 1'000);
  QVERIFY(admissionSpy.size() >= 4);
}

QTEST_MAIN(NetworkClientAdmissionTest)
#include "tst_network_client_admission.moc"
