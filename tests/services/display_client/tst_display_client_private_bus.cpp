// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>
#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_service/resident_display_service.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_client_private_bus_support.h"
#include "support/display_client_test_support.h"

#include <QtDBus/QDBusConnection>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

class DisplayClientPrivateBusTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void transportLocalFailuresAreAsynchronous();
  void lifecycleReplacementAndTransactionAreReal();
};

void DisplayClientPrivateBusTest::transportLocalFailuresAreAsynchronous() {
  PrivateSessionBus bus;
  QString busError;
  QVERIFY2(bus.start(&busError), qPrintable(busError));
  const QString connectionName =
      privateConnectionName(QStringLiteral("transport-contract"));
  QDBusConnection connection =
      QDBusConnection::connectToBus(bus.address(), connectionName);
  QVERIFY(connection.isConnected());

  QtDisplayTransport transport(connection);
  QSignalSpy snapshots(&transport, &DisplayTransport::snapshotReply);
  QSignalSpy operations(&transport, &DisplayTransport::operationReply);
  transport.start();
  transport.fetchSnapshot(QStringLiteral(":1.99"), 1);
  transport.submitPreview(QStringLiteral(":1.99"), 2, QStringLiteral("tx"));
  QCOMPARE(snapshots.size(), 0);
  QCOMPARE(operations.size(), 0);
  QTRY_COMPARE(snapshots.size(), 1);
  QTRY_COMPARE(operations.size(), 1);
  QCOMPARE(snapshots.at(0).at(4).toString(),
           QStringLiteral("owner-unavailable"));
  QCOMPARE(operations.at(0).at(4).toString(),
           QStringLiteral("owner-unavailable"));

  transport.stop();
  QDBusConnection::disconnectFromBus(connectionName);
}

