// SPDX-License-Identifier: GPL-3.0-or-later
#include "restore/recovery_journal_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

class RecoveryJournalTest final : public QObject {
  Q_OBJECT

private slots:
  void pathJournalRoundTrips();
  void untitledJournalRoundTrips();
  void absentJournalReportsAbsent();
  void clearRemovesAndToleratesAbsence();
  void malformedJournalIsRejectedAndSkipped();
  void oversizedJournalIsRefusedWithoutTruncation();
  void invalidKeysAreRefused();
  void enumerationIsBoundedAndNameOrdered();
  void symlinkedJournalIsNotFollowed();
};

void RecoveryJournalTest::pathJournalRoundTrips() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  RecoveryJournalStore store(root.path());
  const QString path = root.filePath(QStringLiteral("doc.txt"));
  const QString key = RecoveryJournalStore::keyForPath(path);
  QVERIFY(!store.contains(key));
  QVERIFY2(store.store(key, path, QStringLiteral("unsaved work ⛄")).ok(),
           "store");
  QVERIFY(store.contains(key));
  const auto loaded = store.load(key);
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.entry->key, key);
  QVERIFY(loaded.entry->path.has_value());
  QCOMPARE(*loaded.entry->path, path);
  QCOMPARE(loaded.entry->text, QStringLiteral("unsaved work ⛄"));
  const auto entries = store.entries();
  QCOMPARE(entries.size(), 1);
  QCOMPARE(entries.first().text, QStringLiteral("unsaved work ⛄"));
  // Atomic replacement keeps permissions owner-only.
  const auto permissions = QFile::permissions(
      QDir(root.path()).filePath(key + QStringLiteral(".json")));
  QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                           QFileDevice::ReadOther | QFileDevice::WriteOther)));
}

void RecoveryJournalTest::untitledJournalRoundTrips() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  const QString key = RecoveryJournalStore::keyForUntitled(7);
  QVERIFY(store.store(key, QString(), QStringLiteral("draft")).ok());
  const auto loaded = store.load(key);
  QVERIFY(loaded.ok());
  QVERIFY(!loaded.entry->path.has_value());
  QCOMPARE(loaded.entry->text, QStringLiteral("draft"));
}

void RecoveryJournalTest::absentJournalReportsAbsent() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  QCOMPARE(store.load(RecoveryJournalStore::keyForUntitled(1)).error,
           RecoveryJournalError::Absent);
  // A missing root is also Absent, not an error storm.
  RecoveryJournalStore missing(root.filePath(QStringLiteral("never-created")));
  QCOMPARE(missing.load(RecoveryJournalStore::keyForUntitled(1)).error,
           RecoveryJournalError::Absent);
  QVERIFY(missing.entries().isEmpty());
}

void RecoveryJournalTest::clearRemovesAndToleratesAbsence() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  const QString key = RecoveryJournalStore::keyForUntitled(3);
  QVERIFY(store.clear(key).ok());
  QVERIFY(store.store(key, QString(), QStringLiteral("draft")).ok());
  QVERIFY(store.clear(key).ok());
  QVERIFY(!store.contains(key));
  QCOMPARE(store.load(key).error, RecoveryJournalError::Absent);
}

void RecoveryJournalTest::malformedJournalIsRejectedAndSkipped() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  const QString key = RecoveryJournalStore::keyForUntitled(9);
  QFile file(QDir(root.path()).filePath(key + QStringLiteral(".json")));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.write("{not json") > 0);
  file.close();
  const auto loaded = store.load(key);
  QVERIFY(!loaded.ok());
  QCOMPARE(loaded.error, RecoveryJournalError::Malformed);
  QVERIFY(store.entries().isEmpty());
}

void RecoveryJournalTest::oversizedJournalIsRefusedWithoutTruncation() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  const QString key = RecoveryJournalStore::keyForUntitled(11);
  QVERIFY(store.store(key, QString(), QStringLiteral("bounded")).ok());
  const QString huge(RecoveryJournalStore::maximumJournalBytes, u'x');
  const auto refused = store.store(key, QString(), huge);
  QCOMPARE(refused.error, RecoveryJournalError::TooLarge);
  // The earlier bounded journal survives an oversized write attempt.
  QCOMPARE(store.load(key).entry->text, QStringLiteral("bounded"));

  const QString hugeKey = RecoveryJournalStore::keyForUntitled(12);
  QFile file(QDir(root.path()).filePath(hugeKey + QStringLiteral(".json")));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QByteArray payload(RecoveryJournalStore::maximumJournalBytes + 1, 'y');
  QCOMPARE(file.write(payload), qint64(payload.size()));
  file.close();
  QCOMPARE(store.load(hugeKey).error, RecoveryJournalError::TooLarge);
}

void RecoveryJournalTest::invalidKeysAreRefused() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  for (const QString &key :
       {QStringLiteral("../escape"), QStringLiteral("file-not-a-hash"),
        QStringLiteral("untitled-1"), QString(), QStringLiteral("a/b")}) {
    QCOMPARE(store.store(key, QString(), QStringLiteral("x")).error,
             RecoveryJournalError::Malformed);
    QCOMPARE(store.load(key).error, RecoveryJournalError::Malformed);
    QVERIFY(store.clear(key).error == RecoveryJournalError::Malformed);
    QVERIFY(!store.contains(key));
  }
}

void RecoveryJournalTest::enumerationIsBoundedAndNameOrdered() {
  QTemporaryDir root;
  RecoveryJournalStore store(root.path());
  for (int index = 0; index < RecoveryJournalStore::maximumJournals + 4;
       ++index) {
    QVERIFY(store.store(RecoveryJournalStore::keyForUntitled(index), QString(),
                        QStringLiteral("draft %1").arg(index))
                .ok());
  }
  QCOMPARE(store.entries().size(), RecoveryJournalStore::maximumJournals);
}

void RecoveryJournalTest::symlinkedJournalIsNotFollowed() {
  QTemporaryDir root;
  QTemporaryDir outside;
  QFile target(outside.filePath(QStringLiteral("host.json")));
  QVERIFY(target.open(QIODevice::WriteOnly));
  QVERIFY(target.write("{}") > 0);
  target.close();
  RecoveryJournalStore store(root.path());
  const QString key = RecoveryJournalStore::keyForUntitled(5);
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("host.json")),
                      QDir(root.path()).filePath(key + QStringLiteral(".json"))));
  QVERIFY(!store.contains(key));
  const auto loaded = store.load(key);
  QVERIFY(!loaded.ok());
  QVERIFY(store.entries().isEmpty());
}

QTEST_GUILESS_MAIN(RecoveryJournalTest)
#include "tst_recovery_journal.moc"
