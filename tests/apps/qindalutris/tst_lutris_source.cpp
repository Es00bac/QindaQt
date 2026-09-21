// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

#include "lutris_source.h"

using namespace QindaQt::QindaLutris;

// The synthetic pga.db used here is created by the test itself (ADR-0231);
// the operator's real database is never touched.
class tst_lutris_source : public QObject {
  Q_OBJECT

  // Builds a games table with the exact columns the reader requires, plus
  // one game per entry of rows: {id, name, slug, runner, directory, installed}.
  static QString makeDatabase(const QString &dir, const QString &fileName,
                              bool dropDirectoryColumn = false) {
    const QString path = dir + QLatin1Char('/') + fileName;
    const QString connection =
        QStringLiteral("fixture-%1").arg(quintptr(&dir), 0, 16);
    {
      QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  connection);
      db.setDatabaseName(path);
      if (!db.open()) {
        return {};
      }
      QSqlQuery query(db);
      if (dropDirectoryColumn) {
        query.exec("CREATE TABLE games (id INTEGER PRIMARY KEY, name TEXT, "
                   "slug TEXT, runner TEXT, installed INTEGER)");
        query.exec("INSERT INTO games VALUES (1, 'A', 'a', 'wine', 1)");
      } else {
        query.exec("CREATE TABLE games (id INTEGER PRIMARY KEY, name TEXT, "
                   "slug TEXT, runner TEXT, directory TEXT, installed INTEGER)");
        query.exec("INSERT INTO games VALUES (1, 'Installed One', 'one', "
                   "'wine', '/games/one', 1)");
        query.exec("INSERT INTO games VALUES (2, 'Not Installed', 'two', "
                   "'steam', '/games/two', 0)");
        query.exec("INSERT INTO games VALUES (3, NULL, 'null-name', 'wine', "
                   "'/games/null', 1)");
        query.exec("INSERT INTO games VALUES (4, 'Second Installed', "
                   "'second', 'linux', '', 1)");
      }
      db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return path;
  }

private Q_SLOTS:
  void installedRowsOnly() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString db = makeDatabase(home.path(), QStringLiteral("pga.db"));
    QVERIFY(!db.isEmpty());
    const LutrisDiscovery out = scanLutrisDatabase(db);
    QCOMPARE(out.games.size(), 2); // installed = 1, null name skipped
    QCOMPARE(out.games.at(0).title, QStringLiteral("Installed One"));
    QCOMPARE(out.games.at(0).id, QStringLiteral("lutris/1"));
    QCOMPARE(out.games.at(0).source, GameSource::Lutris);
    QCOMPARE(out.games.at(0).installPath, QStringLiteral("/games/one"));
    QCOMPARE(out.games.at(1).title, QStringLiteral("Second Installed"));
    QVERIFY(out.warnings.isEmpty());
  }

  void absentDatabase() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LutrisDiscovery out =
        scanLutrisDatabase(home.path() + QStringLiteral("/pga.db"));
    QVERIFY(out.games.isEmpty());
    QVERIFY(out.warnings.isEmpty()); // absence is normal, not degradation
  }

  void missingColumnYieldsNoGames() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString db = makeDatabase(home.path(), QStringLiteral("pga.db"),
                                    /*dropDirectoryColumn=*/true);
    QVERIFY(!db.isEmpty());
    const LutrisDiscovery out = scanLutrisDatabase(db);
    QVERIFY(out.games.isEmpty());
    QVERIFY(!out.warnings.isEmpty());
  }

  void notADatabase() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString path = home.path() + QStringLiteral("/pga.db");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not sqlite");
    file.close();
    const LutrisDiscovery out = scanLutrisDatabase(path);
    QVERIFY(out.games.isEmpty());
    QVERIFY(!out.warnings.isEmpty());
  }

  void coverFromSiblingDirectories() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString db = makeDatabase(home.path(), QStringLiteral("pga.db"));
    QVERIFY(!db.isEmpty());
    QVERIFY(QDir().mkpath(home.path() + QStringLiteral("/coverart")));
    QFile cover(home.path() + QStringLiteral("/coverart/one.jpg"));
    QVERIFY(cover.open(QIODevice::WriteOnly));
    cover.write("\xff\xd8\xff", 3);
    cover.close();
    const LutrisDiscovery out = scanLutrisDatabase(db);
    QCOMPARE(out.games.size(), 2);
    QVERIFY(out.games.at(0).coverPath.endsWith(
        QStringLiteral("coverart/one.jpg")));
    QVERIFY(out.games.at(1).coverPath.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_lutris_source)
#include "tst_lutris_source.moc"