void DisplayClientPrivateBusTest::lifecycleReplacementAndTransactionAreReal() {
  PrivateSessionBus bus;
  QString busError;
  QVERIFY2(bus.start(&busError), qPrintable(busError));
  Display::registerDBusTypes();

  const QString clientName = privateConnectionName(QStringLiteral("client"));
  const QString residentNameA =
      privateConnectionName(QStringLiteral("resident-a"));
  const QString residentNameB =
      privateConnectionName(QStringLiteral("resident-b"));
  QDBusConnection clientConnection =
      QDBusConnection::connectToBus(bus.address(), clientName);
  QDBusConnection residentConnectionA =
      QDBusConnection::connectToBus(bus.address(), residentNameA);
  QVERIFY(clientConnection.isConnected());
  QVERIFY(residentConnectionA.isConnected());

  QtDisplayTransport transport(clientConnection);
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);
  QSignalSpy completions(&client, &Client::operationCompleted);
  client.start();
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Unavailable, 5'000);
  QCOMPARE(client.reasonCode(), QStringLiteral("owner-unavailable"));

  auto inventoryA = std::make_unique<FakeInventorySource>();
  FakeInventorySource *inventoryPointerA = inventoryA.get();
  auto portA = std::make_unique<FakeTransactionPort>();
  FakeTransactionPort *portPointerA = portA.get();
  const DisplayTransaction::Timing timing{
      .applyTimeoutMilliseconds = 1'000,
      .observationTimeoutMilliseconds = 1'000,
      .confirmationTimeoutMilliseconds = 1'000,
      .firstRevertBackoffMilliseconds = 20,
      .secondRevertBackoffMilliseconds = 20};
  auto serviceA = std::make_unique<DisplayService::ResidentDisplayService>(
      std::move(inventoryA), std::move(portA), std::make_unique<ElapsedClock>(),
      [] { return QStringLiteral("d3-private-a"); }, residentConnectionA,
      QString::fromLatin1(Display::kServiceName), timing);
  QCOMPARE(serviceA->start(), DisplayService::ServiceStartStatus::Started);
  inventoryPointerA->publish(inventoryFrame(1));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
  QVERIFY(client.snapshot().has_value());
  const QString firstEpoch = client.snapshot()->serviceEpoch;
  QCOMPARE(client.snapshot()->revision, quint64(1));

  auto changedMetadata = inventoryFrame(2);
  changedMetadata.outputs[0].model = QStringLiteral("Changed model");
  inventoryPointerA->publish(changedMetadata);
  QTRY_VERIFY_WITH_TIMEOUT(
      client.snapshot().has_value() && client.snapshot()->revision == 2, 5'000);
  const Display::Candidate oldCandidate =
      DisplayTopology::candidateFromSnapshot(*client.snapshot());

  QVERIFY(serviceA->model()
              ->safetyChanged(DisplayTransaction::SafetyState::Safe)
              .accepted);
  Display::Candidate candidate = oldCandidate;
  candidate.outputs[0].transform = Display::Transform::Rotate180;
  const quint64 stageId = client.stage(QStringLiteral("private-tx"), candidate);
  QTRY_COMPARE_WITH_TIMEOUT(completions.size(), 1, 5'000);
  QCOMPARE(completions.at(0).at(0).toULongLong(), stageId);
  QCOMPARE(
      qvariant_cast<Display::OperationResult>(completions.at(0).at(1)).status,
      Display::OperationStatus::Accepted);

  const quint64 previewId = client.preview(QStringLiteral("private-tx"));
  QTRY_COMPARE_WITH_TIMEOUT(completions.size(), 2, 5'000);
  QCOMPARE(completions.at(1).at(0).toULongLong(), previewId);
  QCOMPARE(
      qvariant_cast<Display::OperationResult>(completions.at(1).at(1)).status,
      Display::OperationStatus::Accepted);
  QTRY_COMPARE_WITH_TIMEOUT(portPointerA->requests.size(), 1, 5'000);
  portPointerA->completeLast(DisplayTransaction::ApplyOutcome::Applied);
  inventoryPointerA->publish(inventoryFrame(3, Display::Transform::Rotate180));
  QCOMPARE(serviceA->model()->view()->state,
           DisplayTransaction::MachineState::AwaitingConfirmation);
  QTRY_VERIFY_WITH_TIMEOUT(
      client.snapshot().has_value() && client.snapshot()->revision >= 3, 5'000);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
  QTRY_VERIFY_WITH_TIMEOUT(
      client.snapshot().has_value() &&
          !client.snapshot()->transactions.isEmpty() &&
          client.snapshot()->transactions.constFirst().state ==
              Display::TransactionState::AwaitingConfirmation,
      5'000);

  const quint64 confirmId = client.confirm(QStringLiteral("private-tx"));
  QTRY_COMPARE_WITH_TIMEOUT(completions.size(), 3, 5'000);
  QCOMPARE(completions.at(2).at(0).toULongLong(), confirmId);
  QCOMPARE(
      qvariant_cast<Display::OperationResult>(completions.at(2).at(1)).status,
      Display::OperationStatus::Succeeded);

  serviceA->stop();
  serviceA.reset();
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Unavailable, 5'000);
  QVERIFY(!client.hasSnapshot());

  QDBusConnection residentConnectionB =
      QDBusConnection::connectToBus(bus.address(), residentNameB);
  QVERIFY(residentConnectionB.isConnected());
  auto inventoryB = std::make_unique<FakeInventorySource>();
  FakeInventorySource *inventoryPointerB = inventoryB.get();
  auto portB = std::make_unique<FakeTransactionPort>();
  auto serviceB = std::make_unique<DisplayService::ResidentDisplayService>(
      std::move(inventoryB), std::move(portB), std::make_unique<ElapsedClock>(),
      [] { return QStringLiteral("d3-private-b"); }, residentConnectionB,
      QString::fromLatin1(Display::kServiceName), timing);
  QCOMPARE(serviceB->start(), DisplayService::ServiceStartStatus::Started);
  inventoryPointerB->publish(inventoryFrame(1));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
  QVERIFY(client.snapshot().has_value());
  QVERIFY(client.snapshot()->serviceEpoch != firstEpoch);

  const qsizetype beforeStale = completions.size();
  const quint64 staleId = client.stage(QStringLiteral("stale"), oldCandidate);
  QTRY_COMPARE_WITH_TIMEOUT(completions.size(), beforeStale + 1, 5'000);
  QCOMPARE(completions.constLast().at(0).toULongLong(), staleId);
  const auto stale =
      qvariant_cast<Display::OperationResult>(completions.constLast().at(1));
  QCOMPARE(stale.status, Display::OperationStatus::Rejected);
  QCOMPARE(stale.error, Display::ErrorCode::StaleRevision);

  client.stop();
  serviceB->stop();
  QDBusConnection::disconnectFromBus(residentNameB);
  QDBusConnection::disconnectFromBus(residentNameA);
  QDBusConnection::disconnectFromBus(clientName);
}

QTEST_MAIN(DisplayClientPrivateBusTest)
#include "tst_display_client_private_bus.moc"
