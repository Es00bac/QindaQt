// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/transfer_router.h"

#include <QTest>

using namespace QindaQt::Apps::FileManager;

class TestTransferRouter final : public QObject {
  Q_OBJECT

private slots:
  void normalizesBothRealms();
  void refusesUnusableEndpoints();
  void routesByRealm_data();
  void routesByRealm();
  void keepsTheOneChildRemotePathForExactlyItsCase();
  void refusesAFolderIntoItself();
  void refusesEmptyAndOversizedBatches();
};

void TestTransferRouter::normalizesBothRealms() {
  const auto local = TransferRouter::normalizeEndpoint(QStringLiteral("/home/cabewse"));
  QVERIFY(local.has_value());
  QCOMPARE(local->toString(), QStringLiteral("file:///home/cabewse"));
  QCOMPARE(TransferRouter::realmOf(*local), TransferRealm::Local);

  const auto remote =
      TransferRouter::normalizeEndpoint(QStringLiteral("  SFTP://QINDA/mnt/storage  "));
  QVERIFY(remote.has_value());
  QCOMPARE(remote->toString(), QStringLiteral("sftp://qinda/mnt/storage"));
  QCOMPARE(TransferRouter::realmOf(*remote), TransferRealm::Remote);
}

void TestTransferRouter::refusesUnusableEndpoints() {
  // AGENT-GUARD: a relative or uncleaned local path must be refused, never
  // repaired -- "/home/x/../etc" is not the folder the user typed.
  for (const QString &text : {QStringLiteral(""), QStringLiteral("   "),
                              QStringLiteral("relative/path"),
                              QStringLiteral("/home/cabewse/../etc"),
                              QStringLiteral("/home/cabewse/"),
                              QStringLiteral("sftp://user@qinda/mnt"),
                              QStringLiteral("sftp://user:secret@qinda/mnt"),
                              QStringLiteral("sftp:///mnt"),
                              QStringLiteral("sftp://qinda/mnt/../etc")}) {
    QVERIFY2(!TransferRouter::normalizeEndpoint(text).has_value(), qPrintable(text));
  }
  // An unsupported scheme is not a network location; it is also not an
  // absolute local path, so it is refused rather than treated as either.
  QVERIFY(!TransferRouter::normalizeEndpoint(QStringLiteral("ftp://qinda/mnt")).has_value());
}

void TestTransferRouter::routesByRealm_data() {
  QTest::addColumn<QStringList>("sources");
  QTest::addColumn<QString>("destination");
  QTest::addColumn<int>("expected");

  const auto route = [](TransferRoute value) { return static_cast<int>(value); };
  QTest::newRow("local to local")
      << QStringList{QStringLiteral("/home/cabewse/a.txt")}
      << QStringLiteral("/home/cabewse/Downloads") << route(TransferRoute::LocalMutation);
  QTest::newRow("local multi to local")
      << QStringList{QStringLiteral("/home/cabewse/a.txt"),
                     QStringLiteral("/home/cabewse/b.txt")}
      << QStringLiteral("/home/cabewse/Downloads") << route(TransferRoute::LocalMutation);
  QTest::newRow("local to remote")
      << QStringList{QStringLiteral("/home/cabewse/a.txt")}
      << QStringLiteral("sftp://qinda/mnt/storage") << route(TransferRoute::Queue);
  QTest::newRow("remote to local")
      << QStringList{QStringLiteral("sftp://qinda/mnt/storage/a.txt")}
      << QStringLiteral("/home/cabewse/Downloads") << route(TransferRoute::Queue);
  QTest::newRow("remote to another host")
      << QStringList{QStringLiteral("sftp://qinda/mnt/a.txt")}
      << QStringLiteral("sftp://qinda-top/home/cabewse") << route(TransferRoute::Queue);
  QTest::newRow("remote to another scheme")
      << QStringList{QStringLiteral("sftp://qinda/mnt/a.txt")}
      << QStringLiteral("smb://qinda/share") << route(TransferRoute::Queue);
  QTest::newRow("remote to another port")
      << QStringList{QStringLiteral("sftp://qinda/mnt/a.txt")}
      << QStringLiteral("sftp://qinda:2222/mnt") << route(TransferRoute::Queue);
  QTest::newRow("remote multi to same authority")
      << QStringList{QStringLiteral("sftp://qinda/mnt/a.txt"),
                     QStringLiteral("sftp://qinda/mnt/b.txt")}
      << QStringLiteral("sftp://qinda/mnt/backup") << route(TransferRoute::Queue);
  QTest::newRow("remote single to same authority")
      << QStringList{QStringLiteral("sftp://qinda/mnt/a.txt")}
      << QStringLiteral("sftp://qinda/mnt/backup")
      << route(TransferRoute::RemoteSameAuthorityChild);
  QTest::newRow("unusable destination")
      << QStringList{QStringLiteral("/home/cabewse/a.txt")}
      << QStringLiteral("not a path") << route(TransferRoute::Refuse);
  QTest::newRow("unusable source")
      << QStringList{QStringLiteral("ftp://qinda/a.txt")}
      << QStringLiteral("/home/cabewse") << route(TransferRoute::Refuse);
  QTest::newRow("one unusable source in a batch")
      << QStringList{QStringLiteral("/home/cabewse/a.txt"), QStringLiteral("relative")}
      << QStringLiteral("sftp://qinda/mnt") << route(TransferRoute::Refuse);
}

