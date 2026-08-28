// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_protocol/network_codec.h>

#include <QtTest>

#include <memory>
#include <optional>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::TestData;

namespace {

class FakeNetworkTransport final : public NetworkTransport {
  Q_OBJECT

public:
  bool start(QString *error = nullptr) override {
    ++startCalls;
    if (m_startFails) {
      if (error != nullptr) {
        *error = m_startError;
      }
      return false;
    }
    m_running = true;
    return true;
  }
  void stop() override {
    ++stopCalls;
    m_running = false;
  }
  void requestSnapshot(const quint64 token, const QString &owner) override {
    snapshotRequests.append(qMakePair(token, owner));
    if (m_autoSnapshot.has_value() && m_running) {
      Q_EMIT snapshotReceived(token, owner, m_autoSnapshot->second);
    }
  }
  void requestOperation(const quint64 token, const QString &owner,
                        const quint64 epoch, const quint64 revision,
                        const OperationKind kind,
                        const QVariantMap &parameters) override {
    operations.append(
        OperationCall{token, owner, epoch, revision, kind, parameters});
  }

  void setAutoSnapshot(const Snapshot &snapshot) {
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    m_autoSnapshot = qMakePair(snapshot, encoded.payload);
  }
  void clearAutoSnapshot() { m_autoSnapshot.reset(); }
  void emitOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void emitSnapshot(const quint64 token, const QString &owner,
                    const QByteArray &payload) {
    Q_EMIT snapshotReceived(token, owner, payload);
  }
  void emitOperation(const quint64 token, const QString &owner,
                     const QByteArray &payload) {
    Q_EMIT operationReceived(token, owner, payload);
  }
  void emitFailure(const quint64 token, const QString &owner,
                   const QString &errorName) {
    Q_EMIT requestFailed(token, owner, errorName, QStringLiteral("detail"));
  }
  void emitInvalidation(const QString &owner) {
    Q_EMIT snapshotInvalidated(owner);
  }
  void emitBusDisconnected() { Q_EMIT busDisconnected(); }

  struct OperationCall {
    quint64 token = 0;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
    OperationKind kind = OperationKind::RequestScan;
    QVariantMap parameters;
  };
  QList<QPair<quint64, QString>> snapshotRequests;
  QList<OperationCall> operations;
  int startCalls = 0;
  int stopCalls = 0;
  bool m_startFails = false;
  bool m_running = false;
  QString m_startError = QStringLiteral("transport refused");
  std::optional<QPair<Snapshot, QByteArray>> m_autoSnapshot;
};

ClientTiming fastTiming() {
  ClientTiming timing;
  timing.requestTimeoutMilliseconds = 100;
  timing.retryMilliseconds = {100, 100, 100};
  return timing;
}

} // namespace

class NetworkClientTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase() {
    qRegisterMetaType<OperationResult>("QindaQt::Network::OperationResult");
  }
  void reachesReadyBaseline();
  void refetchesAfterInvalidation();
  void toleratesOutOfOrderDuplicateReply();
  void rejectsInvalidSnapshotReply();
  void rejectsPayloadOwnerMismatchAtomically();
  void rejectsMalformedOrOversizedOwnerSignal();
  void replacesOwnerAndRejectsAbaReplay();
  void rejectsRetiredEpochAcrossRealOwnerCycle();
  void timesOutAndNeverReplaysSnapshot();
  void completesOperationAndRefreshes();
  void reportsUncertainOperationOnTimeout();
  void reportsUncertainOperationOnTransportFailure();
  void rejectsLocalIntents();
  void tearsDownCleanlyAndIgnoresLateSignals();
  void reportsTransportLoss();
  void retriesAfterFailedTransportStart();
  void rejectsInvalidTimingConfiguration();

private:
  qint64 m_now = 1'000;
  std::unique_ptr<FakeNetworkTransport> m_transport;
  std::unique_ptr<NetworkClient> makeClient() {
    m_transport = std::make_unique<FakeNetworkTransport>();
    m_transport->setAutoSnapshot(validSnapshot());
    ++m_now;
    auto client = std::make_unique<NetworkClient>(
        *m_transport, Model::MonotonicClock([this] { return m_now; }),
        fastTiming());
    if (!client->start()) {
      return nullptr;
    }
    m_transport->emitOwner(QStringLiteral(":1.23"));
    const bool ready = QTest::qWaitFor(
        [client = client.get()] { return client->state() == ClientState::Ready; },
        2'000);
    if (!ready) {
      return nullptr;
    }
    return client;
  }
};

