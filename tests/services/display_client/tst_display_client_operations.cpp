// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>

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

Display::OperationResult spyResult(const QSignalSpy &spy, qsizetype index = 0) {
  return qvariant_cast<Display::OperationResult>(spy.at(index).at(1));
}
} // namespace

class DisplayClientOperationsTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void inlineReplyIsDeferred();
  void timeoutIsExactlyOnceAndNeverReplayed();
  void stopStartRetainsCompletionAndIdsIncrease();
  void cancelSupersedesOnceAndLateReplyIsDropped();
  void ownerLossRetainsSubmittedLineage();
  void localPreconditionsAreTyped();
};

void DisplayClientOperationsTest::inlineReplyIsDeferred() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);
  transport.inlineOperationReply = true;
  transport.inlineResult = operationResult(
      Display::OperationKind::Stage, Display::OperationStatus::Accepted,
      QStringLiteral("epoch-A"), 1, QStringLiteral("tx"));

  const quint64 id = client.stage(QStringLiteral("tx"),
                                  testCandidate(QStringLiteral("epoch-A"), 1));
  QVERIFY(id != 0);
  QCOMPARE(completions.size(), 0);
  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), id);
}

void DisplayClientOperationsTest::timeoutIsExactlyOnceAndNeverReplayed() {
  FakeDisplayTransport transport;
  Client client(&transport);
  client.setRequestTimeout(10);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  const quint64 id = client.stage(QStringLiteral("tx"),
                                  testCandidate(QStringLiteral("epoch-A"), 1));
  const auto operation = transport.operations.constLast();
  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(spyResult(completions).status, Display::OperationStatus::Uncertain);
  QCOMPARE(spyResult(completions).error, Display::ErrorCode::Timeout);
  QCOMPARE(spyResult(completions).diagnostic,
           QStringLiteral("transport-timeout"));
  QCOMPARE(transport.operations.size(), 1);
  QCOMPARE(client.state(), ClientState::Degraded);
  QVERIFY(transport.fetches.size() >= 2);

  transport.replyOperation(operation,
                           operationResult(Display::OperationKind::Stage,
                                           Display::OperationStatus::Accepted,
                                           QStringLiteral("epoch-A"), 1,
                                           QStringLiteral("tx")));
  QTest::qWait(20);
  QCOMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), id);
}

void DisplayClientOperationsTest::stopStartRetainsCompletionAndIdsIncrease() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  const quint64 first = client.stage(
      QStringLiteral("tx"), testCandidate(QStringLiteral("epoch-A"), 1));
  client.stop();
  client.start();
  transport.publishOwner(QStringLiteral(":1.10"));
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-A"), 2));
  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), first);
  QCOMPARE(spyResult(completions).diagnostic, QStringLiteral("client-stopped"));
  QCOMPARE(spyResult(completions).initiatingEpoch, QStringLiteral("epoch-A"));
  QCOMPARE(spyResult(completions).initiatingRevision, quint64(1));
  QCOMPARE(spyResult(completions).transactionId, QStringLiteral("tx"));

  const quint64 second = client.stage(
      QStringLiteral("tx-2"), testCandidate(QStringLiteral("epoch-A"), 2));
  QVERIFY(second > first);
}

void DisplayClientOperationsTest::ownerLossRetainsSubmittedLineage() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  const quint64 id = client.stage(QStringLiteral("tx"),
                                  testCandidate(QStringLiteral("epoch-A"), 1));
  transport.publishOwner({});

  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), id);
  const Display::OperationResult result = spyResult(completions);
  QCOMPARE(result.status, Display::OperationStatus::Uncertain);
  QCOMPARE(result.initiatingEpoch, QStringLiteral("epoch-A"));
  QCOMPARE(result.initiatingRevision, quint64(1));
  QCOMPARE(result.observedRevision, quint64(1));
  QCOMPARE(result.transactionId, QStringLiteral("tx"));
}

void DisplayClientOperationsTest::cancelSupersedesOnceAndLateReplyIsDropped() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  const quint64 stageId = client.stage(
      QStringLiteral("tx"), testCandidate(QStringLiteral("epoch-A"), 1));
  const auto stageOperation = transport.operations.constLast();
  const quint64 cancelId = client.cancel(QStringLiteral("tx"));
  const auto cancelOperation = transport.operations.constLast();
  QVERIFY(cancelId > stageId);
  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), stageId);
  QCOMPARE(spyResult(completions).status, Display::OperationStatus::Uncertain);

  transport.replyOperation(stageOperation,
                           operationResult(Display::OperationKind::Stage,
                                           Display::OperationStatus::Accepted,
                                           QStringLiteral("epoch-A"), 1,
                                           QStringLiteral("tx")));
  transport.replyOperation(cancelOperation,
                           operationResult(Display::OperationKind::Cancel,
                                           Display::OperationStatus::Succeeded,
                                           QStringLiteral("epoch-A"), 1,
                                           QStringLiteral("tx")));
  QTRY_COMPARE(completions.size(), 2);
  QCOMPARE(completions.at(1).at(0).toULongLong(), cancelId);
}

void DisplayClientOperationsTest::localPreconditionsAreTyped() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  auto invalid = testCandidate(QStringLiteral("epoch-A"), 1);
  invalid.outputs[0].scale = 99.0;
  const quint64 invalidId = client.stage(QStringLiteral("bad"), invalid);
  QTRY_COMPARE(completions.size(), 1);
  QCOMPARE(completions.at(0).at(0).toULongLong(), invalidId);
  QCOMPARE(spyResult(completions).error, Display::ErrorCode::InvalidCandidate);
  QCOMPARE(transport.operations.size(), 0);

  const quint64 invalidTransactionId = client.preview({});
  QTRY_COMPARE(completions.size(), 2);
  QCOMPARE(completions.at(1).at(0).toULongLong(), invalidTransactionId);
  QCOMPARE(spyResult(completions, 1).error,
           Display::ErrorCode::InvalidCandidate);
  QCOMPARE(spyResult(completions, 1).diagnostic,
           QStringLiteral("invalid-transaction-id"));
  QCOMPARE(transport.operations.size(), 0);

  const quint64 activeId = client.stage(
      QStringLiteral("tx"), testCandidate(QStringLiteral("epoch-A"), 1));
  const quint64 busyId = client.stage(
      QStringLiteral("other"), testCandidate(QStringLiteral("epoch-A"), 1));
  QTRY_COMPARE(completions.size(), 3);
  QCOMPARE(completions.at(2).at(0).toULongLong(), busyId);
  QCOMPARE(spyResult(completions, 2).error,
           Display::ErrorCode::TransactionActive);
  QVERIFY(activeId < busyId);
  QCOMPARE(transport.operations.size(), 1);
}

QTEST_MAIN(DisplayClientOperationsTest)
#include "tst_display_client_operations.moc"