void TestTransferRouter::routesByRealm() {
  QFETCH(QStringList, sources);
  QFETCH(QString, destination);
  QFETCH(int, expected);
  const TransferRouting routing = TransferRouter::route(sources, destination);
  QCOMPARE(static_cast<int>(routing.route), expected);
  if (routing.route == TransferRoute::Refuse) {
    QVERIFY(!routing.message.isEmpty());
    QVERIFY(routing.sources.isEmpty());
    QVERIFY(routing.destinationFolder.isEmpty());
  } else {
    QVERIFY(routing.message.isEmpty());
    QCOMPARE(routing.sources.size(), sources.size());
    QVERIFY(!routing.destinationFolder.isEmpty());
  }
}

void TestTransferRouter::keepsTheOneChildRemotePathForExactlyItsCase() {
  // AGENT-GUARD: the ADR-0155/0156 owner must never be named for more than
  // one source -- that is the whole reason the destination dialog may now
  // open on a remote multi-selection.
  const TransferRouting single = TransferRouter::route(
      {QStringLiteral("sftp://qinda/mnt/a.txt")}, QStringLiteral("sftp://qinda/mnt/backup"));
  QCOMPARE(single.route, TransferRoute::RemoteSameAuthorityChild);

  const TransferRouting pair =
      TransferRouter::route({QStringLiteral("sftp://qinda/mnt/a.txt"),
                             QStringLiteral("sftp://qinda/mnt/b.txt")},
                            QStringLiteral("sftp://qinda/mnt/backup"));
  QCOMPARE(pair.route, TransferRoute::Queue);
}

void TestTransferRouter::refusesAFolderIntoItself() {
  const TransferRouting itself = TransferRouter::route(
      {QStringLiteral("sftp://qinda/mnt/storage")},
      QStringLiteral("sftp://qinda/mnt/storage"));
  QCOMPARE(itself.route, TransferRoute::Refuse);
  QVERIFY(itself.message.contains(QStringLiteral("being transferred")));

  const TransferRouting descendant = TransferRouter::route(
      {QStringLiteral("sftp://qinda/mnt/storage")},
      QStringLiteral("sftp://qinda/mnt/storage/backup"));
  QCOMPARE(descendant.route, TransferRoute::Refuse);
  QVERIFY(descendant.message.contains(QStringLiteral("into itself")));

  // A sibling whose name merely starts with the source's name is not inside
  // it, so it must still be accepted.
  const TransferRouting sibling = TransferRouter::route(
      {QStringLiteral("sftp://qinda/mnt/storage")},
      QStringLiteral("sftp://qinda/mnt/storage-backup"));
  QCOMPARE(sibling.route, TransferRoute::RemoteSameAuthorityChild);

  // The same folder name on a different host is a different folder.
  const TransferRouting otherHost = TransferRouter::route(
      {QStringLiteral("sftp://qinda/mnt/storage")},
      QStringLiteral("sftp://qinda-top/mnt/storage"));
  QCOMPARE(otherHost.route, TransferRoute::Queue);
}

void TestTransferRouter::refusesEmptyAndOversizedBatches() {
  QCOMPARE(TransferRouter::route({}, QStringLiteral("/home/cabewse")).route,
           TransferRoute::Refuse);

  QStringList many;
  for (int index = 0; index <= TransferRouter::maximumSources; ++index) {
    many.append(QStringLiteral("sftp://qinda/mnt/%1.txt").arg(index));
  }
  const TransferRouting routing = TransferRouter::route(many, QStringLiteral("/home/cabewse"));
  QCOMPARE(routing.route, TransferRoute::Refuse);
  QVERIFY(routing.message.contains(QString::number(TransferRouter::maximumSources)));
}

QTEST_MAIN(TestTransferRouter)
#include "tst_transfer_router.moc"
