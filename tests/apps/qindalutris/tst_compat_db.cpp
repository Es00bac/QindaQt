// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

#include "compat_db.h"

using namespace QindaQt::QindaLutris;

// The compat-db-v1 document contract (ADR-0275 §3): exact bounded schema,
// whole-document refusal, and the shipped/refreshed newer-wins rule. The
// fixtures under data/compat/ are shared with the Python validator's tests
// (tools/qindalutris-compat/tests), so both sides judge the same bytes.
namespace {

// The fixed clock every fixture is judged against (newer.json is stamped
// 2026-10-01, which must not count as "in the future").
const QDateTime kNow(QDate(2026, 10, 2), QTime(0, 0), QTimeZone::UTC);

QString fixture(const QString &name) {
  return QStringLiteral(QINDALUTRIS_COMPAT_FIXTURES) + QLatin1Char('/') + name;
}

CompatDatabase load(const QString &path, CompatLoadError *error) {
  return loadCompatDatabase(path, error, kNow);
}

bool writeFile(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray minimalWithGames(int count) {
  QByteArray json = R"({"schema":"qindalutris-compat-db","version":1,)"
                    R"("generated":"2026-09-01T00:00:00Z","sources":[],)"
                    R"("defaults":{"recommendedBuild":""},"builds":{},"games":[)";
  for (int i = 0; i < count; ++i) {
    if (i > 0) json += ',';
    json += R"({"id":"g)" + QByteArray::number(i) + R"(","title":"G","keys":{}})";
  }
  return json + "]}";
}

} // namespace

class tst_compat_db : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void validDocumentLoadsEveryField() {
    CompatLoadError error = CompatLoadError::Refused;
    const CompatDatabase db = load(fixture(QStringLiteral("valid/basic.json")), &error);
    QCOMPARE(error, CompatLoadError::None);
    QVERIFY(db.isLoaded());
    QCOMPARE(db.generated(), QDateTime(QDate(2026, 9, 25), QTime(12, 0), QTimeZone::UTC));
    QCOMPARE(db.sources().size(), 2);
    QCOMPARE(db.sources().first().id, QStringLiteral("umu-database"));
    QCOMPARE(db.sources().first().retrieved,
             QDateTime(QDate(2026, 9, 25), QTime(11, 0), QTimeZone::UTC));
    QCOMPARE(db.recommendedBuild(), QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(db.games().size(), 12);
    const CompatGame &wow = db.games().first();
    QCOMPARE(wow.id, QStringLiteral("world-of-warcraft"));
    QCOMPARE(wow.keys.storeIds.value(CompatStore::Battlenet), QStringList{QStringLiteral("wow")});
    QCOMPARE(wow.keys.exeNames, (QStringList{QStringLiteral("Wow.exe"), QStringLiteral("WowClassic.exe")}));
    QCOMPARE(wow.recommendedBuild, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(wow.avoid.size(), 1);
    QCOMPARE(wow.avoid.first().build, QStringLiteral("GE-Proton11-7-x86_64"));
    QCOMPARE(wow.environment.last(), QStringLiteral("WINEDLLOVERRIDES=locationapi=d"));
    QCOMPARE(wow.winetricks, (QStringList{QStringLiteral("corefonts"), QStringLiteral("renderer=vulkan")}));
    QCOMPARE(wow.arguments, QStringList{QStringLiteral("--exec=launch WoW")});
    QCOMPARE(wow.antiCheat, AntiCheatStatus::Running);
    QCOMPARE(wow.antiCheatNotes, QStringLiteral("Anti-cheat: Warden."));
    QCOMPARE(wow.umuStore, QStringLiteral("battlenet"));
    QCOMPARE(wow.links.size(), 2);
    const CompatGame &bl3 = db.games().at(1);
    QCOMPARE(bl3.keys.umuId, QStringLiteral("umu-397540"));
    QCOMPARE(bl3.protondbTier, ProtonDbTier::Gold);
    QCOMPARE(bl3.keys.storeIds.value(CompatStore::Egs), QStringList{QStringLiteral("Catnip")});
    QCOMPARE(bl3.steamDeck, SteamDeckCategory::Playable);
    QCOMPARE(bl3.steamDeckNotes, QStringList{QStringLiteral("Interface text is not legible")});
    QCOMPARE(wow.steamDeck, SteamDeckCategory::Unknown);
    QVERIFY(wow.steamDeckNotes.isEmpty());
  }

  void minimalDocumentIsLoadedButEmpty() {
    CompatLoadError error = CompatLoadError::Refused;
    const CompatDatabase db = load(fixture(QStringLiteral("valid/minimal.json")), &error);
    QCOMPARE(error, CompatLoadError::None);
    QVERIFY(db.isLoaded());
    QVERIFY(db.games().isEmpty());
    QVERIFY(db.recommendedBuild().isEmpty());
  }

  void everyRefusedFixtureIsRefusedWhole_data() {
    QTest::addColumn<QString>("path");
    const QDir dir(fixture(QStringLiteral("refused")));
    const QStringList names = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVERIFY(names.size() >= 60);
    for (const QString &name : names) {
      QTest::newRow(qPrintable(name)) << dir.filePath(name);
    }
  }
  void everyRefusedFixtureIsRefusedWhole() {
    QFETCH(QString, path);
    CompatLoadError error = CompatLoadError::None;
    const CompatDatabase db = load(path, &error);
    QCOMPARE(error, CompatLoadError::Refused);
    QVERIFY(!db.isLoaded());
    QVERIFY(db.games().isEmpty());
    QVERIFY(db.recommendedBuild().isEmpty());
  }

  void absentFileIsAbsentNotRefused() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CompatLoadError error = CompatLoadError::None;
    QVERIFY(!load(dir.filePath(QStringLiteral("nope.json")), &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Absent);
  }

  void symlinkIsRefusedBeforeReading() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString link = dir.filePath(QStringLiteral("compat-db-v1.json"));
    QVERIFY(QFile::link(fixture(QStringLiteral("valid/basic.json")), link));
    CompatLoadError error = CompatLoadError::None;
    QVERIFY(!load(link, &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Refused);
    const QString dangling = dir.filePath(QStringLiteral("dangling.json"));
    QVERIFY(QFile::link(dir.filePath(QStringLiteral("missing")), dangling));
    QVERIFY(!load(dangling, &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Refused);
  }

  void directoryIsRefused() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CompatLoadError error = CompatLoadError::None;
    QVERIFY(!load(dir.path(), &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Refused);
  }

  void oversizeDocumentIsRefused() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A valid document padded past the cap with trailing whitespace: only
    // the size rule can refuse it.
    QFile source(fixture(QStringLiteral("valid/minimal.json")));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QByteArray bytes = source.readAll();
    bytes.append(QByteArray(int(kMaxCompatDbBytes - bytes.size() + 1), ' '));
    const QString path = dir.filePath(QStringLiteral("big.json"));
    QVERIFY(writeFile(path, bytes));
    CompatLoadError error = CompatLoadError::None;
    QVERIFY(!load(path, &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Refused);
    QVERIFY(!parseCompatDocument(bytes, kNow).has_value());
    bytes.chop(1);
    QVERIFY(parseCompatDocument(bytes, kNow).has_value());
  }

  void tooManyGamesIsRefused() {
    QVERIFY(parseCompatDocument(minimalWithGames(3), kNow).has_value());
    QVERIFY(!parseCompatDocument(minimalWithGames(kMaxCompatGames + 1), kNow).has_value());
  }

  void newerGeneratedStampWins() {
    CompatLoadError error = CompatLoadError::None;
    const CompatDatabase older = load(fixture(QStringLiteral("valid/basic.json")), &error);
    const CompatDatabase newer = load(fixture(QStringLiteral("valid/newer.json")), &error);
    QVERIFY(older.isLoaded() && newer.isLoaded());
    QCOMPARE(chooseNewer(older, newer, kNow).generated(), newer.generated());
    QCOMPARE(chooseNewer(newer, older, kNow).generated(), newer.generated());
    QCOMPARE(chooseNewer(older, newer, kNow).recommendedBuild(), QStringLiteral("Proton 9.0 (Beta)"));
  }

  void refusedOrAbsentCopyNeverWins() {
    CompatLoadError error = CompatLoadError::None;
    const CompatDatabase shipped = load(fixture(QStringLiteral("valid/basic.json")), &error);
    const CompatDatabase refused = load(fixture(QStringLiteral("refused/wrong-schema.json")), &error);
    QCOMPARE(error, CompatLoadError::Refused);
    QCOMPARE(chooseNewer(shipped, refused, kNow).generated(), shipped.generated());
    QCOMPARE(chooseNewer(refused, shipped, kNow).generated(), shipped.generated());
    QVERIFY(!chooseNewer(CompatDatabase(), CompatDatabase(), kNow).isLoaded());
    // A tie keeps the shipped copy (the overlay package is the vetted one).
    const CompatDatabase same = load(fixture(QStringLiteral("valid/basic.json")), &error);
    QCOMPARE(chooseNewer(shipped, same, kNow).generated(), shipped.generated());
  }

  void effectiveDatabaseReadsBothInjectedPaths() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString refreshed = refreshedCompatDatabasePath(home.path());
    QCOMPARE(refreshed, home.path() + QStringLiteral("/qindalutris/compat-db-v1.json"));
    const QString shipped = fixture(QStringLiteral("valid/basic.json"));
    QCOMPARE(loadEffectiveCompatDatabase(shipped, refreshed, kNow).generated(),
             QDateTime(QDate(2026, 9, 25), QTime(12, 0), QTimeZone::UTC));
    QVERIFY(QDir().mkpath(home.filePath(QStringLiteral("qindalutris"))));
    QVERIFY(QFile::copy(fixture(QStringLiteral("valid/newer.json")), refreshed));
    QCOMPARE(loadEffectiveCompatDatabase(shipped, refreshed, kNow).generated(),
             QDateTime(QDate(2026, 10, 1), QTime(0, 0), QTimeZone::UTC));
    QVERIFY(!loadEffectiveCompatDatabase(home.filePath(QStringLiteral("x")),
                                         home.filePath(QStringLiteral("y")), kNow).isLoaded());
  }

  void shippedPathIsUnderTheInstallDatadir() {
    QVERIFY(shippedCompatDatabasePath().startsWith(QLatin1Char('/')));
    QVERIFY(shippedCompatDatabasePath().endsWith(QStringLiteral("/qindalutris/compat-db-v1.json")));
  }

  void fifoIsRefusedWithoutBlocking() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fifo = dir.filePath(QStringLiteral("compat-db-v1.json"));
    QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
    CompatLoadError error = CompatLoadError::None;
    QVERIFY(!load(fifo, &error).isLoaded());
    QCOMPARE(error, CompatLoadError::Refused);
  }

