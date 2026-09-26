// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_advice_view.h"

#include <QFileInfo>
#include <QVariantList>

namespace QindaQt::QindaLutris {

namespace {

QString antiCheatText(AntiCheatStatus status) {
  switch (status) {
  case AntiCheatStatus::Supported:
    return QStringLiteral("Anti-cheat supports Linux");
  case AntiCheatStatus::Running:
    return QStringLiteral("Anti-cheat works on Linux");
  case AntiCheatStatus::Planned:
    return QStringLiteral("Linux support for its anti-cheat is planned");
  case AntiCheatStatus::Broken:
    return QStringLiteral("Its anti-cheat does not work on Linux");
  case AntiCheatStatus::Denied:
    return QStringLiteral("The developer blocks Linux in its anti-cheat");
  case AntiCheatStatus::None:
    return QStringLiteral("No anti-cheat");
  case AntiCheatStatus::Unknown:
    break;
  }
  return {};
}

QString protonDbText(ProtonDbTier tier) {
  switch (tier) {
  case ProtonDbTier::Platinum: return QStringLiteral("Platinum");
  case ProtonDbTier::Gold: return QStringLiteral("Gold");
  case ProtonDbTier::Silver: return QStringLiteral("Silver");
  case ProtonDbTier::Bronze: return QStringLiteral("Bronze");
  case ProtonDbTier::Borked: return QStringLiteral("Borked");
  case ProtonDbTier::Native: return QStringLiteral("Native");
  case ProtonDbTier::Pending:
  case ProtonDbTier::Unknown: break;
  }
  return {};
}

QString deckText(SteamDeckCategory category) {
  switch (category) {
  case SteamDeckCategory::Verified: return QStringLiteral("Steam Deck Verified");
  case SteamDeckCategory::Playable: return QStringLiteral("Steam Deck Playable");
  case SteamDeckCategory::Unsupported: return QStringLiteral("Unsupported on Steam Deck");
  case SteamDeckCategory::Unknown: break;
  }
  return {};
}

} // namespace

QVariantMap adviceToVariant(const std::optional<CompatAdvice> &advice) {
  QVariantMap out;
  out.insert(QStringLiteral("found"), advice.has_value());
  if (!advice) {
    out.insert(QStringLiteral("verdict"), QStringLiteral("unknown"));
    out.insert(QStringLiteral("verdictText"), QStringLiteral("Not known yet"));
    out.insert(QStringLiteral("canRun"), true);
    return out;
  }
  const CompatGame &game = advice->game;
  const bool blocked = game.antiCheat == AntiCheatStatus::Broken ||
                       game.antiCheat == AntiCheatStatus::Denied;
  QStringList fixes;
  for (const QString &verb : game.winetricks) {
    fixes.append(QStringLiteral("Installs the Windows component “%1”").arg(verb));
  }
  for (const QString &line : game.environment) {
    fixes.append(QStringLiteral("Sets %1").arg(line.section(QLatin1Char('='), 0, 0)));
  }
  if (!game.arguments.isEmpty()) {
    fixes.append(QStringLiteral("Starts it with %1").arg(game.arguments.join(QLatin1Char(' '))));
  }
  QVariantList avoid;
  for (const CompatAvoid &entry : game.avoid) {
    avoid.append(QVariantMap{{QStringLiteral("build"), entry.build},
                             {QStringLiteral("reason"), entry.reason}});
  }
  const bool goodRating = game.protondbTier == ProtonDbTier::Platinum ||
                          game.protondbTier == ProtonDbTier::Gold ||
                          game.protondbTier == ProtonDbTier::Native ||
                          game.steamDeck == SteamDeckCategory::Verified ||
                          game.steamDeck == SteamDeckCategory::Playable;
  QString verdict = QStringLiteral("unknown");
  QString verdictText = QStringLiteral("Not known yet");
  if (blocked) {
    verdict = QStringLiteral("blocked");
    verdictText = QStringLiteral("Can't run on Linux");
  } else if (!fixes.isEmpty() || !avoid.isEmpty()) {
    verdict = QStringLiteral("fixes");
    verdictText = QStringLiteral("Works with fixes QindaLutris applies");
  } else if (goodRating || !game.recommendedBuild.isEmpty()) {
    verdict = QStringLiteral("works");
    verdictText = QStringLiteral("Works well");
  }
  out.insert(QStringLiteral("verdict"), verdict);
  out.insert(QStringLiteral("verdictText"), verdictText);
  out.insert(QStringLiteral("canRun"), !blocked);
  out.insert(QStringLiteral("antiCheat"), antiCheatText(game.antiCheat));
  out.insert(QStringLiteral("antiCheatNotes"), game.antiCheatNotes);
  const QString tier = protonDbText(game.protondbTier);
  out.insert(QStringLiteral("protondb"), tier.isEmpty() ? QString()
                                         : QStringLiteral("%1 (community rating)").arg(tier));
  out.insert(QStringLiteral("steamDeck"), deckText(game.steamDeck));
  out.insert(QStringLiteral("steamDeckNotes"), game.steamDeckNotes);
  out.insert(QStringLiteral("fixes"), fixes);
  out.insert(QStringLiteral("avoid"), avoid);
  out.insert(QStringLiteral("notes"), game.notes);
  out.insert(QStringLiteral("links"), game.links);
  out.insert(QStringLiteral("sources"),
             tier.isEmpty()
                 ? QStringLiteral("QindaQt compatibility database")
                 : QStringLiteral("QindaQt compatibility database; community rating from "
                                  "ProtonDB (ODbL)"));
  return out;
}

GameKeys keysForLibraryGame(const QVariantMap &game) {
  GameKeys keys;
  keys.title = game.value(QStringLiteral("title")).toString();
  const QString id = game.value(QStringLiteral("id")).toString();
  if (id.startsWith(QLatin1String("steam/"))) {
    keys.steamAppId = id.mid(6);
  }
  const QString path = game.value(QStringLiteral("installPath")).toString();
  if (path.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
    keys.executable = QFileInfo(path).fileName();
  }
  return keys;
}

} // namespace QindaQt::QindaLutris
