// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "compat_db.h"

using namespace QindaQt::QindaLutris;

// Lookup and advice over a loaded compat-db-v1 (ADR-0275 §3): the key
// precedence umu id > store id > Steam appid > exe basename > normalized
// title, ambiguity withheld rather than guessed, and the avoid/pin answers
// the install and "move build" flows ask for.
namespace {

CompatDatabase basic() {
  CompatLoadError error = CompatLoadError::Refused;
  return loadCompatDatabase(
      QStringLiteral(QINDALUTRIS_COMPAT_FIXTURES "/valid/basic.json"), &error);
}

QString matchedId(const CompatDatabase &db, const GameKeys &keys) {
  const std::optional<CompatAdvice> advice = db.lookup(keys);
  return advice.has_value() ? advice->game.id : QString();
}

} // namespace

class tst_compat_lookup : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void precedence_data() {
    QTest::addColumn<int>("dropped"); // how many of the strongest keys are blank
    QTest::addColumn<QString>("expected");
    QTest::addColumn<int>("kind");
    QTest::newRow("umu id wins") << 0 << QStringLiteral("p-umu") << int(CompatMatch::UmuId);
    QTest::newRow("then store id") << 1 << QStringLiteral("p-store") << int(CompatMatch::StoreId);
    QTest::newRow("then steam appid") << 2 << QStringLiteral("p-steam") << int(CompatMatch::SteamAppId);
    QTest::newRow("then exe basename") << 3 << QStringLiteral("p-exe") << int(CompatMatch::ExeName);
    QTest::newRow("then title") << 4 << QStringLiteral("p-title") << int(CompatMatch::Title);
    QTest::newRow("nothing") << 5 << QString() << -1;
  }
  void precedence() {
    QFETCH(int, dropped);
    QFETCH(QString, expected);
    QFETCH(int, kind);
    const CompatDatabase db = basic();
    QVERIFY(db.isLoaded());
    // Every key names a DIFFERENT game, so the winner shows the rank.
    GameKeys keys;
    if (dropped < 1) keys.umuId = QStringLiteral("umu-precedence");
    if (dropped < 2) {
      keys.store = CompatStore::Gog;
      keys.storeId = QStringLiteral("1207658883");
    }
    if (dropped < 3) keys.steamAppId = QStringLiteral("4000");
    if (dropped < 4) keys.executable = QStringLiteral("C:\\Games\\P\\PrecedenceGame.exe");
    if (dropped < 5) keys.title = QStringLiteral("precedence   TITLE!");
    const std::optional<CompatAdvice> advice = db.lookup(keys);
    QCOMPARE(advice.has_value() ? advice->game.id : QString(), expected);
    if (advice.has_value()) QCOMPARE(int(advice->matchedBy), kind);
  }

  void unknownStrongKeysFallThrough() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.umuId = QStringLiteral("umu-not-there");
    keys.steamAppId = QStringLiteral("397540");
    QCOMPARE(matchedId(db, keys), QStringLiteral("umu-397540"));
  }

  void storeIdNeedsItsStore() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.storeId = QStringLiteral("wow"); // no store: not a store match
    QCOMPARE(matchedId(db, keys), QString());
    keys.store = CompatStore::Gog;        // wrong store
    QCOMPARE(matchedId(db, keys), QString());
    keys.store = CompatStore::Battlenet;
    QCOMPARE(matchedId(db, keys), QStringLiteral("world-of-warcraft"));
  }

  void exeMatchesBasenameCaseInsensitively() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.executable = QStringLiteral("/home/u/Games/battlenet/drive_c/World of Warcraft/_retail_/WOW.EXE");
    QCOMPARE(matchedId(db, keys), QStringLiteral("world-of-warcraft"));
    keys.executable = QStringLiteral("C:\\World of Warcraft\\_classic_\\wowclassic.exe");
    QCOMPARE(matchedId(db, keys), QStringLiteral("world-of-warcraft"));
    keys.executable = QStringLiteral("alpha.exe");
    QCOMPARE(matchedId(db, keys), QStringLiteral("exe-alpha"));
  }

  void ambiguousWeakKeysAreWithheld() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.executable = QStringLiteral("SHARED.exe"); // two games list it
    QCOMPARE(matchedId(db, keys), QString());
    keys.title = QStringLiteral("Alpha Game");      // falls through to title
    QCOMPARE(matchedId(db, keys), QStringLiteral("exe-alpha"));
    GameKeys twins;
    twins.title = QStringLiteral("twin title");     // "Twin Title" and "Twin: Title"
    QCOMPARE(matchedId(db, twins), QString());
  }

  void titleAliasesMatchNormalized() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.title = QStringLiteral("hl2 update");
    QCOMPARE(matchedId(db, keys), QStringLiteral("half-life-2-update"));
    keys.title = QStringLiteral("Half Life 2 Update");
    QCOMPARE(matchedId(db, keys), QStringLiteral("half-life-2-update"));
  }

  void emptyDatabaseGivesNoAdvice() {
    const CompatDatabase empty;
    GameKeys keys;
    keys.title = QStringLiteral("World of Warcraft");
    QVERIFY(!empty.lookup(keys).has_value());
    QVERIFY(empty.recommendedBuild().isEmpty());
    QCOMPARE(recommendedBuildFor(empty, std::nullopt), QString());
    QCOMPARE(empty.buildStatus(QStringLiteral("GE-Proton11-6-x86_64")), CompatBuildStatus::Untested);
  }

  void avoidReasonNamesOnlyTheAvoidedBuild() {
    const CompatDatabase db = basic();
    GameKeys keys;
    keys.title = QStringLiteral("World of Warcraft");
    const std::optional<CompatAdvice> wow = db.lookup(keys);
    QVERIFY(wow.has_value());
    QCOMPARE(avoidReasonFor(*wow, QStringLiteral("GE-Proton11-7-x86_64")),
             QStringLiteral("Freezes the game for about a second every two seconds"));
    QVERIFY(avoidReasonFor(*wow, QStringLiteral("GE-Proton11-6-x86_64")).isEmpty());
    QVERIFY(avoidReasonFor(*wow, QStringLiteral("ge-proton11-7-x86_64")).isEmpty());
  }

  void recommendedBuildPrefersTheGame() {
    const CompatDatabase db = basic();
    GameKeys wowKeys;
    wowKeys.title = QStringLiteral("World of Warcraft");
    QCOMPARE(recommendedBuildFor(db, db.lookup(wowKeys)), QStringLiteral("GE-Proton11-6-x86_64"));
    GameKeys bl3;
    bl3.steamAppId = QStringLiteral("397540");
    const std::optional<CompatAdvice> advice = db.lookup(bl3);
    QVERIFY(advice.has_value() && advice->game.recommendedBuild.isEmpty());
    QCOMPARE(recommendedBuildFor(db, advice), db.recommendedBuild());
    QCOMPARE(recommendedBuildFor(db, std::nullopt), db.recommendedBuild());
  }

  void buildStatusAndNotes() {
    const CompatDatabase db = basic();
    QCOMPARE(db.buildStatus(QStringLiteral("GE-Proton11-6-x86_64")), CompatBuildStatus::Tested);
    QCOMPARE(db.buildStatus(QStringLiteral("GE-Proton11-7-x86_64")), CompatBuildStatus::KnownIssues);
    QCOMPARE(db.buildStatus(QStringLiteral("Proton 9.0 (Beta)")), CompatBuildStatus::Untested);
    QCOMPARE(db.buildStatus(QStringLiteral("GE-Proton12-1")), CompatBuildStatus::Untested);
    QCOMPARE(db.buildNotes(QStringLiteral("GE-Proton11-7-x86_64")),
             QStringLiteral("WoW render-loop stalls"));
    QVERIFY(db.buildNotes(QStringLiteral("GE-Proton12-1")).isEmpty());
  }

  void buildNameRule() {
    QVERIFY(isValidCompatBuildName(QStringLiteral("GE-Proton11-6-x86_64")));
    QVERIFY(isValidCompatBuildName(QStringLiteral("Proton 9.0 (Beta)")));
    QVERIFY(!isValidCompatBuildName(QStringLiteral("../GE-Proton")));
    QVERIFY(!isValidCompatBuildName(QStringLiteral("GE/Proton")));
    QVERIFY(!isValidCompatBuildName(QStringLiteral(".hidden")));
    QVERIFY(!isValidCompatBuildName(QStringLiteral("trailing ")));
    QVERIFY(!isValidCompatBuildName(QString()));
  }

  void storeIdsRoundTrip() {
    for (const char *id : {"egs", "gog", "amazon", "ubisoft", "ea", "battlenet", "humble",
                           "itchio", "zoomplatform"}) {
      const std::optional<CompatStore> store = compatStoreForId(QLatin1String(id));
      QVERIFY(store.has_value());
      QCOMPARE(compatStoreId(*store), QLatin1String(id));
    }
    QVERIFY(!compatStoreForId(QStringLiteral("steam")).has_value());
  }
};

QTEST_GUILESS_MAIN(tst_compat_lookup)
#include "tst_compat_lookup.moc"
