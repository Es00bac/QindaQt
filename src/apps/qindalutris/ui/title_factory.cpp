// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_factory.h"

#include "proton_pin.h"
#include "title_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace QindaQt::QindaLutris {

namespace {

constexpr qint64 kMaxPrefixVersionBytes = 256;

QString windowsBaseName(const QString &windowsPath) {
  const qsizetype slash = windowsPath.lastIndexOf(QLatin1Char('\\'));
  return slash < 0 ? windowsPath : windowsPath.mid(slash + 1);
}

std::optional<ProtonBuild> pinnableNamed(const QVector<ProtonBuild> &builds, const QString &name) {
  std::optional<ProtonBuild> found;
  for (const ProtonBuild &build : builds) {
    if (build.name != name || !build.pinnable) {
      continue;
    }
    if (!found || (build.origin == ProtonBuild::Origin::System &&
                   found->origin != ProtonBuild::Origin::System)) {
      found = build;
    }
  }
  return found;
}

QString keyOf(const QString &assignment) {
  return assignment.left(assignment.indexOf(QLatin1Char('=')));
}

} // namespace

GameStore gameStoreForRecipe(const QString &recipeId) {
  if (recipeId == QLatin1String("battlenet")) return GameStore::BattleNet;
  if (recipeId == QLatin1String("ea")) return GameStore::Ea;
  if (recipeId == QLatin1String("ubisoft")) return GameStore::Ubisoft;
  if (recipeId == QLatin1String("egs-launcher")) return GameStore::Egs;
  if (recipeId == QLatin1String("gog-galaxy")) return GameStore::Gog;
  if (recipeId == QLatin1String("amazon-games")) return GameStore::Amazon;
  return GameStore::None;
}

std::optional<CompatAdvice> adviceForRecipe(const CompatDatabase *database,
                                            const StoreRecipe &recipe) {
  if (database == nullptr || !database->isLoaded()) {
    return std::nullopt;
  }
  GameKeys keys;
  keys.title = recipe.displayName;
  if (!recipe.launcherExecutableCandidates.isEmpty()) {
    keys.executable = windowsBaseName(recipe.launcherExecutableCandidates.first());
  }
  return database->lookup(keys);
}

std::optional<CompatAdvice> adviceForGame(const CompatDatabase *database, const QString &title,
                                          const QString &executable) {
  if (database == nullptr || !database->isLoaded()) {
    return std::nullopt;
  }
  GameKeys keys;
  keys.title = title;
  keys.executable = QFileInfo(executable).fileName();
  return database->lookup(keys);
}

std::optional<ProtonBuild> chooseBuildForNewTitle(const QVector<ProtonBuild> &builds,
                                                  const QString &preferredName,
                                                  const CompatDatabase *database,
                                                  const std::optional<CompatAdvice> &advice) {
  if (database != nullptr && database->isLoaded() && advice) {
    const QString recommended = recommendedBuildFor(*database, advice);
    if (!recommended.isEmpty() && avoidReasonFor(*advice, recommended).isEmpty()) {
      if (const auto build = pinnableNamed(builds, recommended)) {
        return build;
      }
    }
  }
  auto fallback = chooseDefaultBuild(builds, preferredName);
  // AGENT-GUARD: never pin a new title to a build the database says this
  // title must avoid; with no alternative the install is refused upstream.
  if (fallback && advice && !avoidReasonFor(*advice, fallback->name).isEmpty()) {
    for (const ProtonBuild &build : builds) {
      if (build.pinnable && build.origin != ProtonBuild::Origin::Steam &&
          avoidReasonFor(*advice, build.name).isEmpty()) {
        return build;
      }
    }
    return std::nullopt;
  }
  return fallback;
}

std::optional<ProtonBuild> buildMatchingPrefix(const QString &prefixPath,
                                               const QVector<ProtonBuild> &builds) {
  QFile file(QDir(prefixPath).filePath(QStringLiteral("version")));
  if (QFileInfo(file.fileName()).isSymLink() || !file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  const QString version =
      QString::fromUtf8(file.read(kMaxPrefixVersionBytes)).section(QLatin1Char('\n'), 0, 0).trimmed();
  if (version.isEmpty()) {
    return std::nullopt;
  }
  std::optional<ProtonBuild> found;
  for (const ProtonBuild &build : builds) {
    if (build.pinnable && build.versionText.section(QLatin1Char(' '), -1) == version.section(QLatin1Char(' '), -1)) {
      if (!found || (build.origin == ProtonBuild::Origin::System &&
                     found->origin != ProtonBuild::Origin::System)) {
        found = build;
      }
    }
  }
  return found;
}

std::optional<TitleRecord> makeTitleRecord(const NewTitle &facts, const ProtonBuild &build,
                                           const std::optional<CompatAdvice> &advice,
                                           const QStringList &takenIds, const QString &today,
                                           QString *why) {
  TitleRecord record;
  record.id = makeTitleId(facts.title, takenIds);
  record.title = facts.title;
  record.kind = facts.kind;
  record.store = facts.store;
  record.storeGameId = facts.storeGameId;
  record.prefixPath = QDir::cleanPath(facts.prefixPath);
  record.protonBuild = build.name;
  record.protonBuildVersion = build.versionText;
  record.umuId = facts.umuId;
  record.umuStore = facts.umuStore;
  record.executable = QDir::cleanPath(facts.executable);
  record.installedAt = today;
  if (advice) {
    if (record.umuId.isEmpty() || record.umuId == QLatin1String("umu-0")) {
      record.umuId = advice->game.keys.umuId;
    }
    if (record.umuStore.isEmpty() || record.umuStore == QLatin1String("none")) {
      record.umuStore = advice->game.umuStore;
    }
    record.arguments = advice->game.arguments;
    for (const QString &line : advice->game.environment) {
      if (!isReservedUmuEnvironmentKey(keyOf(line))) {
        record.environment.append(line);
      }
    }
  }
  QString reason;
  if (!validateTitleRecord(record, &reason)) {
    if (why != nullptr) {
      *why = reason;
    }
    return std::nullopt;
  }
  return record;
}

bool appendTitle(const QString &configRoot, const TitleRecord &record, QString *error) {
  const auto fail = [error](const QString &message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  const TitleStore store(configRoot);
  TitleStore::Error readError = TitleStore::Error::None;
  QVector<TitleRecord> titles = store.readTitles(&readError);
  if (readError == TitleStore::Error::Refused) {
    return fail(QStringLiteral("Your list of installed games could not be read, so it was "
                               "not changed."));
  }
  for (const TitleRecord &existing : titles) {
    if (existing.id == record.id || existing.executable == record.executable) {
      return fail(QStringLiteral("%1 is already in your library.").arg(record.title));
    }
  }
  titles.append(record);
  if (store.writeTitles(titles) != TitleStore::Error::None) {
    return fail(QStringLiteral("Your list of installed games could not be saved."));
  }
  return true;
}

} // namespace QindaQt::QindaLutris
