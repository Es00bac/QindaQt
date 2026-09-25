// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "preferences_store.h"
#include "proton_fixture.h"
#include "title_factory.h"
#include "title_store.h"

using namespace QindaQt::QindaLutris;
using Origin = ProtonBuild::Origin;

// ADR-0275 sections 1-3 at install time: which build a NEW title is pinned
// to, how database advice lands in the record, adoption of an existing
// prefix, and the append-only titles document. The database is the
// committed snapshot, so these rows also prove the real WoW entry steers
// new installs away from GE-Proton11-7.
class tst_title_factory : public QObject {
  Q_OBJECT

  CompatDatabase m_db;

  static QVector<ProtonBuild> catalog() {
    return {ProtonFixture::build(QStringLiteral("GE-Proton11-7-x86_64"), Origin::System),
            ProtonFixture::build(QStringLiteral("GE-Proton11-6-x86_64"), Origin::System),
            ProtonFixture::build(QStringLiteral("Proton - Experimental"), Origin::Steam)};
  }

  static NewTitle wowFacts(const QString &prefix) {
    NewTitle facts;
    facts.title = QStringLiteral("World of Warcraft");
    facts.kind = TitleKind::StoreGame;
    facts.store = GameStore::BattleNet;
    facts.prefixPath = prefix;
    facts.executable = prefix + QStringLiteral("/drive_c/WoW/_retail_/Wow.exe");
    return facts;
  }

private slots:
  void initTestCase() {
    CompatLoadError error = CompatLoadError::None;
    m_db = loadCompatDatabase(QStringLiteral(QINDALUTRIS_COMPAT_SNAPSHOT), &error);
    QCOMPARE(error, CompatLoadError::None);
    QVERIFY(m_db.isLoaded());
  }

  void recipesMapToStores() {
    QCOMPARE(gameStoreForRecipe(QStringLiteral("battlenet")), GameStore::BattleNet);
    QCOMPARE(gameStoreForRecipe(QStringLiteral("egs-launcher")), GameStore::Egs);
    QCOMPARE(gameStoreForRecipe(QStringLiteral("amazon-games")), GameStore::Amazon);
    QCOMPARE(gameStoreForRecipe(QStringLiteral("nope")), GameStore::None);
  }

  void theDatabaseRecommendationWinsOverTheNewestBuild() {
    const auto advice =
        adviceForGame(&m_db, QStringLiteral("World of Warcraft"), QStringLiteral("Wow.exe"));
    QVERIFY(advice.has_value());
    // With no preference the newest system build (11-7) would be the default;
    // WoW's record recommends 11-6 and lists 11-7 as a build to avoid.
    const auto build = chooseBuildForNewTitle(catalog(), {}, &m_db, advice);
    QVERIFY(build.has_value());
    QCOMPARE(build->name, QStringLiteral("GE-Proton11-6-x86_64"));
  }

  void anAvoidedDefaultIsNeverPicked() {
    const auto advice =
        adviceForGame(&m_db, QStringLiteral("World of Warcraft"), QStringLiteral("Wow.exe"));
    // Only 11-7 (avoided) and a Steam rolling channel are installed.
    const QVector<ProtonBuild> builds{catalog().at(0), catalog().at(2)};
    QVERIFY(!chooseBuildForNewTitle(builds, {}, &m_db, advice).has_value());
  }

  void withoutAdviceTheUsersDefaultIsUsed() {
    const auto build =
        chooseBuildForNewTitle(catalog(), QStringLiteral("GE-Proton11-7-x86_64"), nullptr, {});
    QVERIFY(build.has_value());
    QCOMPARE(build->name, QStringLiteral("GE-Proton11-7-x86_64"));
  }

