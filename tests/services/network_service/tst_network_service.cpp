// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_network_backend.h"

#include <qindaqt/services/network_protocol/network_validation.h>
#include <qindaqt/services/network_service/network_service_coordinator.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Service;
using namespace QindaQt::Network::Tests;

class NetworkServiceTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void publishesValidatedAtomicSnapshots();
  void rejectsMalformedBackendAndRedactsDiagnostics();
  void admitsTypedOperationsAndFencesLineage();
  void timesOutExactlyOnceWithoutReplay();
  void authorityReplacementRequiresFreshPublicOwner();
  void unavailableBackendIsHonest();
};

void NetworkServiceTests::publishesValidatedAtomicSnapshots() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator coordinator(&backend);
  QSignalSpy changed(&coordinator, &NetworkServiceCoordinator::snapshotChanged);
  QVERIFY(coordinator.start(QStringLiteral(":1.50")));
  QCOMPARE(coordinator.snapshot().availability, Availability::Starting);
  QVERIFY(validateSnapshot(coordinator.snapshot()).accepted);
  const quint64 epoch = coordinator.snapshot().epoch;
  QVERIFY(epoch != 0);

  backend.publish(readyNetworkObservation());
  QCOMPARE(changed.size(), 2);
  QCOMPARE(coordinator.snapshot().availability, Availability::Ready);
  QCOMPARE(coordinator.snapshot().owner, QStringLiteral(":1.50"));
  QCOMPARE(coordinator.snapshot().epoch, epoch);
  QCOMPARE(coordinator.snapshot().revision, quint64(2));
  QVERIFY(validateSnapshot(coordinator.snapshot()).accepted);
  QVERIFY(!coordinator.snapshotPayload().isEmpty());
}

void NetworkServiceTests::rejectsMalformedBackendAndRedactsDiagnostics() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator coordinator(&backend);
  QVERIFY(coordinator.start(QStringLiteral(":1.51")));
  auto observation = readyNetworkObservation();
  observation.diagnostic =
      QStringLiteral("password=do-not-publish backend is ready");
  backend.publish(observation);
  QVERIFY(!coordinator.snapshot().diagnostic.contains(
      QStringLiteral("do-not-publish")));
  QVERIFY(
      coordinator.snapshot().diagnostic.contains(QStringLiteral("<redacted>")));

  observation.devices[0].interfaceName = QStringLiteral("invalid/interface");
  backend.publish(observation);
  QCOMPARE(coordinator.snapshot().availability, Availability::Degraded);
  QCOMPARE(coordinator.snapshot().reasonCode,
           QStringLiteral("backend-malformed"));
  QVERIFY(coordinator.snapshot().devices.isEmpty());
  QVERIFY(coordinator.snapshot().knownNetworks.isEmpty());
  QVERIFY(validateSnapshot(coordinator.snapshot()).accepted);
}

