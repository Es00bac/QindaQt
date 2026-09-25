// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_db.h"

#include "compat_db_rules.h"
#include "compat_json_scan.h"
#include "game.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

// AGENT-CONTRACT: every rule in this file has a twin in
// tools/qindalutris-compat/qlcompat/schema.py and schema_rules.py (the
// generator's validator); the byte-level rules live in compat_json_scan.cpp.
// Keep the two in step, and keep docs/wiki/apps/qindalutris-compat-db.md
// describing both. Parsing is all-or-nothing: each helper returns false on
// the first out-of-set value and the caller discards the whole document.

namespace QindaQt::QindaLutris {
namespace {

using namespace CompatRules;

bool exactKeys(const QJsonObject &object, std::initializer_list<const char *> required,
               std::initializer_list<const char *> optional = {}) {
  QSet<QString> allowed;
  for (const char *key : required) {
    if (!object.contains(QLatin1String(key))) return false;
    allowed.insert(QLatin1String(key));
  }
  for (const char *key : optional) allowed.insert(QLatin1String(key));
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    if (!allowed.contains(it.key())) return false;
  }
  return true;
}

// A bounded list of strings, each accepted by `accept`. Duplicates refuse:
// a list with repeats is a generator defect, not data.
template <typename Accept>
bool stringList(const QJsonValue &value, int maxEntries, Accept accept,
                QStringList *out) {
  if (!value.isArray()) return false;
  const QJsonArray array = value.toArray();
  if (array.size() > maxEntries) return false;
  QSet<QString> seen;
  for (const QJsonValue &entry : array) {
    if (!entry.isString()) return false;
    const QString text = entry.toString();
    if (!accept(text) || seen.contains(text)) return false;
    seen.insert(text);
    out->append(text);
  }
  return true;
}

bool optionalList(const QJsonObject &object, const char *key, int maxEntries,
                  const auto &accept, QStringList *out) {
  const QJsonValue value = object.value(QLatin1String(key));
  return value.isUndefined() || stringList(value, maxEntries, accept, out);
}

bool stringField(const QJsonValue &value, const auto &accept, QString *out) {
  if (!value.isString() || !accept(value.toString())) return false;
  *out = value.toString();
  return true;
}

const auto kIsTitle = [](const QString &t) { return isText(t, kMaxGameTitleChars, false); };
const auto kIsNote = [](const QString &t) { return isText(t, kMaxCompatTextChars, false); };

using Builds = QHash<QString, CompatBuildInfo>;

// A pin -- the database default or a game's recommendation -- may only name
// a build the same document lists as tested.
bool isTestedBuild(const Builds &builds, const QString &name) {
  const auto it = builds.constFind(name);
  return it != builds.constEnd() && it->status == CompatBuildStatus::Tested;
}

// A stamp more than 24 h ahead of the injected clock is a wrong clock or a
// forgery; it must not let a copy win chooseNewer (qlcompat FUTURE_SLACK).
bool notFuture(const QDateTime &stamp, const QDateTime &now) {
  return stamp.isValid() && stamp <= now.addSecs(kCompatFutureSlackSeconds);
}

bool parseKeys(const QJsonValue &value, CompatGameKeys *keys) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  if (!exactKeys(object, {}, {"umuId", "steamAppIds", "storeIds", "exeNames", "titles"})) {
    return false;
  }
  const QJsonValue umu = object.value(QStringLiteral("umuId"));
  if (!umu.isUndefined() && !stringField(umu, isUmuId, &keys->umuId)) return false;
  if (!optionalList(object, "steamAppIds", kMaxCompatListEntries, isSteamAppId,
                    &keys->steamAppIds)
      || !optionalList(object, "exeNames", kMaxCompatListEntries, isExeName,
                       &keys->exeNames)
      || !optionalList(object, "titles", kMaxCompatListEntries, kIsTitle,
                       &keys->titles)) {
    return false;
  }
  const QJsonValue stores = object.value(QStringLiteral("storeIds"));
  if (stores.isUndefined()) return true;
  if (!stores.isObject()) return false;
  const QJsonObject storeObject = stores.toObject();
  for (auto it = storeObject.constBegin(); it != storeObject.constEnd(); ++it) {
    const std::optional<CompatStore> store = compatStoreForId(it.key());
    QStringList ids;
    if (!store.has_value()
        || !stringList(it.value(), kMaxCompatListEntries, isStoreId, &ids)
        || ids.isEmpty()) {
      return false;
    }
    keys->storeIds.insert(*store, ids);
  }
  return true;
}