void NetworkClientTests::reachesReadyBaseline() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QCOMPARE(client->state(), ClientState::Ready);
  QVERIFY(client->model().snapshot().has_value());
  QVERIFY(client->projection().hasSnapshot);
  QCOMPARE(client->projection().owner, QStringLiteral(":1.23"));
  QCOMPARE(m_transport->snapshotRequests.size(), 1);
}

void NetworkClientTests::rejectsMalformedOrOversizedOwnerSignal() {
  std::unique_ptr<NetworkClient> client = makeClient();
  const quint64 highWater = client->model().lineageHighWater()->epoch;

  m_transport->emitOwner(QString(kMaxOwnerUtf8Bytes + 1, u'a'));
  QCOMPARE(client->state(), ClientState::Degraded);
  QVERIFY(!client->model().snapshot().has_value());
  QCOMPARE(client->model().lineageHighWater()->epoch, highWater);
  const qsizetype requestCount = m_transport->snapshotRequests.size();

  m_transport->emitOwner(QStringLiteral("not-a-unique-owner"));
  QCOMPARE(client->state(), ClientState::Degraded);
  QCOMPARE(m_transport->snapshotRequests.size(), requestCount);
  QVERIFY(client->lastError().contains(QStringLiteral("owner is invalid")));
}

void NetworkClientTests::rejectsPayloadOwnerMismatchAtomically() {
  std::unique_ptr<NetworkClient> client = makeClient();
  const Model::ModelState before = client->projection();

  Snapshot foreignPayload = validSnapshot();
  foreignPayload.owner = QStringLiteral(":1.42");
  foreignPayload.epoch = 42;
  foreignPayload.revision = 1;
  m_transport->setAutoSnapshot(foreignPayload);
  client->refresh();
  QTRY_VERIFY_WITH_TIMEOUT(client->state() == ClientState::Degraded, 2'000);
  QCOMPARE(client->projection(), before);
  QCOMPARE(client->model().lineageHighWater()->epoch, quint64(41));
}

void NetworkClientTests::refetchesAfterInvalidation() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QCOMPARE(client->model().lineage()->revision, quint64(7));

  Snapshot updated = validSnapshot();
  updated.revision = 8;
  m_transport->setAutoSnapshot(updated);
  m_transport->emitInvalidation(QStringLiteral(":1.23"));
  QTRY_VERIFY_WITH_TIMEOUT(client->model().lineage()->revision == quint64(8),
                           2'000);

  // Invalidation from a foreign owner is ignored.
  m_transport->emitInvalidation(QStringLiteral(":1.99"));
  QTest::qWait(150);
  QCOMPARE(m_transport->snapshotRequests.size(), 2);
}

void NetworkClientTests::toleratesOutOfOrderDuplicateReply() {
  std::unique_ptr<NetworkClient> client = makeClient();
  const quint64 staleToken = m_transport->snapshotRequests.first().first;
  const QByteArray stalePayload =
      m_transport->m_autoSnapshot->second;

  // A duplicate reply for the already-current revision is benign.
  m_transport->clearAutoSnapshot();
  client->refresh();
  QTRY_COMPARE(m_transport->snapshotRequests.size(), 2);
  const quint64 token = m_transport->snapshotRequests.last().first;
  m_transport->emitSnapshot(token, QStringLiteral(":1.23"), stalePayload);
  QCOMPARE(client->state(), ClientState::Ready);
  QCOMPARE(client->model().lineage()->revision, quint64(7));

  // A reply for a retired token is dropped entirely.
  m_transport->emitSnapshot(staleToken + 500, QStringLiteral(":1.23"),
                            stalePayload);
  QCOMPARE(client->state(), ClientState::Ready);
}

void NetworkClientTests::rejectsInvalidSnapshotReply() {
  std::unique_ptr<NetworkClient> client = makeClient();
  m_transport->clearAutoSnapshot();
  client->refresh();
  QTRY_COMPARE(m_transport->snapshotRequests.size(), 2);
  const quint64 token = m_transport->snapshotRequests.last().first;

  // The encoder refuses invalid values, so deliver a structurally complete
  // payload whose epoch field was zeroed: semantic validation must reject it.
  Snapshot hostile = validSnapshot();
  QByteArray payload = encodeSnapshot(hostile).payload;
  QVERIFY(!payload.isEmpty());
  qsizetype epochOffset = 4 + 4 + 4;  // magic, codec version, protocol version
  epochOffset += 4 + hostile.owner.toUtf8().size();
  for (int index = 0; index < 8; ++index) {
    payload[epochOffset + index] = '\0';
  }
  m_transport->emitSnapshot(token, QStringLiteral(":1.23"), payload);
  QCOMPARE(client->state(), ClientState::Degraded);
  QVERIFY(!client->lastError().isEmpty());
  // The last accepted baseline survives a hostile reply.
  QVERIFY(client->model().snapshot().has_value());
  QCOMPARE(client->model().lineage()->revision, quint64(7));
}

void NetworkClientTests::replacesOwnerAndRejectsAbaReplay() {
  std::unique_ptr<NetworkClient> client = makeClient();

  // Owner B replaces A with a new epoch; the baseline must reset.
  m_transport->setAutoSnapshot([this] {
    Snapshot replacement = validSnapshot();
    replacement.owner = QStringLiteral(":1.42");
    replacement.epoch = 42;
    replacement.revision = 1;
    return replacement;
  }());
  m_transport->emitOwner(QStringLiteral(":1.42"));
  // Owner replacement clears the lineage before the refetch repopulates it,
  // so the poll must guard the empty optional.
  QTRY_VERIFY_WITH_TIMEOUT(
      client->model().lineage().has_value()
          && client->model().lineage()->owner == QStringLiteral(":1.42"),
      2'000);
  QCOMPARE(client->model().lineage()->revision, quint64(1));
  QCOMPARE(client->state(), ClientState::Ready);

  // Delayed A reply replays its retired owner: the transport fence drops it
  // by owner before the model gate ever sees it. B's lineage and Ready state
  // survive untouched; the model-level ABA rejection is covered by the gate
  // and model tests.
  m_transport->clearAutoSnapshot();
  client->refresh();
  QTRY_COMPARE(m_transport->snapshotRequests.size(), 3);
  const quint64 token = m_transport->snapshotRequests.last().first;
  Snapshot replayedA = validSnapshot();
  replayedA.revision = 500;
  m_transport->emitSnapshot(token, QStringLiteral(":1.23"),
                            encodeSnapshot(replayedA).payload);
  QCOMPARE(client->model().lineage()->owner, QStringLiteral(":1.42"));
  QCOMPARE(client->model().lineage()->revision, quint64(1));
  QCOMPARE(client->state(), ClientState::Ready);

  // A's legitimate later return requires a newer epoch again.
  m_transport->setAutoSnapshot([this] {
    Snapshot returning = validSnapshot();
    returning.owner = QStringLiteral(":1.23");
    returning.epoch = 43;
    returning.revision = 1;
    return returning;
  }());
  m_transport->emitOwner(QStringLiteral(":1.23"));
  QTRY_VERIFY_WITH_TIMEOUT(
      client->model().lineage().has_value()
          && client->model().lineage()->epoch == quint64(43),
      2'000);
  QCOMPARE(client->state(), ClientState::Ready);
}

void NetworkClientTests::rejectsRetiredEpochAcrossRealOwnerCycle() {
  std::unique_ptr<NetworkClient> client = makeClient();

  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.42");
  ownerB.epoch = 42;
  ownerB.revision = 1;
  m_transport->setAutoSnapshot(ownerB);
  m_transport->emitOwner(ownerB.owner);
  QTRY_VERIFY_WITH_TIMEOUT(
      client->model().lineage().has_value()
          && client->model().lineage()->owner == ownerB.owner,
      2'000);

  Snapshot retiredA = validSnapshot();
  retiredA.revision = 500;
  m_transport->setAutoSnapshot(retiredA);
  m_transport->emitOwner(retiredA.owner);
  QTRY_VERIFY_WITH_TIMEOUT(client->state() == ClientState::Degraded, 2'000);
  QVERIFY(!client->model().snapshot().has_value());
  QVERIFY(!client->model().lineage().has_value());
  QCOMPARE(client->model().lineageHighWater()->epoch, quint64(42));
}

void NetworkClientTests::timesOutAndNeverReplaysSnapshot() {
  std::unique_ptr<NetworkClient> client = makeClient();
  m_transport->clearAutoSnapshot();
  client->refresh();
  QTRY_COMPARE(m_transport->snapshotRequests.size(), 2);

  QTRY_VERIFY_WITH_TIMEOUT(client->state() == ClientState::Degraded, 2'000);
  QVERIFY(client->lastError().contains(QStringLiteral("timed out")));

  // Retries are bounded by the schedule; each retry is one fresh request.
  QTRY_VERIFY_WITH_TIMEOUT(m_transport->snapshotRequests.size() == 3, 2'000);

  // The very late reply for the timed-out token is ignored.
  const quint64 lateToken = m_transport->snapshotRequests.at(1).first;
  m_transport->emitSnapshot(lateToken, QStringLiteral(":1.23"),
                            encodeSnapshot(validSnapshot()).payload);
  QVERIFY(client->model().snapshot().has_value());
  QCOMPARE(client->model().lineage()->revision, quint64(7));
}

void NetworkClientTests::completesOperationAndRefreshes() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QString error;
  QVERIFY(client->requestScan(30'000, &error));
  QVERIFY(client->operationInFlight());
  QCOMPARE(m_transport->operations.size(), 1);
  QCOMPARE(m_transport->operations.first().kind, OperationKind::RequestScan);
  QCOMPARE(m_transport->operations.first().epoch, quint64(41));
  QCOMPARE(m_transport->operations.first().revision, quint64(7));
  QCOMPARE(m_transport->operations.first().parameters.value(
               QStringLiteral("deadlineMs")),
           QVariant::fromValue<qint64>(30'000));

  QSignalSpy finishedSpy(client.get(), &NetworkClient::operationFinished);
  OperationResult result = validOperationResult(
      OperationKind::RequestScan, OperationStatus::Succeeded);
  m_transport->emitOperation(m_transport->operations.first().token,
                             QStringLiteral(":1.42"),
                             encodeOperationResult(result).payload);
  // Wrong owner: nothing happens.
  QCOMPARE(finishedSpy.size(), 0);
  QVERIFY(client->operationInFlight());

  m_transport->emitOperation(m_transport->operations.first().token,
                             QStringLiteral(":1.23"),
                             encodeOperationResult(result).payload);
  QCOMPARE(finishedSpy.size(), 1);
  QVERIFY(!client->operationInFlight());

  // The authoritative revision follows; client refetches it.
  Snapshot updated = validSnapshot();
  updated.revision = 8;
  m_transport->setAutoSnapshot(updated);
  QTRY_VERIFY_WITH_TIMEOUT(client->model().lineage()->revision == quint64(8),
                           2'000);
}

void NetworkClientTests::reportsUncertainOperationOnTimeout() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QSignalSpy uncertainSpy(client.get(), &NetworkClient::operationUncertain);
  QVERIFY(client->requestScan(30'000));
  QTRY_VERIFY_WITH_TIMEOUT(uncertainSpy.size() == 1, 2'000);
  QVERIFY(!client->operationInFlight());
  // Never replayed: exactly one transport operation was issued. The client
  // refetches after reporting uncertain, so its state may legitimately have
  // recovered to Ready by the time the spy observed the report; the state is
  // not part of the uncertain guarantee.
  QCOMPARE(m_transport->operations.size(), 1);
}

void NetworkClientTests::reportsUncertainOperationOnTransportFailure() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QSignalSpy uncertainSpy(client.get(), &NetworkClient::operationUncertain);
  QVERIFY(client->connectKnownNetwork(cafeNetwork().id));
  m_transport->emitFailure(m_transport->operations.first().token,
                           QStringLiteral(":1.23"),
                           QStringLiteral("org.qindaqt.Network1.Busy"));
  QCOMPARE(uncertainSpy.size(), 1);
  QVERIFY(!client->operationInFlight());
  QCOMPARE(client->state(), ClientState::Degraded);
  QCOMPARE(m_transport->operations.size(), 1);
}

void NetworkClientTests::rejectsLocalIntents() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QString error;
  QVERIFY(!client->connectKnownNetwork(QStringLiteral("deadbeef"), &error));
  QCOMPARE(error, QStringLiteral("known-network-id-invalid"));
  QCOMPARE(m_transport->operations.size(), 0);

  QVERIFY(!client->requestScan(999, &error));
  QCOMPARE(error, QStringLiteral("scan-deadline-out-of-bounds"));

  QVERIFY(!client->disconnectDevice(QStringLiteral("wlan0"), &error));
  QCOMPARE(error, QStringLiteral("device-not-connected"));

  QVERIFY(client->requestScan(30'000, &error));
  QVERIFY(!client->requestScan(30'000, &error));
  QCOMPARE(error, QStringLiteral("operation-in-flight"));
  QVERIFY(!client->setRadio(RadioKind::Wifi, false, &error));
  QCOMPARE(error, QStringLiteral("operation-in-flight"));

  // A degraded client rejects new intents locally without transporting them.
  m_transport->clearAutoSnapshot();
  client->refresh();
  QTRY_VERIFY_WITH_TIMEOUT(client->state() == ClientState::Degraded, 2'000);
  QVERIFY(!client->requestScan(30'000, &error));
  QCOMPARE(error, QStringLiteral("client-not-ready"));
  QCOMPARE(m_transport->operations.size(), 1);
}

void NetworkClientTests::tearsDownCleanlyAndIgnoresLateSignals() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QSignalSpy stateSpy(client.get(), &NetworkClient::stateChanged);
  QSignalSpy uncertainSpy(client.get(), &NetworkClient::operationUncertain);
  QVERIFY(client->requestScan(30'000));
  const quint64 token = m_transport->operations.first().token;

  client->stop();
  QCOMPARE(client->state(), ClientState::Unavailable);
  QVERIFY(!client->operationInFlight());
  QVERIFY(!client->model().snapshot().has_value());
  QCOMPARE(uncertainSpy.size(), 0);

  const qsizetype statesBefore = stateSpy.size();
  m_transport->emitOperation(token, QStringLiteral(":1.23"),
                             encodeOperationResult(validOperationResult()).payload);
  m_transport->emitSnapshot(token, QStringLiteral(":1.23"),
                            encodeSnapshot(validSnapshot()).payload);
  m_transport->emitOwner(QStringLiteral(":1.77"));
  m_transport->emitBusDisconnected();
  QCOMPARE(stateSpy.size(), statesBefore);
  QCOMPARE(client->state(), ClientState::Unavailable);
  QVERIFY(!client->model().snapshot().has_value());

  // Stop is idempotent.
  client->stop();
  QCOMPARE(client->state(), ClientState::Unavailable);
}

void NetworkClientTests::reportsTransportLoss() {
  std::unique_ptr<NetworkClient> client = makeClient();
  QSignalSpy uncertainSpy(client.get(), &NetworkClient::operationUncertain);
  QVERIFY(client->requestScan(30'000));
  m_transport->emitBusDisconnected();
  QCOMPARE(client->state(), ClientState::Unavailable);
  QCOMPARE(uncertainSpy.size(), 1);
  QVERIFY(!client->model().snapshot().has_value());
  QVERIFY(client->lastError().contains(QStringLiteral("disconnected")));
}

void NetworkClientTests::retriesAfterFailedTransportStart() {
  FakeNetworkTransport transport;
  transport.m_startFails = true;
  transport.m_startError =
      QStringLiteral("password=\"transport secret\" could not start");
  NetworkClient client(transport, Model::MonotonicClock([this] {
                         return m_now;
                       }), fastTiming());
  QString error;
  QVERIFY(!client.start(&error));
  QCOMPARE(client.state(), ClientState::Unavailable);
  QVERIFY(!client.model().snapshot().has_value());
  QVERIFY(!error.contains(QStringLiteral("transport secret")));
  QVERIFY(!client.lastError().contains(QStringLiteral("transport secret")));
  QCOMPARE(transport.startCalls, 1);
  QCOMPARE(transport.stopCalls, 1);

  transport.m_startFails = false;
  QVERIFY(client.start(&error));
  QVERIFY(error.isEmpty());
  QCOMPARE(transport.startCalls, 2);
  QVERIFY(transport.m_running);
  QCOMPARE(client.state(), ClientState::Connecting);
  client.stop();
  QCOMPARE(transport.stopCalls, 2);
}

void NetworkClientTests::rejectsInvalidTimingConfiguration() {
  FakeNetworkTransport transport;
  ClientTiming broken = fastTiming();
  broken.requestTimeoutMilliseconds = 1;
  NetworkClient client(transport, {}, broken);
  QString error;
  QVERIFY(!client.start(&error));
  QVERIFY(!error.isEmpty());

  ClientTiming tooMany = fastTiming();
  tooMany.retryMilliseconds =
      QVector<int>(kMaximumRetryDelays + 1, 100);
  NetworkClient tooManyClient(transport, {}, tooMany);
  QVERIFY(!tooManyClient.start(&error));
  QCOMPARE(transport.startCalls, 0);
}

QTEST_MAIN(NetworkClientTests)
#include "tst_network_client.moc"
