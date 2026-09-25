// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "game_library.h"

using namespace QindaQt::QindaLutris;

namespace {

Game makeGame(const QString &id, const QString &title, GameSource source) {
  Game game;
  game.id = id;
  game.title = title;
  game.source = source;
  return game;
}

} // namespace

class tst_game_library : public QObject {
  Q_OBJECT
private Q_SLOTS:
  // A Lutris row naming a Steam game's title folds into the Steam record.
  void lutrisDuplicateOfSteamIsDropped() {
    const GameLibrary out = mergeGameSources(
        {makeGame(QStringLiteral("steam/10"), QStringLiteral("Cyber Quest"),
                  GameSource::Steam)},
        {makeGame(QStringLiteral("lutris/3"), QStringLiteral("Cyber Quest"),
                  GameSource::Lutris),
         makeGame(QStringLiteral("lutris/4"), QStringLiteral("Other Game"),
                  GameSource::Lutris)},
        {}, {}, {}, {});
    QCOMPARE(out.games.size(), 2);
    QCOMPARE(out.games.at(0).id, QStringLiteral("steam/10"));
    QCOMPARE(out.games.at(1).id, QStringLiteral("lutris/4"));
  }

  // The match is case/punctuation-insensitive but only ever drops the Lutris
  // copy; two distinct Steam appids with the same title both survive.
  void normalizationRules() {
    QCOMPARE(normalizedTitleForMatch(QStringLiteral("Half-Life 2: Update")),
             normalizedTitleForMatch(QStringLiteral("half life 2 update")));
    const GameLibrary out = mergeGameSources(
        {makeGame(QStringLiteral("steam/1"), QStringLiteral("Twin"),
                  GameSource::Steam),
         makeGame(QStringLiteral("steam/2"), QStringLiteral("Twin"),
                  GameSource::Steam)},
        {makeGame(QStringLiteral("lutris/1"), QStringLiteral("TWIN!!"),
                  GameSource::Lutris)},
        {}, {}, {}, {});
    QCOMPARE(out.games.size(), 2);
  }

  // No source at all is an honest empty library, never an error.
  void emptyEverything() {
    const GameLibrary out = mergeGameSources({}, {}, {}, {}, {}, {});
    QVERIFY(out.games.isEmpty());
    QVERIFY(out.warnings.isEmpty());
  }

  // Installed titles are merged first, so the kMaxGames cap never drops a
  // game QindaLutris itself installed.
  void installedTitlesSurviveTheCap() {
    QVector<Game> steam;
    for (int i = 0; i < kMaxGames; ++i) {
      steam.append(makeGame(QStringLiteral("steam/%1").arg(i),
                            QStringLiteral("Game %1").arg(i), GameSource::Steam));
    }
    const GameLibrary out = mergeGameSources(
        steam, {}, {}, {},
        {makeGame(QStringLiteral("title/wow"), QStringLiteral("World of Warcraft"),
                  GameSource::Installed)},
        {});
    QCOMPARE(out.games.size(), kMaxGames);
    bool found = false;
    for (const Game &game : out.games) {
      found = found || game.id == QLatin1String("title/wow");
    }
    QVERIFY(found);
  }

  // The ADR-0275 source id round-trips like the others.
  void installedSourceId() {
    QCOMPARE(gameSourceId(GameSource::Installed), QStringLiteral("installed"));
    QCOMPARE(gameSourceForId(QStringLiteral("installed")),
             std::optional<GameSource>(GameSource::Installed));
    QVERIFY(!gameSourceForId(QStringLiteral("Installed")).has_value());
  }

  // Output is title-sorted and deterministic.
  void sorted() {
    const GameLibrary out = mergeGameSources(
        {makeGame(QStringLiteral("steam/2"), QStringLiteral("Zeta"),
                  GameSource::Steam)},
        {},
        {makeGame(QStringLiteral("desktop/a"), QStringLiteral("Alpha"),
                  GameSource::Desktop)},
        {makeGame(QStringLiteral("wine/m"), QStringLiteral("Middle"),
                  GameSource::Wine)},
        {makeGame(QStringLiteral("title/beta"), QStringLiteral("Beta"),
                  GameSource::Installed)},
        {});
    QCOMPARE(out.games.size(), 4);
    QCOMPARE(out.games.at(0).title, QStringLiteral("Alpha"));
    QCOMPARE(out.games.at(1).title, QStringLiteral("Beta"));
    QCOMPARE(out.games.at(1).source, GameSource::Installed);
    QCOMPARE(out.games.at(2).title, QStringLiteral("Middle"));
    QCOMPARE(out.games.at(3).title, QStringLiteral("Zeta"));
  }
};

QTEST_GUILESS_MAIN(tst_game_library)
#include "tst_game_library.moc"