void NetworkServiceTests::admitsTypedOperationsAndFencesLineage() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator coordinator(&backend);
  QSignalSpy completed(&coordinator,
                       &NetworkServiceCoordinator::operationCompleted);
  QVERIFY(coordinator.start(QStringLiteral(":1.52")));
  backend.publish(readyNetworkObservation());
  const Snapshot current = coordinator.snapshot();

  NetworkServiceRequest scan;
  scan.kind = OperationKind::RequestScan;
  scan.initiatingEpoch = current.epoch;
  scan.initiatingRevision = current.revision;
  scan.scanDeadlineMilliseconds = 30'000;
  const OperationSubmission accepted = coordinator.submit(scan);
  QVERIFY(accepted.pending);
  QCOMPARE(backend.calls.size(), 1);
  QCOMPARE(backend.calls.first().request.kind, OperationKind::RequestScan);
  QCOMPARE(backend.calls.first().request.scanDeadlineMilliseconds,
           qint64(30'000));

  BackendOperationOutcome success;
  success.status = BackendOperationStatus::Succeeded;
  backend.finish(accepted.operationId, success);
  QCOMPARE(completed.size(), 1);
  const OperationResult result =
      completed.first().at(1).value<OperationResult>();
  QCOMPARE(result.status, OperationStatus::Succeeded);
  QCOMPARE(result.initiatingEpoch, current.epoch);
  QCOMPARE(result.initiatingRevision, current.revision);
  QVERIFY(validateOperationResult(result).accepted);

  const OperationSubmission failed = coordinator.submit(scan);
  QVERIFY(failed.pending);
  BackendOperationOutcome secretFailure;
  secretFailure.status = BackendOperationStatus::Failed;
  secretFailure.reasonCode = QStringLiteral("scan-dispatch-failed");
  secretFailure.diagnostic = QStringLiteral("password=do-not-publish");
  backend.finish(failed.operationId, secretFailure);
  QCOMPARE(completed.size(), 2);
  const OperationResult redacted =
      completed.last().at(1).value<OperationResult>();
  QVERIFY(!redacted.diagnostic.contains(QStringLiteral("do-not-publish")));
  QVERIFY(redacted.diagnostic.contains(QStringLiteral("<redacted>")));

  scan.initiatingRevision = current.revision - 1;
  const OperationSubmission stale = coordinator.submit(scan);
  QVERIFY(!stale.pending);
  QCOMPARE(stale.immediateResult.status, OperationStatus::Rejected);
  QCOMPARE(stale.immediateResult.reasonCode, QStringLiteral("stale-lineage"));
  QCOMPARE(backend.calls.size(), 2);

  scan.initiatingEpoch = 0;
  scan.initiatingRevision = 0;
  const OperationSubmission malformed = coordinator.submit(scan);
  QVERIFY(!malformed.pending);
  QVERIFY(validateOperationResult(malformed.immediateResult).accepted);
}

void NetworkServiceTests::timesOutExactlyOnceWithoutReplay() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator coordinator(&backend, 100);
  QSignalSpy completed(&coordinator,
                       &NetworkServiceCoordinator::operationCompleted);
  QVERIFY(coordinator.start(QStringLiteral(":1.53")));
  backend.publish(readyNetworkObservation());
  NetworkServiceRequest request;
  request.kind = OperationKind::SetRadio;
  request.initiatingEpoch = coordinator.snapshot().epoch;
  request.initiatingRevision = coordinator.snapshot().revision;
  request.radioKind = RadioKind::Wifi;
  request.enable = false;
  const OperationSubmission submission = coordinator.submit(request);
  QVERIFY(submission.pending);
  QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 2'000);
  QCOMPARE(completed.first().at(1).value<OperationResult>().status,
           OperationStatus::Uncertain);
  QCOMPARE(backend.cancelled, QList<quint64>{submission.operationId});
  BackendOperationOutcome late;
  late.status = BackendOperationStatus::Succeeded;
  backend.finish(submission.operationId, late);
  QCOMPARE(completed.size(), 1);
  QCOMPARE(backend.calls.size(), 1);
}

void NetworkServiceTests::authorityReplacementRequiresFreshPublicOwner() {
  FakeNetworkBackend backend;
  NetworkServiceCoordinator coordinator(&backend);
  QSignalSpy completed(&coordinator,
                       &NetworkServiceCoordinator::operationCompleted);
  QSignalSpy restart(&coordinator, &NetworkServiceCoordinator::restartRequired);
  QVERIFY(coordinator.start(QStringLiteral(":1.54")));
  backend.publish(readyNetworkObservation());
  const quint64 epoch = coordinator.snapshot().epoch;

  NetworkServiceRequest request;
  request.kind = OperationKind::RequestScan;
  request.initiatingEpoch = epoch;
  request.initiatingRevision = coordinator.snapshot().revision;
  request.scanDeadlineMilliseconds = 10'000;
  const OperationSubmission submission = coordinator.submit(request);
  QVERIFY(submission.pending);
  const quint64 retiredGeneration = backend.generation;
  backend.replaceAuthority();
  QCOMPARE(restart.size(), 1);
  QCOMPARE(completed.size(), 1);
  QCOMPARE(completed.first().at(1).value<OperationResult>().status,
           OperationStatus::Uncertain);
  QCOMPARE(coordinator.snapshot().epoch, epoch);
  QCOMPARE(coordinator.snapshot().availability, Availability::Unavailable);
  QCOMPARE(coordinator.snapshot().reasonCode,
           QStringLiteral("networkmanager-replaced"));

  const Snapshot retired = coordinator.snapshot();
  backend.publishForGeneration(retiredGeneration, readyNetworkObservation());
  BackendOperationOutcome late;
  late.status = BackendOperationStatus::Succeeded;
  backend.finishForGeneration(retiredGeneration, submission.operationId, late);
  QCOMPARE(coordinator.snapshot(), retired);
  QCOMPARE(completed.size(), 1);
}

void NetworkServiceTests::unavailableBackendIsHonest() {
  FakeNetworkBackend backend;
  backend.failStart = true;
  NetworkServiceCoordinator coordinator(&backend);
  QVERIFY(coordinator.start(QStringLiteral(":1.55")));
  QCOMPARE(coordinator.snapshot().availability, Availability::Unavailable);
  QCOMPARE(coordinator.snapshot().capabilities, Capabilities{});
  QCOMPARE(coordinator.snapshot().reasonCode,
           QStringLiteral("backend-start-failed"));
  QVERIFY(validateSnapshot(coordinator.snapshot()).accepted);
}

QTEST_GUILESS_MAIN(NetworkServiceTests)
#include "tst_network_service.moc"
