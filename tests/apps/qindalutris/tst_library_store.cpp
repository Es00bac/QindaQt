// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTest>

#include "library_store.h"

using namespace QindaQt::QindaLutris;

// App-local store contract (ADR-0231 on ADR-0198): exact schema, atomic
// writes, whole-document refusal, defaults on refusal.
class tst_library_store : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void absentIsFirstRun() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    LibraryStore::Error error = LibraryStore::Error::None;
    QVERIFY(store.readWineEntries(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Absent);
    QVERIFY(store.readLaunchOptions(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Absent);
  }

  void wineEntriesRoundTrip() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    const QVector<WineEntryRecord> records{
        {QStringLiteral("alpha-1234abcd"), QStringLiteral("Alpha Game"),
         QStringLiteral("/games/alpha/game.exe"),
         QStringLiteral("/games/alpha/prefix"), WineRunner::Wine, {}, {}},
        {QStringLiteral("beta-5678efab"), QStringLiteral("Beta Game"),
         QStringLiteral("/games/beta/run.exe"), QString(),
         WineRunner::Proton, QStringLiteral("GE-Proton11-6"),
         QStringLiteral("1756415527 GE-Proton11-6")},
    };
    QCOMPARE(store.writeWineEntries(records), LibraryStore::Error::None);
    LibraryStore::Error error = LibraryStore::Error::None;
    QCOMPARE(store.readWineEntries(&error), records);
    QCOMPARE(error, LibraryStore::Error::None);
  }

  void launchOptionsRoundTrip() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    QHash<QString, LaunchOptions> options;
    LaunchOptions tuned;
    tuned.gamemode = true;
    tuned.mangohud = true;
    tuned.targetDisplay = QStringLiteral("DP-1");
    tuned.extraEnvironment = {QStringLiteral("DXVK_HUD=compiler"),
                              QStringLiteral("MANGOHUD_CONFIG=fps")};
    options.insert(QStringLiteral("steam/10"), tuned);
    LaunchOptions wineOptions;
    wineOptions.runnerOverride = WineRunner::Proton;
    wineOptions.prefixOverride = QStringLiteral("/prefixes/custom");
    options.insert(QStringLiteral("wine/alpha-1234abcd"), wineOptions);
    QCOMPARE(store.writeLaunchOptions(options), LibraryStore::Error::None);
    LibraryStore::Error error = LibraryStore::Error::None;
    QCOMPARE(store.readLaunchOptions(&error), options);
    QCOMPARE(error, LibraryStore::Error::None);
  }

  void newerVersionIsRefusedWhole() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    QFile file(store.launchOptionsPath());
    QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"version\": 99, \"games\": {}}");
    file.close();
    LibraryStore::Error error = LibraryStore::Error::None;
    QVERIFY(store.readLaunchOptions(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Refused);
  }

  void outOfSetValueIsRefusedNotSnapped() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    QFile file(store.launchOptionsPath());
    QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    // runner "dosbox" is outside the documented set.
    file.write("{\"version\": 1, \"games\": {\"wine/x\": {\"runner\": \"dosbox\"}}}");
    file.close();
    LibraryStore::Error error = LibraryStore::Error::None;
    QVERIFY(store.readLaunchOptions(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Refused);
  }

  void symlinkStoreIsRefused() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QFile real(home.path() + QStringLiteral("/real.json"));
    QVERIFY(real.open(QIODevice::WriteOnly));
    real.write("{\"version\": 1, \"entries\": []}");
    real.close();
    const LibraryStore store(home.path());
    QVERIFY(QFile::link(real.fileName(), store.wineEntriesPath()));
    LibraryStore::Error error = LibraryStore::Error::None;
    QVERIFY(store.readWineEntries(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Refused);
  }

  // Wine entries saved before ADR-0275 recorded versions still load;
  // a present protonVersion of the wrong type refuses the document.
  void legacyWineEntryWithoutVersionLoads() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const LibraryStore store(home.path());
    QFile file(store.wineEntriesPath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"version\":1,\"entries\":[{\"title\":\"Old\",\"slug\":\"old-1\","
               "\"executable\":\"/g/old.exe\",\"prefix\":\"/g/p\",\"runner\":\"proton\","
               "\"proton\":\"\"}]}");
    file.close();
    LibraryStore::Error error = LibraryStore::Error::None;
    const QVector<WineEntryRecord> records = store.readWineEntries(&error);
    QCOMPARE(error, LibraryStore::Error::None);
    QCOMPARE(records.size(), 1);
    QVERIFY(records.first().protonPath.isEmpty());
    QVERIFY(records.first().protonVersion.isEmpty());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("{\"version\":1,\"entries\":[{\"title\":\"Old\",\"slug\":\"old-1\","
               "\"executable\":\"/g/old.exe\",\"prefix\":\"\",\"runner\":\"proton\","
               "\"proton\":\"GE-Proton9-1\",\"protonVersion\":7}]}");
    file.close();
    QVERIFY(store.readWineEntries(&error).isEmpty());
    QCOMPARE(error, LibraryStore::Error::Refused);
  }

  // The shared writer refuses a symlinked destination for this store too.
  void writeRefusesASymlinkedDestination() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString victim = home.path() + QStringLiteral("/victim.txt");
    QFile real(victim);
    QVERIFY(real.open(QIODevice::WriteOnly));
    real.write("precious");
    real.close();
    const LibraryStore store(home.path());
    QVERIFY(QFile::link(victim, store.launchOptionsPath()));
    QCOMPARE(store.writeLaunchOptions({}), LibraryStore::Error::WriteFailed);
    QVERIFY(QFile::link(victim, store.wineEntriesPath()));
    QCOMPARE(store.writeWineEntries({}), LibraryStore::Error::WriteFailed);
    QVERIFY(real.open(QIODevice::ReadOnly));
    QCOMPARE(real.readAll(), QByteArray("precious"));
  }

  void environmentAssignmentValidation() {
    QVERIFY(isValidEnvironmentAssignment(QStringLiteral("DXVK_HUD=compiler")));
    QVERIFY(isValidEnvironmentAssignment(QStringLiteral("_A=1")));
    QVERIFY(!isValidEnvironmentAssignment(QStringLiteral("1BAD=1")));
    QVERIFY(!isValidEnvironmentAssignment(QStringLiteral("NO_EQUALS")));
    QVERIFY(!isValidEnvironmentAssignment(QStringLiteral("=novalue")));
    QVERIFY(!isValidEnvironmentAssignment(QStringLiteral("HAS SPACE=1")));
    QVERIFY(!isValidEnvironmentAssignment(QStringLiteral("BAD\nLINE=1")));
  }

  void wineSlugIsStableAndDistinct() {
    QCOMPARE(wineSlugFor(QStringLiteral("My Game"), QStringLiteral("/a/my.exe")),
             wineSlugFor(QStringLiteral("my game"), QStringLiteral("/a/my.exe")));
    QVERIFY(wineSlugFor(QStringLiteral("My Game"), QStringLiteral("/a/my.exe"))
            != wineSlugFor(QStringLiteral("My Game"), QStringLiteral("/b/my.exe")));
  }
};

QTEST_GUILESS_MAIN(tst_library_store)
#include "tst_library_store.moc"
