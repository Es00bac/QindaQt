// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/display_coordinator.h>

#include "support/display_client_test_support.h"
#include "support/fake_display_transport.h"

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

namespace {
void makeReady(FakeDisplayTransport &transport, Client &client) {
  client.start();
  transport.publishOwner(QStringLiteral(":1.10"));
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-A"), 1));
}

Display::Snapshot transactionSnapshot(
    quint64 revision, Display::TransactionState state,
    Display::TransactionReason reason = Display::TransactionReason::None) {
  auto snapshot = testSnapshot(QStringLiteral("epoch-A"), revision);
  snapshot.transactions = {{.transactionId = QStringLiteral("tx"),
                            .state = state,
                            .reason = reason,
                            .initiatingEpoch = QStringLiteral("epoch-A"),
                            .baseRevision = 1,
                            .observedRevision = revision,
                            .deadlineMonotonicMilliseconds = 50'000,
                            .revertAttempt = 0}};
  return snapshot;
}

void replyLast(FakeDisplayTransport &transport, Display::OperationStatus status,
               QString diagnostic = {},
               Display::ErrorCode error = Display::ErrorCode::None) {
  const auto operation = transport.operations.constLast();
  transport.replyOperation(
      operation,
      operationResult(operation.kind, status, QStringLiteral("epoch-A"), 1,
                      operation.transactionId, std::move(diagnostic), error));
  QCoreApplication::processEvents();
}

void advanceToPreviewAccepted(FakeDisplayTransport &transport, Client &client,
                              Coordinator &coordinator) {
  QVERIFY(coordinator.begin(QStringLiteral("tx"),
                            testCandidate(QStringLiteral("epoch-A"), 1)));
  replyLast(transport, Display::OperationStatus::Accepted);
  QCOMPARE(transport.operations.constLast().kind,
           Display::OperationKind::Preview);
  replyLast(transport, Display::OperationStatus::Accepted);
  QCOMPARE(coordinator.state(), CoordinatorState::Previewing);
  QVERIFY(client.operationPending() == false);
}
} // namespace

class DisplayClientCoordinatorTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void serverProjectionGatesConfirmation();
  void vanishedSummaryAndServiceLossFinishTruthfully();
  void uncertainPreviewNeedsSuccessfulCancel();
  void confirmingCannotBeCancelled();
  void noOpHasClosedOutcome();
  void rescueTimerNeverPreemptsServerWindow();
};

void DisplayClientCoordinatorTest::serverProjectionGatesConfirmation() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  makeReady(transport, client);
  QSignalSpy finished(&coordinator, &Coordinator::transactionFinished);
  advanceToPreviewAccepted(transport, client, coordinator);

  transport.replySnapshot(
      transport.fetches.constLast(),
      transactionSnapshot(2, Display::TransactionState::Applying));
  QCOMPARE(coordinator.state(), CoordinatorState::Previewing);
  QVERIFY(!coordinator.confirm());

  transport.replySnapshot(
      transport.fetches.constLast(),
      transactionSnapshot(3, Display::TransactionState::AwaitingConfirmation));
  QCOMPARE(coordinator.state(), CoordinatorState::AwaitingConfirmation);
  QVERIFY(coordinator.confirm());
  QCOMPARE(coordinator.state(), CoordinatorState::Confirming);
  replyLast(transport, Display::OperationStatus::Succeeded);
  QCOMPARE(coordinator.state(), CoordinatorState::Confirmed);
  QCOMPARE(finished.size(), 1);
  QCOMPARE(qvariant_cast<CoordinatorOutcome>(finished.at(0).at(1)),
           CoordinatorOutcome::Confirmed);
}

