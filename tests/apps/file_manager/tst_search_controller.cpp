// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/search_controller.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

// Shared fixture:
//   needle-one.txt
//   plain.txt
//   sub/needle-two.txt
//   sub/deep/Needle-Three.txt        (case-insensitive match)
//   .hidden-needle.txt               (hidden file)
//   sub/.hidden-dir/needle-four.txt  (below a hidden directory)
//   needle-loop -> .                 (symlink cycle; listed, never descended)
[[nodiscard]] bool seedFixture(const QDir &root) {
  if (!root.mkpath(QStringLiteral("sub/deep")) ||
      !root.mkpath(QStringLiteral("sub/.hidden-dir"))) {
    return false;
  }
  if (!writeFile(root.filePath(QStringLiteral("needle-one.txt")), "1") ||
      !writeFile(root.filePath(QStringLiteral("plain.txt")), "p") ||
      !writeFile(root.filePath(QStringLiteral("sub/needle-two.txt")), "2") ||
      !writeFile(root.filePath(QStringLiteral("sub/deep/Needle-Three.txt")),
                 "3") ||
      !writeFile(root.filePath(QStringLiteral(".hidden-needle.txt")), "h") ||
      !writeFile(root.filePath(QStringLiteral("sub/.hidden-dir/needle-four.txt")),
                 "4")) {
    return false;
  }
  return QFile::link(root.absolutePath(),
                     root.filePath(QStringLiteral("needle-loop")));
}

[[nodiscard]] QStringList namesOf(const QVector<DirectoryEntry> &entries) {
  QStringList names;
  names.reserve(entries.size());
  for (const DirectoryEntry &entry : entries) {
    names.append(entry.name);
  }
  names.sort();
  return names;
}

} // namespace

class TestSearchController final : public QObject {
  Q_OBJECT

private slots:
  void matchesNamesRecursively();
  void hiddenEntriesRequireOptIn();
  void symlinkMatchesButIsNeverDescended();
  void emptyQueryAndMissingRootAreRefused();
  void cancelSuppressesPendingResults();
  void supersededSearchNeverPublishes();
  void restartRerunsTheLastParameters();
};

void TestSearchController::matchesNamesRecursively() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  const quint64 token =
      controller.startSearch(fixture.path(), QStringLiteral("needle"), false);
  QVERIFY(token != 0);
  QVERIFY(controller.searching());
  QCOMPARE(controller.rootPath(), fixture.path());
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  QVERIFY(!controller.searching());

  const QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toULongLong(), token);
  const auto entries = args.at(1).value<QVector<DirectoryEntry>>();
  // The needle-loop symlink matches by name and is listed, but adds no
  // children of its own; hidden entries stay out without opt-in.
  QCOMPARE(namesOf(entries),
           QStringList({QStringLiteral("Needle-Three.txt"),
                        QStringLiteral("needle-loop"),
                        QStringLiteral("needle-one.txt"),
                        QStringLiteral("needle-two.txt")}));
  QCOMPARE(args.at(2).toString(), QStringLiteral("4 matches for \"needle\""));
  // Results carry the listing-time identity the mutation contract consumes.
  for (const DirectoryEntry &entry : entries) {
    QVERIFY(entry.device != 0);
    QVERIFY(entry.inode != 0);
    QVERIFY(entry.absolutePath.startsWith(fixture.path() + QLatin1Char('/')));
  }
}

void TestSearchController::hiddenEntriesRequireOptIn() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  const quint64 token =
      controller.startSearch(fixture.path(), QStringLiteral("needle"), true);
  QVERIFY(token != 0);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);

  const QList<QVariant> args = spy.takeFirst();
  const auto entries = args.at(1).value<QVector<DirectoryEntry>>();
  QCOMPARE(namesOf(entries),
           QStringList({QStringLiteral(".hidden-needle.txt"),
                        QStringLiteral("Needle-Three.txt"),
                        QStringLiteral("needle-four.txt"),
                        QStringLiteral("needle-loop"),
                        QStringLiteral("needle-one.txt"),
                        QStringLiteral("needle-two.txt")}));
  QCOMPARE(args.at(2).toString(), QStringLiteral("6 matches for \"needle\""));
}

