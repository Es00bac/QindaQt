// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/fake_network_backend.h"
#include <QtTest>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_service/network_service_coordinator.h>
using namespace QindaQt::Network;
using namespace QindaQt::Network::Service;
using namespace QindaQt::Network::Tests;
class HiddenServiceTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void compatiblePendingAndCompletion();
  void lineageBusyAndAuthorityLoss();
};
void HiddenServiceTests::compatiblePendingAndCompletion() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator service(&backend);
  QVERIFY(service.start(QStringLiteral(":1.99")));
  backend.publish(readyNetworkObservation());
  const QByteArray before = service.snapshotPayload();
  NetworkServiceRequest request;
  request.kind = OperationKind::ConnectKnownNetwork;
  request.initiatingEpoch = service.snapshot().epoch;
  request.initiatingRevision = service.snapshot().revision;
  request.hiddenJoin = ConnectHiddenIntent{QStringLiteral("wlan0"),
                                           QStringLiteral(" new hidden "),
                                           SecuritySuite::Wpa3Personal};
  QSignalSpy completed(&service,
                       &NetworkServiceCoordinator::operationCompleted);
  auto accepted = service.submit(request);
  QVERIFY(accepted.pending);
  // An old strict snapshot decoder sees unchanged domains/bytes while pending.
  QCOMPARE(service.snapshotPayload(), before);
  Snapshot decoded;
  QVERIFY(decodeSnapshot(service.snapshotPayload(), decoded).succeeded());
  QTRY_COMPARE(backend.calls.size(), 1);
  QCOMPARE(backend.calls.first().request.kind,
           OperationKind::ConnectKnownNetwork);
  QCOMPARE(backend.calls.first().request.hiddenJoin, request.hiddenJoin);
  QVERIFY(backend.calls.first().request.identifier.isEmpty());
  backend.finish(accepted.operationId,
                 {BackendOperationStatus::Succeeded, {}, {}});
  QCOMPARE(completed.size(), 1);
  auto result = completed.first().at(1).value<OperationResult>();
  QCOMPARE(result.kind, OperationKind::ConnectKnownNetwork);
  OperationResult oldDecoded;
  QVERIFY(
      decodeOperationResult(encodeOperationResult(result).payload, oldDecoded)
          .succeeded());
  QCOMPARE(result, oldDecoded);
}
void HiddenServiceTests::lineageBusyAndAuthorityLoss() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator service(&backend);
  QVERIFY(service.start(QStringLiteral(":1.98")));
  backend.publish(readyNetworkObservation());
  NetworkServiceRequest request;
  request.kind = OperationKind::ConnectKnownNetwork;
  request.hiddenJoin =
      ConnectHiddenIntent{QStringLiteral("wlan0"), QStringLiteral("hidden"),
                          SecuritySuite::Wpa2Personal};
  request.initiatingEpoch = service.snapshot().epoch;
  request.initiatingRevision = service.snapshot().revision - 1;
  QCOMPARE(service.submit(request).immediateResult.reasonCode,
           QStringLiteral("stale-lineage"));
  request.initiatingRevision = service.snapshot().revision;
  auto accepted = service.submit(request);
  QVERIFY(accepted.pending);
  QCOMPARE(service.submit(request).immediateResult.status,
           OperationStatus::Busy);
  QSignalSpy completed(&service,
                       &NetworkServiceCoordinator::operationCompleted);
  service.stop();
  QCOMPARE(completed.size(), 1);
  QCOMPARE(completed.first().at(1).value<OperationResult>().status,
           OperationStatus::Uncertain);
  QTest::qWait(1);
  QCOMPARE(backend.calls.size(), 0);
}
QTEST_GUILESS_MAIN(HiddenServiceTests)
#include "tst_network_hidden_service.moc"
