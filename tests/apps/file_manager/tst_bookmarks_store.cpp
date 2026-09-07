// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/bookmarks_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] Bookmark bookmark(const QString &name, const QString &path) {
  return Bookmark{name, path};
}

// Writes raw bytes to the store's state file, creating the state directory
// first, so malformed/oversized on-disk fixtures can be built without going
// through the validating store() path.
[[nodiscard]] bool writeRawState(const BookmarksStore &store,
                                 const QByteArray &bytes) {
  if (!QDir().mkpath(QFileInfo(store.filePath()).absolutePath())) {
    return false;
  }
  QFile file(store.filePath());
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

class TestBookmarksStore final : public QObject {
  Q_OBJECT

private slots:
  void firstRunIsAbsentWithoutDiagnostic();
  void storeLoadRoundTrip();
  void secondStoreReplacesTheFirst();
  void malformedInventoryIsRejectedOnLoad_data();
  void malformedInventoryIsRejectedOnLoad();
  void outOfBoundsInventoryIsRejectedOnStore();
  void loadRejectsOversizedAndSymlinkedState();
  void symlinkedAncestorCannotEscapeStateRoot();
};

void TestBookmarksStore::firstRunIsAbsentWithoutDiagnostic() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  // A missing state directory and a missing state file are both the clean
  // first-run result: not ok(), but an empty bookmark list and no diagnostic.
  BookmarksStore missingRoot(directory.filePath(QStringLiteral("state")));
  const BookmarksLoadResult absentRoot = missingRoot.load();
  QCOMPARE(absentRoot.error, BookmarksError::Absent);
  QVERIFY(!absentRoot.ok());
  QVERIFY(absentRoot.bookmarks.isEmpty());
  QVERIFY(absentRoot.diagnostic.isEmpty());

  BookmarksStore emptyRoot(directory.path());
  const BookmarksLoadResult absentFile = emptyRoot.load();
  QCOMPARE(absentFile.error, BookmarksError::Absent);
  QVERIFY(absentFile.diagnostic.isEmpty());
}

void TestBookmarksStore::storeLoadRoundTrip() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  BookmarksStore store(directory.filePath(QStringLiteral("state")));
  const QVector<Bookmark> expected = {
      bookmark(QStringLiteral("Docs"), QStringLiteral("/home/user/docs")),
      bookmark(QStringLiteral("Pictures"), QStringLiteral("/home/user/pictures")),
  };
  QVERIFY2(store.store(expected).ok(), "store must succeed on a fresh root");

  const QFileDevice::Permissions permissions =
      QFileInfo(store.filePath()).permissions();
  QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                           QFileDevice::ReadOther | QFileDevice::WriteOther)));

  const BookmarksLoadResult loaded = store.load();
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.bookmarks, expected);
}

void TestBookmarksStore::secondStoreReplacesTheFirst() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  BookmarksStore store(directory.path());
  QVERIFY(store.store({bookmark(QStringLiteral("Old"), QStringLiteral("/old"))}).ok());
  const QVector<Bookmark> replacement = {
      bookmark(QStringLiteral("One"), QStringLiteral("/one")),
      bookmark(QStringLiteral("Two"), QStringLiteral("/two")),
  };
  QVERIFY(store.store(replacement).ok());
  const BookmarksLoadResult loaded = store.load();
  QVERIFY(loaded.ok());
  QCOMPARE(loaded.bookmarks, replacement);
}

void TestBookmarksStore::malformedInventoryIsRejectedOnLoad_data() {
  QTest::addColumn<QByteArray>("payload");
  QTest::addRow("not-json") << QByteArray("this is not json");
  QTest::addRow("json-array-not-object") << QByteArray(R"([{"name":"a","path":"/a"}])");
  QTest::addRow("wrong-version") << QByteArray(R"({"version":2,"bookmarks":[]})");
  QTest::addRow("extra-key") << QByteArray(
      R"({"version":1,"bookmarks":[],"future":true})");
  QTest::addRow("bookmarks-not-array") << QByteArray(R"({"version":1,"bookmarks":{}})");
  QTest::addRow("entry-not-object") << QByteArray(R"({"version":1,"bookmarks":["/a"]})");
  QTest::addRow("entry-missing-path")
      << QByteArray(R"({"version":1,"bookmarks":[{"name":"a"}]})");
  QTest::addRow("duplicate-path") << QByteArray(
      R"({"version":1,"bookmarks":[{"name":"a","path":"/a"},)"
      R"({"name":"b","path":"/a"}]})");
  QTest::addRow("relative-path") << QByteArray(
      R"({"version":1,"bookmarks":[{"name":"a","path":"relative/dir"}]})");
  QTest::addRow("empty-name") << QByteArray(
      R"({"version":1,"bookmarks":[{"name":"","path":"/a"}]})");
}