bool parseProton(const QJsonValue &value, const Builds &known, CompatGame *game) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  if (!exactKeys(object, {}, {"recommended", "avoid"})) return false;
  const QJsonValue recommended = object.value(QStringLiteral("recommended"));
  if (!recommended.isUndefined()
      && (!stringField(recommended, isValidCompatBuildName, &game->recommendedBuild)
          || !isTestedBuild(known, game->recommendedBuild))) {
    return false;
  }
  const QJsonValue avoid = object.value(QStringLiteral("avoid"));
  if (avoid.isUndefined()) return true;
  if (!avoid.isArray() || avoid.toArray().size() > kMaxCompatListEntries) return false;
  QSet<QString> builds;
  for (const QJsonValue &entry : avoid.toArray()) {
    if (!entry.isObject()) return false;
    const QJsonObject item = entry.toObject();
    CompatAvoid out;
    if (!exactKeys(item, {"build", "reason", "source"})
        || !stringField(item.value(QStringLiteral("build")), isValidCompatBuildName, &out.build)
        || !stringField(item.value(QStringLiteral("reason")), kIsNote, &out.reason)
        || !stringField(item.value(QStringLiteral("source")), kIsNote, &out.source)
        || builds.contains(out.build)) {
      return false;
    }
    builds.insert(out.build);
    game->avoid.append(out);
  }
  return true;
}

bool parseAntiCheat(const QJsonValue &value, CompatGame *game) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  if (!exactKeys(object, {"status"}, {"notes"})) return false;
  const QJsonValue status = object.value(QStringLiteral("status"));
  const std::optional<AntiCheatStatus> parsed =
      status.isString() ? antiCheatStatusForId(status.toString()) : std::nullopt;
  if (!parsed.has_value()) return false;
  game->antiCheat = *parsed;
  const QJsonValue notes = object.value(QStringLiteral("notes"));
  return notes.isUndefined() || stringField(notes, kIsNote, &game->antiCheatNotes);
}

bool parseSteamDeck(const QJsonValue &value, CompatGame *game) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  if (!exactKeys(object, {"category"}, {"notes"})) return false;
  const QJsonValue category = object.value(QStringLiteral("category"));
  const std::optional<SteamDeckCategory> parsed =
      category.isString() ? steamDeckCategoryForId(category.toString()) : std::nullopt;
  if (!parsed.has_value()) return false;
  game->steamDeck = *parsed;
  return optionalList(object, "notes", kMaxCompatListEntries, kIsNote,
                      &game->steamDeckNotes);
}

bool uniqueEnvironmentKeys(const QStringList &environment) {
  QSet<QString> keys;
  for (const QString &line : environment) {
    const QString key = compatEnvironmentKey(line);
    if (keys.contains(key)) return false;
    keys.insert(key);
  }
  return true;
}

bool parseGame(const QJsonValue &value, const Builds &known, CompatGame *game) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  if (!exactKeys(object, {"id", "title", "keys"},
                 {"proton", "environment", "winetricks", "arguments", "antiCheat",
                  "protondbTier", "steamDeck", "umuStore", "notes", "links"})
      || !stringField(object.value(QStringLiteral("id")), isGameId, &game->id)
      || !stringField(object.value(QStringLiteral("title")), kIsTitle, &game->title)
      || !parseKeys(object.value(QStringLiteral("keys")), &game->keys)) {
    return false;
  }
  const QJsonValue proton = object.value(QStringLiteral("proton"));
  if (!proton.isUndefined() && !parseProton(proton, known, game)) return false;
  const QJsonValue antiCheat = object.value(QStringLiteral("antiCheat"));
  if (!antiCheat.isUndefined() && !parseAntiCheat(antiCheat, game)) return false;
  const QJsonValue tier = object.value(QStringLiteral("protondbTier"));
  if (!tier.isUndefined()) {
    const std::optional<ProtonDbTier> parsed =
        tier.isString() ? protonDbTierForId(tier.toString()) : std::nullopt;
    if (!parsed.has_value()) return false;
    game->protondbTier = *parsed;
  }
  const QJsonValue deck = object.value(QStringLiteral("steamDeck"));
  if (!deck.isUndefined() && !parseSteamDeck(deck, game)) return false;
  const QJsonValue store = object.value(QStringLiteral("umuStore"));
  if (!store.isUndefined() && !stringField(store, isUmuStore, &game->umuStore)) {
    return false;
  }
  const auto isArgument = [](const QString &t) {
    return isText(t, kMaxCompatArgumentChars, false);
  };
  return optionalList(object, "environment", kMaxExtraEnvironmentEntries,
                      isCompatEnvironmentAssignment, &game->environment)
      && uniqueEnvironmentKeys(game->environment)
      && optionalList(object, "winetricks", kMaxCompatListEntries,
                      isWinetricksVerb, &game->winetricks)
      && optionalList(object, "arguments", kMaxCompatListEntries, isArgument,
                      &game->arguments)
      && optionalList(object, "notes", kMaxCompatListEntries, kIsNote, &game->notes)
      && optionalList(object, "links", kMaxCompatListEntries, isHttpsUrl,
                      &game->links);
}

