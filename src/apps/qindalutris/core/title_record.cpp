// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_record.h"

#include "game.h"
#include "library_store.h"
#include "proton_catalog.h"
#include "store_io.h"

#include <QDate>
#include <QDir>
#include <QRegularExpression>

namespace QindaQt::QindaLutris {
namespace {

bool fail(QString *why, const QString &text) {
  if (why != nullptr) {
    *why = text;
  }
  return false;
}

bool isToken(const QString &text, int maxChars) {
  static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._-]+$"));
  return !text.isEmpty() && text.size() <= maxChars && pattern.match(text).hasMatch();
}

bool isAbsoluteCleanPath(const QString &path) {
  return !path.isEmpty() && QDir::isAbsolutePath(path)
         && StoreIo::isCleanLine(path, kMaxTitlePathChars);
}

} // namespace

QString titleKindId(TitleKind kind) {
  switch (kind) {
  case TitleKind::StoreLauncher: return QStringLiteral("storeLauncher");
  case TitleKind::StoreGame: return QStringLiteral("storeGame");
  case TitleKind::SetupInstalled: return QStringLiteral("setupInstalled");
  }
  Q_UNREACHABLE();
}

std::optional<TitleKind> titleKindForId(const QString &id) {
  for (const TitleKind kind : {TitleKind::StoreLauncher, TitleKind::StoreGame,
                               TitleKind::SetupInstalled}) {
    if (titleKindId(kind) == id) {
      return kind;
    }
  }
  return std::nullopt;
}

QString gameStoreId(GameStore store) {
  switch (store) {
  case GameStore::None: return QStringLiteral("none");
  case GameStore::Steam: return QStringLiteral("steam");
  case GameStore::BattleNet: return QStringLiteral("battlenet");
  case GameStore::Ea: return QStringLiteral("ea");
  case GameStore::Ubisoft: return QStringLiteral("ubisoft");
  case GameStore::Egs: return QStringLiteral("egs");
  case GameStore::Gog: return QStringLiteral("gog");
  case GameStore::Amazon: return QStringLiteral("amazon");
  }
  Q_UNREACHABLE();
}

std::optional<GameStore> gameStoreForId(const QString &id) {
  for (const GameStore store :
       {GameStore::None, GameStore::Steam, GameStore::BattleNet, GameStore::Ea,
        GameStore::Ubisoft, GameStore::Egs, GameStore::Gog, GameStore::Amazon}) {
    if (gameStoreId(store) == id) {
      return store;
    }
  }
  return std::nullopt;
}

bool isReservedUmuEnvironmentKey(const QString &key) {
  for (const QLatin1String reserved :
       {QLatin1String("WINEPREFIX"), QLatin1String("PROTONPATH"),
        QLatin1String("GAMEID"), QLatin1String("STORE"),
        QLatin1String("UMU_RUNTIME_UPDATE")}) {
    if (key == reserved) {
      return true;
    }
  }
  return false;
}

bool isValidTitleId(const QString &id) {
  static const QRegularExpression pattern(
      QStringLiteral("^title/[a-z0-9][a-z0-9-]{0,%1}$").arg(kMaxTitleSlugChars - 1));
  return pattern.match(id).hasMatch();
}

QString makeTitleId(const QString &title, const QStringList &takenIds) {
  QString slug = normalizedTitleForMatch(title);
  slug.replace(QLatin1Char(' '), QLatin1Char('-'));
  // normalizedTitleForMatch keeps any Unicode letter; an id is ASCII.
  QString ascii;
  for (const QChar ch : slug) {
    const char16_t u = ch.unicode();
    if ((u >= u'a' && u <= u'z') || (u >= u'0' && u <= u'9') || u == u'-') {
      ascii += ch;
    }
  }
  while (ascii.startsWith(QLatin1Char('-'))) {
    ascii.remove(0, 1);
  }
  ascii = ascii.left(kMaxTitleSlugChars - 8);
  if (ascii.isEmpty()) {
    ascii = QStringLiteral("game");
  }
  QString candidate = QStringLiteral("title/") + ascii;
  for (int n = 2; takenIds.contains(candidate) && n <= kMaxTitles + 1; ++n) {
    candidate = QStringLiteral("title/%1-%2").arg(ascii).arg(n);
  }
  return candidate;
}

bool validateTitleRecord(const TitleRecord &record, QString *why) {
  if (why != nullptr) {
    why->clear();
  }
  if (!isValidTitleId(record.id)) {
    return fail(why, QStringLiteral("id is not title/<slug>"));
  }
  if (record.title.trimmed().isEmpty()
      || !StoreIo::isCleanLine(record.title, kMaxGameTitleChars)) {
    return fail(why, QStringLiteral("title is empty or unprintable"));
  }
  if (!StoreIo::isCleanLine(record.storeGameId, 256)) {
    return fail(why, QStringLiteral("storeGameId is unprintable"));
  }
  if (!isAbsoluteCleanPath(record.prefixPath)) {
    return fail(why, QStringLiteral("prefixPath is not an absolute path"));
  }
  if (!isValidProtonBuildName(record.protonBuild)) {
    return fail(why, QStringLiteral(
        "protonBuild must name one concrete build, never empty or an alias"));
  }
  if (!record.umuId.isEmpty() && !isToken(record.umuId, 64)) {
    return fail(why, QStringLiteral("umuId is not a plain token"));
  }
  if (!record.umuStore.isEmpty() && !isToken(record.umuStore, 32)) {
    return fail(why, QStringLiteral("umuStore is not a plain token"));
  }
  if (!isAbsoluteCleanPath(record.executable)) {
    return fail(why, QStringLiteral("executable is not an absolute path"));
  }
  if (record.arguments.size() > kMaxTitleArguments) {
    return fail(why, QStringLiteral("too many arguments"));
  }
  for (const QString &argument : record.arguments) {
    if (!StoreIo::isCleanLine(argument, kMaxTitlePathChars)) {
      return fail(why, QStringLiteral("an argument is unprintable"));
    }
  }
  if (record.environment.size() > kMaxExtraEnvironmentEntries) {
    return fail(why, QStringLiteral("too many environment entries"));
  }
  for (const QString &line : record.environment) {
    if (!isValidEnvironmentAssignment(line)) {
      return fail(why, QStringLiteral("an environment entry is not KEY=VALUE"));
    }
    if (isReservedUmuEnvironmentKey(line.left(line.indexOf(QLatin1Char('='))))) {
      return fail(why, QStringLiteral("an environment entry sets a reserved key"));
    }
  }
  if (!record.launcherTitleId.isEmpty()
      && (!isValidTitleId(record.launcherTitleId)
          || record.launcherTitleId == record.id)) {
    return fail(why, QStringLiteral("launcherTitleId is not another title id"));
  }
  if (record.winetricksApplied.size() > kMaxWinetricksVerbs) {
    return fail(why, QStringLiteral("too many winetricks verbs"));
  }
  static const QRegularExpression verb(QStringLiteral("^[a-z0-9_.=-]{1,64}$"));
  for (const QString &applied : record.winetricksApplied) {
    if (!verb.match(applied).hasMatch()) {
      return fail(why, QStringLiteral("a winetricks verb is malformed"));
    }
  }
  static const QRegularExpression isoDate(
      QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
  if (!isoDate.match(record.installedAt).hasMatch()
      || !QDate::fromString(record.installedAt, Qt::ISODate).isValid()) {
    return fail(why, QStringLiteral("installedAt is not a YYYY-MM-DD date"));
  }
  return true;
}

} // namespace QindaQt::QindaLutris