void DisplayClientCoordinatorTest::
    vanishedSummaryAndServiceLossFinishTruthfully() {
  {
    FakeDisplayTransport transport;
    Client client(&transport);
    Coordinator coordinator(&client);
    makeReady(transport, client);
    QSignalSpy finished(&coordinator, &Coordinator::transactionFinished);
    advanceToPreviewAccepted(transport, client, coordinator);
    transport.replySnapshot(
        transport.fetches.constLast(),
        transactionSnapshot(2, Display::TransactionState::Applying));
    transport.replySnapshot(transport.fetches.constLast(),
                            testSnapshot(QStringLiteral("epoch-A"), 3));
    QCOMPARE(coordinator.state(), CoordinatorState::Reverted);
    QCOMPARE(qvariant_cast<CoordinatorOutcome>(finished.at(0).at(1)),
             CoordinatorOutcome::Reverted);
  }

  {
    FakeDisplayTransport transport;
    Client client(&transport);
    Coordinator coordinator(&client);
    makeReady(transport, client);
    QSignalSpy finished(&coordinator, &Coordinator::transactionFinished);
    advanceToPreviewAccepted(transport, client, coordinator);
    transport.replySnapshot(
        transport.fetches.constLast(),
        transactionSnapshot(2,
                            Display::TransactionState::AwaitingConfirmation));
    transport.publishOwner({});
    QCOMPARE(coordinator.state(), CoordinatorState::Uncertain);
    QCOMPARE(qvariant_cast<CoordinatorOutcome>(finished.at(0).at(1)),
             CoordinatorOutcome::Uncertain);
    QCOMPARE(finished.at(0).at(2).toString(), QStringLiteral("lineage-lost"));
  }
}

void DisplayClientCoordinatorTest::uncertainPreviewNeedsSuccessfulCancel() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  makeReady(transport, client);
  QSignalSpy finished(&coordinator, &Coordinator::transactionFinished);
  QVERIFY(coordinator.begin(QStringLiteral("tx"),
                            testCandidate(QStringLiteral("epoch-A"), 1)));
  replyLast(transport, Display::OperationStatus::Accepted);
  const auto preview = transport.operations.constLast();
  transport.replyOperation(preview, {}, false,
                           QStringLiteral("transport-timeout"));
  QCoreApplication::processEvents();
  QCOMPARE(coordinator.state(), CoordinatorState::Cancelling);
  replyLast(transport, Display::OperationStatus::Rejected,
            QStringLiteral("unknown-transaction"),
            Display::ErrorCode::UnknownTransaction);
  QCOMPARE(coordinator.state(), CoordinatorState::Uncertain);
  QCOMPARE(qvariant_cast<CoordinatorOutcome>(finished.at(0).at(1)),
           CoordinatorOutcome::Uncertain);
}

void DisplayClientCoordinatorTest::confirmingCannotBeCancelled() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  makeReady(transport, client);
  advanceToPreviewAccepted(transport, client, coordinator);
  transport.replySnapshot(
      transport.fetches.constLast(),
      transactionSnapshot(2, Display::TransactionState::AwaitingConfirmation));
  QVERIFY(coordinator.confirm());
  QVERIFY(!coordinator.cancel());
  QCOMPARE(transport.operations.constLast().kind,
           Display::OperationKind::Confirm);
}

void DisplayClientCoordinatorTest::noOpHasClosedOutcome() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  makeReady(transport, client);
  QSignalSpy finished(&coordinator, &Coordinator::transactionFinished);
  QVERIFY(coordinator.begin(QStringLiteral("tx"),
                            testCandidate(QStringLiteral("epoch-A"), 1)));
  replyLast(transport, Display::OperationStatus::Succeeded,
            QStringLiteral("no-op"));
  QCOMPARE(coordinator.state(), CoordinatorState::NoOp);
  QCOMPARE(qvariant_cast<CoordinatorOutcome>(finished.at(0).at(1)),
           CoordinatorOutcome::NoOp);
  QCOMPARE(transport.operations.size(), 1);
}

void DisplayClientCoordinatorTest::rescueTimerNeverPreemptsServerWindow() {
  FakeDisplayTransport transport;
  Client client(&transport);
  Coordinator coordinator(&client);
  coordinator.setConfirmationDeadline(1);
  makeReady(transport, client);
  advanceToPreviewAccepted(transport, client, coordinator);
  transport.replySnapshot(
      transport.fetches.constLast(),
      transactionSnapshot(2, Display::TransactionState::AwaitingConfirmation));
  const qsizetype operations = transport.operations.size();
  QTest::qWait(30);
  QCOMPARE(coordinator.state(), CoordinatorState::AwaitingConfirmation);
  QCOMPARE(transport.operations.size(), operations);
}

QTEST_MAIN(DisplayClientCoordinatorTest)
#include "tst_display_client_coordinator.moc"