bool parseSources(const QJsonValue &value, const QDateTime &now,
                  QVector<CompatSource> *sources) {
  if (!value.isArray() || value.toArray().size() > kMaxCompatSources) return false;
  QSet<QString> ids;
  for (const QJsonValue &entry : value.toArray()) {
    if (!entry.isObject()) return false;
    const QJsonObject object = entry.toObject();
    CompatSource source;
    QString retrieved;
    if (!exactKeys(object, {"id", "url", "retrieved"})
        || !stringField(object.value(QStringLiteral("id")), isSourceId, &source.id)
        || !stringField(object.value(QStringLiteral("url")), isHttpsUrl, &source.url)
        || !stringField(object.value(QStringLiteral("retrieved")),
                        [](const QString &t) { return parseTimestamp(t).isValid(); },
                        &retrieved)
        || ids.contains(source.id)) {
      return false;
    }
    source.retrieved = parseTimestamp(retrieved);
    if (!notFuture(source.retrieved, now)) return false;
    ids.insert(source.id);
    sources->append(source);
  }
  return true;
}

bool parseBuilds(const QJsonValue &value, QHash<QString, CompatBuildInfo> *builds) {
  if (!value.isObject() || value.toObject().size() > kMaxCompatBuilds) return false;
  const QJsonObject object = value.toObject();
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    if (!isValidCompatBuildName(it.key()) || !it.value().isObject()) return false;
    const QJsonObject item = it.value().toObject();
    const QJsonValue status = item.value(QStringLiteral("status"));
    const std::optional<CompatBuildStatus> parsed =
        status.isString() ? buildStatusForId(status.toString()) : std::nullopt;
    CompatBuildInfo info;
    if (!exactKeys(item, {"status", "notes"}) || !parsed.has_value()
        || !stringField(item.value(QStringLiteral("notes")),
                        [](const QString &t) { return isText(t, kMaxCompatTextChars, true); },
                        &info.notes)) {
      return false;
    }
    info.status = *parsed;
    builds->insert(it.key(), info);
  }
  return true;
}

bool parseHeader(const QJsonObject &root, const QDateTime &now, CompatDocument *document) {
  if (!exactKeys(root, {"schema", "version", "generated", "sources", "defaults",
                        "builds", "games"})
      || root.value(QStringLiteral("schema")) != QLatin1String(kCompatSchemaName)
      || !root.value(QStringLiteral("version")).isDouble()
      || root.value(QStringLiteral("version")).toDouble() != kCompatSchemaVersion) {
    return false;
  }
  const QJsonValue generated = root.value(QStringLiteral("generated"));
  document->generated = generated.isString() ? parseTimestamp(generated.toString())
                                             : QDateTime();
  if (!notFuture(document->generated, now)
      || !parseSources(root.value(QStringLiteral("sources")), now, &document->sources)
      || !parseBuilds(root.value(QStringLiteral("builds")), &document->builds)) {
    return false;
  }
  const QJsonValue defaults = root.value(QStringLiteral("defaults"));
  if (!defaults.isObject() || !exactKeys(defaults.toObject(), {"recommendedBuild"})) {
    return false;
  }
  const QJsonValue recommended = defaults.toObject().value(QStringLiteral("recommendedBuild"));
  if (!recommended.isString()) return false;
  document->recommendedBuild = recommended.toString();
  // The default pin must be a build the database itself lists as tested.
  return document->recommendedBuild.isEmpty()
      || isTestedBuild(document->builds, document->recommendedBuild);
}

} // namespace

std::optional<CompatDocument> parseCompatDocument(const QByteArray &bytes,
                                                  const QDateTime &now) {
  if (bytes.size() > kMaxCompatDbBytes || !CompatJson::isStrictJson(bytes)) {
    return std::nullopt;
  }
  QJsonParseError parseError{};
  const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
    return std::nullopt;
  }
  const QJsonObject root = json.object();
  CompatDocument document;
  if (!parseHeader(root, now, &document)) return std::nullopt;
  const QJsonValue games = root.value(QStringLiteral("games"));
  if (!games.isArray() || games.toArray().size() > kMaxCompatGames) return std::nullopt;
  const QJsonArray array = games.toArray();
  document.games.reserve(array.size());
  for (const QJsonValue &entry : array) {
    CompatGame game;
    if (!parseGame(entry, document.builds, &game)) return std::nullopt;
    document.games.append(std::move(game));
  }
  return document;
}

} // namespace QindaQt::QindaLutris