void TestBookmarksStore::malformedInventoryIsRejectedOnLoad() {
  QFETCH(QByteArray, payload);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  BookmarksStore store(directory.path());
  QVERIFY(writeRawState(store, payload));
  const BookmarksLoadResult loaded = store.load();
  QVERIFY(!loaded.ok());
  QCOMPARE(loaded.error, BookmarksError::Malformed);
  QVERIFY(loaded.bookmarks.isEmpty());
  QVERIFY(!loaded.diagnostic.isEmpty());
}

void TestBookmarksStore::outOfBoundsInventoryIsRejectedOnStore() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  BookmarksStore store(directory.path());

  // AGENT-CONTRACT: The same bounds guard both directions of the v1 schema
  // (128 entries, 4096-char absolute clean paths, 256-char non-empty names,
  // no duplicate cleaned paths); see BookmarksStore::validate. store()
  // reports a violation as Malformed and must not touch the on-disk file.
  QVector<Bookmark> tooMany;
  for (int i = 0; i < BookmarksStore::maximumBookmarks + 1; ++i) {
    tooMany.append(bookmark(QStringLiteral("b%1").arg(i),
                            QStringLiteral("/dir-%1").arg(i)));
  }
  QCOMPARE(store.store(tooMany).error, BookmarksError::Malformed);

  const QString longPath = QStringLiteral("/") + QString(4096, QLatin1Char('p'));
  QCOMPARE(store.store({bookmark(QStringLiteral("long"), longPath)}).error,
           BookmarksError::Malformed);
  QCOMPARE(store.store({bookmark(QStringLiteral("rel"),
                                 QStringLiteral("relative/dir"))})
               .error,
           BookmarksError::Malformed);
  QCOMPARE(store.store({bookmark(QStringLiteral("unclean"),
                                 QStringLiteral("/tmp/../etc"))})
               .error,
           BookmarksError::Malformed);
  QCOMPARE(store.store({bookmark(QStringLiteral("one"), QStringLiteral("/same")),
                        bookmark(QStringLiteral("two"), QStringLiteral("/same"))})
               .error,
           BookmarksError::Malformed);
  QCOMPARE(store.store({bookmark(QString(), QStringLiteral("/named"))}).error,
           BookmarksError::Malformed);
  QCOMPARE(store.store({bookmark(QString(257, QLatin1Char('n')),
                                 QStringLiteral("/named"))})
               .error,
           BookmarksError::Malformed);
  QVERIFY(!QFileInfo::exists(store.filePath()));
}

void TestBookmarksStore::loadRejectsOversizedAndSymlinkedState() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  BookmarksStore store(directory.path());

  QVERIFY(writeRawState(store, QByteArray(BookmarksStore::maximumBytes + 1, 'x')));
  QCOMPARE(store.load().error, BookmarksError::TooLarge);

  QVERIFY(QFile::remove(store.filePath()));
  const QString target = directory.filePath(QStringLiteral("target.json"));
  QVERIFY(writeRawState(BookmarksStore(directory.path()), QByteArray("{}")));
  QVERIFY(QFile::rename(store.filePath(), target));
  QVERIFY(QFile::link(target, store.filePath()));
  QCOMPARE(store.load().error, BookmarksError::Malformed);
}

void TestBookmarksStore::symlinkedAncestorCannotEscapeStateRoot() {
  // AGENT-GUARD: The openat/O_NOFOLLOW traversal must refuse every symlinked
  // directory ancestor so load/store cannot be redirected outside the injected
  // state root. Mirrors tst_restore_state.cpp's P1-2 regression.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString stateRoot = directory.filePath(QStringLiteral("state"));
  const QString escaped = directory.filePath(QStringLiteral("escaped"));
  QVERIFY(QDir().mkpath(stateRoot));
  QVERIFY(QDir().mkpath(escaped));
  QVERIFY(QFile::link(escaped,
                      QDir(stateRoot).filePath(QStringLiteral("qindaqt"))));

  BookmarksStore store(
      QDir(stateRoot).filePath(QStringLiteral("qindaqt/file-manager")));
  QCOMPARE(store.load().error, BookmarksError::InvalidRoot);
  const BookmarksWriteResult written =
      store.store({bookmark(QStringLiteral("a"), QStringLiteral("/a"))});
  QCOMPARE(written.error, BookmarksError::InvalidRoot);
  QVERIFY(!QFileInfo::exists(
      QDir(escaped).filePath(QStringLiteral("file-manager/bookmarks-v1.json"))));
}

QTEST_GUILESS_MAIN(TestBookmarksStore)
#include "tst_bookmarks_store.moc"
