// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_db.h"

#include "game.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#ifndef QINDALUTRIS_COMPAT_DB_SHIPPED_PATH
#define QINDALUTRIS_COMPAT_DB_SHIPPED_PATH "/usr/share/qindalutris/compat-db-v1.json"
#endif

namespace QindaQt::QindaLutris {
namespace {

QString storeKey(CompatStore store, const QString &id) {
  return compatStoreId(store) + QChar(0x1f) + id;
}

QString exeKey(const QString &executable) {
  const qsizetype slash = qMax(executable.lastIndexOf(QLatin1Char('/')),
                               executable.lastIndexOf(QLatin1Char('\\')));
  return executable.mid(slash + 1).toCaseFolded();
}

// Weak keys: several games may legitimately share one (two editions called
// "Launcher.exe", two umu records for one title). A shared key is marked -1
// and never matches -- advice is withheld rather than guessed.
void indexWeak(QHash<QString, int> *index, const QString &key, int game) {
  if (key.isEmpty()) return;
  const auto it = index->constFind(key);
  if (it == index->constEnd()) {
    index->insert(key, game);
  } else if (it.value() != game) {
    index->insert(key, -1);
  }
}

// Strong keys must be unique across the document; false = collision.
bool indexStrong(QHash<QString, int> *index, const QString &key, int game) {
  if (key.isEmpty()) return true;
  if (index->contains(key)) return false;
  index->insert(key, game);
  return true;
}

std::optional<int> hit(const QHash<QString, int> &index, const QString &key) {
  if (key.isEmpty()) return std::nullopt;
  const int game = index.value(key, -1);
  return game >= 0 ? std::optional<int>(game) : std::nullopt;
}

} // namespace

std::optional<CompatDatabase> CompatDatabase::fromDocument(CompatDocument document) {
  CompatDatabase db;
  QSet<QString> ids;
  for (int i = 0; i < document.games.size(); ++i) {
    const CompatGame &game = document.games.at(i);
    if (ids.contains(game.id)) return std::nullopt;
    ids.insert(game.id);
    if (!indexStrong(&db.m_byUmu, game.keys.umuId, i)) return std::nullopt;
    for (const QString &appId : game.keys.steamAppIds) {
      if (!indexStrong(&db.m_bySteam, appId, i)) return std::nullopt;
    }
    for (auto it = game.keys.storeIds.constBegin(); it != game.keys.storeIds.constEnd(); ++it) {
      for (const QString &id : it.value()) {
        if (!indexStrong(&db.m_byStore, storeKey(it.key(), id), i)) return std::nullopt;
      }
    }
    for (const QString &exe : game.keys.exeNames) indexWeak(&db.m_byExe, exeKey(exe), i);
    indexWeak(&db.m_byTitle, normalizedTitleForMatch(game.title), i);
    for (const QString &alias : game.keys.titles) {
      indexWeak(&db.m_byTitle, normalizedTitleForMatch(alias), i);
    }
  }
  db.m_doc = std::move(document);
  return db;
}

CompatBuildStatus CompatDatabase::buildStatus(const QString &buildName) const {
  const auto it = m_doc.builds.constFind(buildName);
  return it == m_doc.builds.constEnd() ? CompatBuildStatus::Untested : it->status;
}

QString CompatDatabase::buildNotes(const QString &buildName) const {
  return m_doc.builds.value(buildName).notes;
}

std::optional<CompatAdvice> CompatDatabase::lookup(const GameKeys &keys) const {
  const auto advice = [this](int game, CompatMatch kind) {
    return std::optional<CompatAdvice>(CompatAdvice{m_doc.games.at(game), kind});
  };
  if (const auto game = hit(m_byUmu, keys.umuId)) return advice(*game, CompatMatch::UmuId);
  if (keys.store.has_value() && !keys.storeId.isEmpty()) {
    if (const auto game = hit(m_byStore, storeKey(*keys.store, keys.storeId))) {
      return advice(*game, CompatMatch::StoreId);
    }
  }
  if (const auto game = hit(m_bySteam, keys.steamAppId)) {
    return advice(*game, CompatMatch::SteamAppId);
  }
  if (const auto game = hit(m_byExe, exeKey(keys.executable))) {
    return advice(*game, CompatMatch::ExeName);
  }
  if (const auto game = hit(m_byTitle, normalizedTitleForMatch(keys.title))) {
    return advice(*game, CompatMatch::Title);
  }
  return std::nullopt;
}

QString avoidReasonFor(const CompatAdvice &advice, const QString &buildName) {
  for (const CompatAvoid &avoid : advice.game.avoid) {
    if (avoid.build == buildName) return avoid.reason;
  }
  return {};
}

QString recommendedBuildFor(const CompatDatabase &database,
                            const std::optional<CompatAdvice> &advice) {
  if (advice.has_value() && !advice->game.recommendedBuild.isEmpty()) {
    return advice->game.recommendedBuild;
  }
  return database.recommendedBuild();
}

CompatDatabase loadCompatDatabase(const QString &path, CompatLoadError *error) {
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    *error = CompatLoadError::Absent;
    return {};
  }
  *error = CompatLoadError::Refused;
  // AGENT-GUARD: symlink, type and size are refused before a byte is read;
  // the refresh path writes this file, so it is attacker-reachable input.
  if (info.isSymLink() || !info.isFile() || info.size() > kMaxCompatDbBytes) return {};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  const QByteArray bytes = file.read(kMaxCompatDbBytes + 1);
  std::optional<CompatDocument> document = parseCompatDocument(bytes);
  if (!document.has_value()) return {};
  std::optional<CompatDatabase> database = CompatDatabase::fromDocument(std::move(*document));
  if (!database.has_value()) return {};
  *error = CompatLoadError::None;
  return *database;
}

CompatDatabase chooseNewer(const CompatDatabase &shipped, const CompatDatabase &refreshed) {
  if (!refreshed.isLoaded()) return shipped;
  if (!shipped.isLoaded()) return refreshed;
  return refreshed.generated() > shipped.generated() ? refreshed : shipped;
}

CompatDatabase loadEffectiveCompatDatabase(const QString &shippedPath,
                                           const QString &refreshedPath) {
  CompatLoadError ignored = CompatLoadError::None;
  const CompatDatabase shipped = loadCompatDatabase(shippedPath, &ignored);
  const CompatDatabase refreshed = loadCompatDatabase(refreshedPath, &ignored);
  return chooseNewer(shipped, refreshed);
}

QString shippedCompatDatabasePath() {
  // AGENT-CONTRACT: set from CMAKE_INSTALL_FULL_DATADIR in core/CMakeLists.txt,
  // the same directory src/apps/qindalutris/CMakeLists.txt installs the
  // snapshot into (component QindaLutrisCompatDb).
  return QStringLiteral(QINDALUTRIS_COMPAT_DB_SHIPPED_PATH);
}

QString refreshedCompatDatabasePath(const QString &dataHome) {
  return dataHome + QStringLiteral("/qindalutris/compat-db-v1.json");
}

QString defaultRefreshedCompatDatabasePath() {
  return refreshedCompatDatabasePath(
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
}

} // namespace QindaQt::QindaLutris
