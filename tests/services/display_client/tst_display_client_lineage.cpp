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
void makeReady(FakeDisplayTransport &transport, Client &client,
               const QString &owner = QStringLiteral(":1.10"),
               const QString &epoch = QStringLiteral("epoch-A"),
               quint64 revision = 1) {
  client.start();
  transport.publishOwner(owner);
  QVERIFY(!transport.fetches.isEmpty());
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(epoch, revision));
  QCOMPARE(client.state(), ClientState::Ready);
}
} // namespace

class DisplayClientLineageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void lateAndDuplicateRepliesAreFenced();
  void hostileOperationLineageBecomesUncertain_data();
  void hostileOperationLineageBecomesUncertain();
  void announcedEpochFencesInitialRead();
  void revisionAndEpochLineageNeverRegresses();
};

void DisplayClientLineageTest::lateAndDuplicateRepliesAreFenced() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);

  client.start();
  transport.publishOwner(QStringLiteral(":1.10"));
  const auto oldFetch = transport.fetches.constLast();
  transport.publishOwner(QStringLiteral(":1.11"));
  const auto newFetch = transport.fetches.constLast();

  transport.replySnapshot(oldFetch, testSnapshot(QStringLiteral("epoch-A"), 1));
  QCOMPARE(snapshots.size(), 0);
  transport.replySnapshot(newFetch, testSnapshot(QStringLiteral("epoch-B"), 1));
  QCOMPARE(snapshots.size(), 1);
  transport.replySnapshot(newFetch, testSnapshot(QStringLiteral("epoch-B"), 2));
  QCOMPARE(snapshots.size(), 1);
  QVERIFY(client.snapshot().has_value());
  QCOMPARE(client.snapshot()->serviceEpoch, QStringLiteral("epoch-B"));
}

void DisplayClientLineageTest::hostileOperationLineageBecomesUncertain_data() {
  QTest::addColumn<QString>("fault");
  QTest::newRow("wrong-owner") << QStringLiteral("owner");
  QTest::newRow("wrong-kind") << QStringLiteral("kind");
  QTest::newRow("wrong-transaction") << QStringLiteral("transaction");
  QTest::newRow("wrong-epoch") << QStringLiteral("epoch");
}

void DisplayClientLineageTest::hostileOperationLineageBecomesUncertain() {
  QFETCH(QString, fault);
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QSignalSpy completions(&client, &Client::operationCompleted);

  const quint64 id = client.stage(QStringLiteral("tx"),
                                  testCandidate(QStringLiteral("epoch-A"), 1));
  const auto operation = transport.operations.constLast();
  Display::OperationResult result = operationResult(
      Display::OperationKind::Stage, Display::OperationStatus::Accepted,
      QStringLiteral("epoch-A"), 1, QStringLiteral("tx"));
  QString owner = operation.owner;
  if (fault == QStringLiteral("owner"))
    owner = QStringLiteral(":1.99");
  if (fault == QStringLiteral("kind"))
    result.kind = Display::OperationKind::Preview;
  if (fault == QStringLiteral("transaction"))
    result.transactionId = QStringLiteral("other");
  if (fault == QStringLiteral("epoch"))
    result.initiatingEpoch = QStringLiteral("epoch-B");
  transport.replyOperationAs(owner, id, result);

  QTRY_COMPARE(completions.size(), 1);
  const auto received =
      qvariant_cast<Display::OperationResult>(completions.at(0).at(1));
  QCOMPARE(received.status, Display::OperationStatus::Uncertain);
  QCOMPARE(received.diagnostic, QStringLiteral("lineage-mismatch"));
}

void DisplayClientLineageTest::announcedEpochFencesInitialRead() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);

  client.start();
  transport.publishOwner(QStringLiteral(":1.10"));
  const auto staleFetch = transport.fetches.constLast();
  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-B"), 1);
  transport.replySnapshot(staleFetch,
                          testSnapshot(QStringLiteral("epoch-A"), 1));

  QCOMPARE(snapshots.size(), 0);
  QVERIFY(!client.hasSnapshot());
  QCOMPARE(client.state(), ClientState::Degraded);
  QCOMPARE(client.reasonCode(), QStringLiteral("lineage-mismatch"));

  client.refresh();
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-B"), 1));
  QCOMPARE(snapshots.size(), 1);
  QCOMPARE(client.state(), ClientState::Ready);
  QCOMPARE(client.snapshot()->serviceEpoch, QStringLiteral("epoch-B"));
}

void DisplayClientLineageTest::revisionAndEpochLineageNeverRegresses() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);
  makeReady(transport, client, QStringLiteral(":1.10"),
            QStringLiteral("epoch-A"), 3);
  QCOMPARE(snapshots.size(), 1);

  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-A"), 4);
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-A"), 2));
  QCOMPARE(client.state(), ClientState::Degraded);
  QCOMPARE(snapshots.size(), 1);

  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-A"), 4);
  auto hybrid = testSnapshot(QStringLiteral("epoch-A"), 3);
  hybrid.outputs[0].label = QStringLiteral("hybrid");
  transport.replySnapshot(transport.fetches.constLast(), hybrid);
  QCOMPARE(client.reasonCode(), QStringLiteral("snapshot-rejected"));
  QCOMPARE(snapshots.size(), 1);

  client.refresh();
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-B"), 1));
  QCOMPARE(client.reasonCode(), QStringLiteral("lineage-mismatch"));
  QCOMPARE(snapshots.size(), 1);

  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-B"), 1);
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-B"), 1));
  QCOMPARE(snapshots.size(), 2);
  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-A"), 4);
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-A"), 4));
  QCOMPARE(snapshots.size(), 3);
}

QTEST_MAIN(DisplayClientLineageTest)
#include "tst_display_client_lineage.moc"