  void adoptionKeepsTheBuildThePrefixLastRanOn() {
    QTemporaryDir dir;
    ProtonFixture::writeFile(dir.filePath(QStringLiteral("version")), "GE-Proton11-6\n");
    // Real GE-Proton `version` files read "<timestamp> GE-Proton11-6" while the
    // prefix records just "GE-Proton11-6"; the last word is what matches.
    const QVector<ProtonBuild> builds{
        ProtonFixture::build(QStringLiteral("GE-Proton11-7-x86_64"), Origin::System,
                             QStringLiteral("1789520217 GE-Proton11-7")),
        ProtonFixture::build(QStringLiteral("GE-Proton11-6-x86_64"), Origin::System,
                             QStringLiteral("1787951532 GE-Proton11-6"))};
    const auto build = buildMatchingPrefix(dir.path(), builds);
    QVERIFY(build.has_value());
    QCOMPARE(build->name, QStringLiteral("GE-Proton11-6-x86_64"));
    ProtonFixture::writeFile(dir.filePath(QStringLiteral("version")), "GE-Proton9-1\n");
    QVERIFY(!buildMatchingPrefix(dir.path(), builds).has_value());
  }

  void aRecordCarriesThePinAndTheAdvice() {
    QTemporaryDir dir;
    const auto advice =
        adviceForGame(&m_db, QStringLiteral("World of Warcraft"), QStringLiteral("Wow.exe"));
    QString why;
    const auto record = makeTitleRecord(wowFacts(dir.path()), catalog().at(1), advice, {},
                                        QStringLiteral("2026-09-25"), &why);
    QVERIFY2(record.has_value(), qPrintable(why));
    QCOMPARE(record->protonBuild, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(record->protonBuildVersion, catalog().at(1).versionText);
    QCOMPARE(record->umuStore, QStringLiteral("battlenet"));
    QCOMPARE(record->id, QStringLiteral("title/world-of-warcraft"));
    for (const QString &line : record->environment) {
      QVERIFY(!isReservedUmuEnvironmentKey(line.section(QLatin1Char('='), 0, 0)));
    }
  }

  void appendIsAppendOnlyAndRefusesDuplicates() {
    QTemporaryDir dir;
    QString why;
    const auto record = makeTitleRecord(wowFacts(dir.filePath(QStringLiteral("Games/battlenet"))),
                                        catalog().at(1), {}, {}, QStringLiteral("2026-09-25"),
                                        &why);
    QVERIFY2(record.has_value(), qPrintable(why));
    QString error;
    QVERIFY2(appendTitle(dir.path(), *record, &error), qPrintable(error));
    QVERIFY(!appendTitle(dir.path(), *record, &error));
    QVERIFY(error.contains(QStringLiteral("already in your library")));
    TitleStore::Error readError = TitleStore::Error::None;
    QCOMPARE(TitleStore(dir.path()).readTitles(&readError).size(), 1);
  }

  void aRefusedTitlesDocumentIsNeverRewritten() {
    QTemporaryDir dir;
    const QString path = TitleStore(dir.path()).titlesPath();
    ProtonFixture::writeFile(path, "{\"version\": 99}");
    QString why;
    const auto record = makeTitleRecord(wowFacts(dir.filePath(QStringLiteral("p"))),
                                        catalog().at(1), {}, {}, QStringLiteral("2026-09-25"),
                                        &why);
    QVERIFY(record.has_value());
    QString error;
    QVERIFY(!appendTitle(dir.path(), *record, &error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{\"version\": 99}"));
  }

  void preferencesRoundTripAndRefuseWhole() {
    QTemporaryDir dir;
    const PreferencesStore store(dir.path());
    PreferencesStore::Error error = PreferencesStore::Error::None;
    QCOMPARE(store.read(&error), Preferences{});
    QCOMPARE(error, PreferencesStore::Error::Absent);
    QCOMPARE(store.write({QStringLiteral("GE-Proton11-6-x86_64")}), PreferencesStore::Error::None);
    QCOMPARE(store.read(&error).defaultProtonBuild, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(store.write({QStringLiteral("GE-Proton")}), PreferencesStore::Error::Refused);
    for (const QByteArray &bad :
         {QByteArray("{\"version\":2}"), QByteArray("{\"version\":1,\"x\":1}"),
          QByteArray("{\"version\":1,\"defaultProtonBuild\":\"../evil\"}"), QByteArray("[]")}) {
      ProtonFixture::writeFile(store.path(), bad);
      QCOMPARE(store.read(&error), Preferences{});
      QCOMPARE(error, PreferencesStore::Error::Refused);
    }
  }
};

QTEST_GUILESS_MAIN(tst_title_factory)
#include "tst_title_factory.moc"
