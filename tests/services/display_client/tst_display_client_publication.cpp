// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_protocol/display_limits.h>

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
  QCOMPARE(client.state(), ClientState::Ready);
}
} // namespace

class DisplayClientPublicationTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void publicationIsAtomicAndCoalesced();
  void unavailableDropsLastKnownGoodAndEmptyOwnerNeverPolls();
  void serviceErrorDropsLastKnownGood();
  void hostilePayloadsNeverPublish_data();
  void hostilePayloadsNeverPublish();
};

void DisplayClientPublicationTest::publicationIsAtomicAndCoalesced() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);
  makeReady(transport, client);
  QCOMPARE(snapshots.size(), 1);

  client.refresh();
  const auto duplicateFetch = transport.fetches.constLast();
  transport.replySnapshot(duplicateFetch,
                          testSnapshot(QStringLiteral("epoch-A"), 1));
  QCOMPARE(snapshots.size(), 1);

  client.refresh();
  const auto first = transport.fetches.constLast();
  for (int i = 0; i < 5; ++i) {
    transport.publishInvalidation(QStringLiteral(":1.10"),
                                  QStringLiteral("epoch-A"), 2);
  }
  const qsizetype beforeReply = transport.fetches.size();
  transport.replySnapshot(first, testSnapshot(QStringLiteral("epoch-A"), 2));
  QCOMPARE(transport.fetches.size(), beforeReply + 1);
  transport.replySnapshot(transport.fetches.constLast(),
                          testSnapshot(QStringLiteral("epoch-A"), 2));
  QCOMPARE(snapshots.size(), 2);
  QCOMPARE(client.snapshot()->revision, quint64(2));
}

void DisplayClientPublicationTest::serviceErrorDropsLastKnownGood() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QVERIFY(client.hasSnapshot());

  client.refresh();
  const qsizetype fetchCount = transport.fetches.size();
  transport.replySnapshot(transport.fetches.constLast(), {}, false,
                          QStringLiteral("service-unavailable"));

  QCOMPARE(client.state(), ClientState::Unavailable);
  QVERIFY(!client.hasSnapshot());
  QTest::qWait(30);
  QCOMPARE(transport.fetches.size(), fetchCount);
}

void DisplayClientPublicationTest::
    unavailableDropsLastKnownGoodAndEmptyOwnerNeverPolls() {
  FakeDisplayTransport transport;
  Client client(&transport);
  makeReady(transport, client);
  QVERIFY(client.hasSnapshot());

  const qsizetype fetchesBefore = transport.fetches.size();
  client.refresh();
  const auto retiredFetch = transport.fetches.constLast();
  transport.publishInvalidation(QStringLiteral(":1.10"),
                                QStringLiteral("epoch-A"), 2, false);
  QCOMPARE(client.state(), ClientState::Unavailable);
  QVERIFY(!client.hasSnapshot());
  transport.replySnapshot(retiredFetch,
                          testSnapshot(QStringLiteral("epoch-A"), 2));
  QVERIFY(!client.hasSnapshot());
  QTest::qWait(30);
  QCOMPARE(transport.fetches.size(), fetchesBefore + 1);

  transport.publishOwner({});
  QCOMPARE(client.state(), ClientState::Unavailable);
  client.refresh();
  QTest::qWait(30);
  QCOMPARE(transport.fetches.size(), fetchesBefore + 1);
  QCOMPARE(transport.activationRequests, 1);
}

void DisplayClientPublicationTest::hostilePayloadsNeverPublish_data() {
  QTest::addColumn<QString>("fault");
  QTest::newRow("too-many-outputs") << QStringLiteral("count");
  QTest::newRow("oversized-stable-id") << QStringLiteral("text");
  QTest::newRow("invalid-scale") << QStringLiteral("scale");
  QTest::newRow("bad-fingerprint") << QStringLiteral("fingerprint");
  QTest::newRow("unsupported-version") << QStringLiteral("version");
  QTest::newRow("no-primary") << QStringLiteral("primary");
}

void DisplayClientPublicationTest::hostilePayloadsNeverPublish() {
  QFETCH(QString, fault);
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);
  makeReady(transport, client);

  client.refresh();
  auto hostile = testSnapshot(QStringLiteral("epoch-A"), 2);
  if (fault == QStringLiteral("count")) {
    while (hostile.outputs.size() <= Display::kMaxOutputs) {
      auto output = hostile.outputs.constFirst();
      output.stableId = QStringLiteral("output-%1").arg(hostile.outputs.size());
      output.primary = false;
      hostile.outputs.push_back(output);
    }
  } else if (fault == QStringLiteral("text")) {
    hostile.outputs[0].stableId = QString(200, QLatin1Char('x'));
  } else if (fault == QStringLiteral("scale")) {
    hostile.outputs[0].scale = 99.0;
  } else if (fault == QStringLiteral("fingerprint")) {
    hostile.liveFingerprint = QByteArray(31, 'x');
  } else if (fault == QStringLiteral("version")) {
    hostile.protocolVersion = 2;
  } else if (fault == QStringLiteral("primary")) {
    hostile.outputs[0].primary = false;
  }
  transport.replySnapshot(transport.fetches.constLast(), hostile);
  QCOMPARE(snapshots.size(), 1);
  QCOMPARE(client.state(), ClientState::Degraded);
  QCOMPARE(client.reasonCode(), QStringLiteral("malformed-reply"));
  QCOMPARE(client.snapshot()->revision, quint64(1));
}

QTEST_MAIN(DisplayClientPublicationTest)
#include "tst_display_client_publication.moc"