void TestSearchController::symlinkMatchesButIsNeverDescended() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  QVERIFY(controller.startSearch(fixture.path(), QStringLiteral("needle"),
                                 false) != 0);
  // Terminating at all is half the proof: needle-loop points back at the
  // root, so descending symlinks would loop until the visited cap.
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);

  const QList<QVariant> args = spy.takeFirst();
  const auto entries = args.at(1).value<QVector<DirectoryEntry>>();
  bool sawSymlink = false;
  for (const DirectoryEntry &entry : entries) {
    QVERIFY(!entry.absolutePath.contains(QStringLiteral("needle-loop/")));
    if (entry.name == QLatin1String("needle-loop")) {
      sawSymlink = true;
      QVERIFY(entry.isSymlink);
    }
  }
  QVERIFY(sawSymlink);
}

void TestSearchController::emptyQueryAndMissingRootAreRefused() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  QCOMPARE(controller.startSearch(fixture.path(), QStringLiteral("   "), false),
           0ull);
  QCOMPARE(controller.startSearch(fixture.filePath(QStringLiteral("missing")),
                                  QStringLiteral("needle"), false),
           0ull);
  QVERIFY(!controller.searching());

  // A refused search forgets the previous parameters so a later restart()
  // cannot resurrect a listing the UI already dismissed.
  QVERIFY(controller.startSearch(fixture.path(), QStringLiteral("needle"),
                                 false) != 0);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  QCOMPARE(controller.startSearch(fixture.path(), QString(), false), 0ull);
  QVERIFY(controller.rootPath().isEmpty());
  QCOMPARE(controller.restart(), 0ull);
}

void TestSearchController::cancelSuppressesPendingResults() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  const quint64 token =
      controller.startSearch(fixture.path(), QStringLiteral("needle"), false);
  QVERIFY(token != 0);
  controller.cancel();
  QVERIFY(!controller.searching());
  // AGENT-NOTE: Deterministic only because resultsReady crosses by value and
  // cancel() invalidates the token first; a queued delivery from the joined
  // worker is dropped instead of publishing (or dereferencing it).
  QVERIFY(!spy.wait(400));

  // cancel() stops the search but keeps the parameters restart() replays.
  QVERIFY(controller.restart() != 0);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
}

void TestSearchController::supersededSearchNeverPublishes() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QSignalSpy spy(&controller, &SearchController::searchReady);
  const quint64 first =
      controller.startSearch(fixture.path(), QStringLiteral("needle"), false);
  const quint64 second =
      controller.startSearch(fixture.path(), QStringLiteral("plain"), false);
  QVERIFY(first != 0);
  QVERIFY(second != 0);
  QVERIFY(second != first);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);

  const QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toULongLong(), second);
  QCOMPARE(namesOf(args.at(1).value<QVector<DirectoryEntry>>()),
           QStringList({QStringLiteral("plain.txt")}));
  QCOMPARE(args.at(2).toString(), QStringLiteral("1 match for \"plain\""));
  // The superseded search's delivery, if any, is fenced by the live token.
  QVERIFY(!spy.wait(400));
}

void TestSearchController::restartRerunsTheLastParameters() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(seedFixture(QDir(fixture.path())));

  SearchController controller;
  QCOMPARE(controller.restart(), 0ull);

  QSignalSpy spy(&controller, &SearchController::searchReady);
  const quint64 token =
      controller.startSearch(fixture.path(), QStringLiteral("needle"), false);
  QVERIFY(token != 0);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  QCOMPARE(namesOf(spy.at(0).at(1).value<QVector<DirectoryEntry>>()).size(), 4);

  // A mutation inside the result set is picked up by the restart.
  QVERIFY(writeFile(
      QDir(fixture.path()).filePath(QStringLiteral("sub/needle-late.txt")),
      "5"));
  const quint64 again = controller.restart();
  QVERIFY(again > token);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
  const QList<QVariant> args = spy.at(1);
  QCOMPARE(args.at(0).toULongLong(), again);
  QCOMPARE(namesOf(args.at(1).value<QVector<DirectoryEntry>>()),
           QStringList({QStringLiteral("Needle-Three.txt"),
                        QStringLiteral("needle-late.txt"),
                        QStringLiteral("needle-loop"),
                        QStringLiteral("needle-one.txt"),
                        QStringLiteral("needle-two.txt")}));
}

QTEST_GUILESS_MAIN(TestSearchController)
#include "tst_search_controller.moc"
