// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_launch.h"

#include "umu_launch.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::QindaLutris {
namespace {

LaunchPlan refused(const QString &reason) {
  LaunchPlan plan;
  plan.reason = reason;
  return plan;
}

struct ClientFacts {
  QString binary;
  QString missingReason;
};

ClientFacts clientFor(GameStore store, const StoreClientSet &clients) {
  switch (store) {
  case GameStore::Egs:
    return {clients.legendaryBinary,
            QStringLiteral("Epic Games support is not installed. Install "
                           "games-util/legendary.")};
  case GameStore::Gog:
    return {clients.gogdlBinary,
            QStringLiteral("GOG support is not installed. Install games-util/gogdl-bin.")};
  case GameStore::Amazon:
    return {clients.nileBinary,
            QStringLiteral("Amazon Games support is not installed. Install "
                           "games-util/nile-bin.")};
  default:
    break;
  }
  return {};
}

} // namespace

bool launchesThroughStoreClient(GameStore store) {
  return store == GameStore::Egs || store == GameStore::Gog || store == GameStore::Amazon;
}

bool isSafeStoreGameId(const QString &id) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
  return pattern.match(id).hasMatch();
}

QString joinForShlex(const QStringList &tokens) {
  static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_@%+=:,./-]+$"));
  QStringList quoted;
  for (const QString &token : tokens) {
    if (plain.match(token).hasMatch()) {
      quoted.append(token);
    } else {
      QString escaped = token;
      escaped.replace(QLatin1Char('\''), QStringLiteral("'\"'\"'"));
      quoted.append(QLatin1Char('\'') + escaped + QLatin1Char('\''));
    }
  }
  return quoted.join(QLatin1Char(' '));
}

QString findGogInstallDirectory(const QString &executable, const QString &gogId) {
  if (!isSafeStoreGameId(gogId) || !QDir::isAbsolutePath(executable)) {
    return {};
  }
  const QString infoName = QStringLiteral("goggame-%1.info").arg(gogId);
  QDir dir = QFileInfo(executable).absoluteDir();
  for (int depth = 0; depth <= kMaxGogInfoDepth; ++depth) {
    if (QFileInfo(dir.filePath(infoName)).isFile()) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}

LaunchPlan planStoreGameLaunch(const TitleRecord &title, const LaunchOptions &options,
                               const LaunchToolSet &tools,
                               const QVector<DisplayTarget> &displays) {
  const ClientFacts client = clientFor(title.store, tools.storeClients);
  if (client.missingReason.isEmpty()) {
    return refused(QStringLiteral("This game's store cannot start games here."));
  }
  if (client.binary.isEmpty()) {
    return refused(client.missingReason);
  }
  if (!isSafeStoreGameId(title.storeGameId)) {
    return refused(QStringLiteral(
        "This game's store entry is damaged. Remove it and install it again."));
  }
  UmuLaunchRequest request = umuRequestForTitle(title);
  request.arguments.clear();
  // A new store game's prefix is made by umu on its first start.
  request.prefixMustExist = false;
  LaunchPlan plan = planUmuLaunch(request, options, tools, displays);
  if (!plan.ok) {
    return plan;
  }
  const QStringList argv = QStringList{plan.program} + plan.arguments;
  const qsizetype gameAt = argv.lastIndexOf(request.executable);
  if (gameAt < 1) {
    return refused(QStringLiteral("This game could not be prepared to start."));
  }
  const QString wrapper = joinForShlex(argv.mid(0, gameAt));
  const QString id = title.storeGameId;
  QStringList arguments;
  QString workingDirectory = plan.workingDirectory;
  switch (title.store) {
  case GameStore::Egs:
    arguments = {QStringLiteral("launch"), id, QStringLiteral("--no-wine"),
                 QStringLiteral("--wrapper"), wrapper,
                 QStringLiteral("--skip-version-check")};
    break;
  case GameStore::Gog: {
    const QString installDir = findGogInstallDirectory(request.executable, id);
    if (installDir.isEmpty()) {
      return refused(QStringLiteral(
          "This GOG game's files are incomplete. Install it again."));
    }
    arguments = {QStringLiteral("launch"), installDir, id,
                 QStringLiteral("--platform"), QStringLiteral("windows"),
                 QStringLiteral("--no-wine"), QStringLiteral("--wrapper"), wrapper};
    workingDirectory = installDir;
    break;
  }
  default: // Amazon (clientFor refused every other store)
    arguments = {QStringLiteral("launch"), id, QStringLiteral("--no-wine"),
                 QStringLiteral("--wrapper"), wrapper};
    break;
  }
  if (!title.arguments.isEmpty()) {
    plan.notes.append(QStringLiteral(
        "the store starts this game with its own options; saved options were not used"));
  }
  plan.program = client.binary;
  plan.arguments = arguments;
  plan.workingDirectory = workingDirectory;
  for (auto it = tools.storeClients.environment.cbegin();
       it != tools.storeClients.environment.cend(); ++it) {
    if (!isReservedUmuEnvironmentKey(it.key())) {
      plan.environment.insert(it.key(), it.value());
    }
  }
  // gogdl runs this program instead of the game when it is set.
  plan.unsetEnvironment.append(QStringLiteral("HEROIC_GOGDL_WRAPPER_EXE"));
  return plan;
}

} // namespace QindaQt::QindaLutris