  // AGENT-GUARD: the committed snapshot is what ships; if the generator and
  // this parser drift apart, this row fails before a release does. Judged
  // against the real clock, as the app will.
  void committedSnapshotLoads() {
    CompatLoadError error = CompatLoadError::Refused;
    const CompatDatabase db =
        loadCompatDatabase(QStringLiteral(QINDALUTRIS_COMPAT_SNAPSHOT), &error);
    QCOMPARE(error, CompatLoadError::None);
    QVERIFY(db.games().size() > 1000);
    QCOMPARE(db.recommendedBuild(), QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(db.buildStatus(QStringLiteral("GE-Proton11-7-x86_64")), CompatBuildStatus::KnownIssues);
    GameKeys wow;
    wow.store = CompatStore::Battlenet;
    wow.storeId = QStringLiteral("wow");
    const std::optional<CompatAdvice> advice = db.lookup(wow);
    QVERIFY(advice.has_value());
    QCOMPARE(advice->game.id, QStringLiteral("world-of-warcraft"));
    QVERIFY(!avoidReasonFor(*advice, QStringLiteral("GE-Proton11-7-x86_64")).isEmpty());
    QCOMPARE(recommendedBuildFor(db, advice), QStringLiteral("GE-Proton11-6-x86_64"));
  }
};

QTEST_GUILESS_MAIN(tst_compat_db)
#include "tst_compat_db.moc"
