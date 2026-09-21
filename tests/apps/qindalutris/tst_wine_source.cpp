// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTest>

#include "wine_source.h"

using namespace QindaQt::QindaLutris;

// Manual Wine entries (ADR-0231): stored records become games, sizes are
// honest, missing executables degrade to size-less games, and a non-PE file
// yields no cover rather than an error.
class tst_wine_source : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void recordsBecomeGames() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QFile exe(home.path() + QStringLiteral("/game.exe"));
    QVERIFY(exe.open(QIODevice::WriteOnly));
    exe.write(QByteArray(2048, 'x')); // not a PE: no icon, but a real size
    exe.close();

    const QVector<WineEntryRecord> records{
        {QStringLiteral("alpha-1234abcd"), QStringLiteral("Alpha"),
         exe.fileName(), QStringLiteral("/prefix"), WineRunner::Wine, {}},
    };
    const QVector<Game> games =
        gamesFromWineEntries(records, home.path() + QStringLiteral("/cache"));
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.at(0).id, QStringLiteral("wine/alpha-1234abcd"));
    QCOMPARE(games.at(0).source, GameSource::Wine);
    QCOMPARE(games.at(0).installPath, exe.fileName());
    QCOMPARE(games.at(0).winePrefix, QStringLiteral("/prefix"));
    QVERIFY(games.at(0).installSizeBytes.has_value());
    QCOMPARE(*games.at(0).installSizeBytes, quint64(2048));
    QVERIFY(games.at(0).coverPath.isEmpty()); // not a PE: honest, no error
  }

  void missingExecutableStillLists() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QVector<WineEntryRecord> records{
        {QStringLiteral("gone-1234abcd"), QStringLiteral("Gone"),
         home.path() + QStringLiteral("/missing.exe"), QString(),
         WineRunner::Proton, {}},
    };
    const QVector<Game> games =
        gamesFromWineEntries(records, home.path() + QStringLiteral("/cache"));
    QCOMPARE(games.size(), 1);
    QVERIFY(!games.at(0).installSizeBytes.has_value());
    QVERIFY(games.at(0).coverPath.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_wine_source)
#include "tst_wine_source.moc"
