// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_clients.h"

#include "store_clients_detail.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace QindaQt::QindaLutris {

using namespace store_detail;

// ---- Parsers -----------------------------------------------------------------

StoreAccountStatus parseAccountStatus(StoreClient client, const QByteArray &out) {
  const QJsonObject object = jsonFrom(out).object();
  StoreAccountStatus status;
  switch (client) {
  case StoreClient::Epic: {
    const QString account = object.value(QStringLiteral("account")).toString();
    status.signedIn = !account.isEmpty() && account != QLatin1String("<not logged in>");
    status.userName = status.signedIn ? account.left(128) : QString();
    break;
  }
  case StoreClient::Gog: {
    const QString token = object.value(QStringLiteral("access_token")).toString();
    status.signedIn = isCode(token);
    status.accessToken = status.signedIn ? token : QString();
    break;
  }
  case StoreClient::Amazon:
    status.signedIn = object.value(QStringLiteral("LoggedIn")).toBool();
    status.userName =
        status.signedIn ? object.value(QStringLiteral("Username")).toString().left(128)
                        : QString();
    break;
  }
  return status;
}

QVector<OwnedStoreGame> parseEpicLibrary(const QByteArray &json) {
  QVector<OwnedStoreGame> games;
  for (const QJsonValue &value : jsonFrom(json).array()) {
    const QJsonObject game = value.toObject();
    const QJsonObject metadata = game.value(QStringLiteral("metadata")).toObject();
    if (metadata.value(QStringLiteral("customAttributes"))
            .toObject()
            .contains(QStringLiteral("ThirdPartyManagedApp"))) {
      continue;
    }
    OwnedStoreGame owned;
    owned.id = game.value(QStringLiteral("app_name")).toString();
    owned.title = game.value(QStringLiteral("app_title")).toString().simplified().left(256);
    if (!isClientGameId(owned.id) || owned.title.isEmpty()) {
      continue;
    }
    QUrl fallback;
    for (const QJsonValue &image : metadata.value(QStringLiteral("keyImages")).toArray()) {
      const QJsonObject key = image.toObject();
      const QUrl url = httpsUrl(key.value(QStringLiteral("url")).toString());
      const QString type = key.value(QStringLiteral("type")).toString();
      if (type == QLatin1String("DieselGameBoxTall") && url.isValid()) {
        owned.imageUrl = url;
        break;
      }
      if (fallback.isEmpty() && url.isValid()) {
        fallback = url;
      }
    }
    if (owned.imageUrl.isEmpty()) {
      owned.imageUrl = fallback;
    }
    games.append(owned);
    if (games.size() >= kMaxOwnedGames) {
      break;
    }
  }
  return games;
}

QVector<OwnedStoreGame> parseAmazonLibrary(const QByteArray &json) {
  QVector<OwnedStoreGame> games;
  for (const QJsonValue &value : jsonFrom(json).array()) {
    const QJsonObject product = value.toObject().value(QStringLiteral("product")).toObject();
    OwnedStoreGame owned;
    owned.id = product.value(QStringLiteral("id")).toString();
    owned.title = product.value(QStringLiteral("title")).toString().simplified().left(256);
    if (!isClientGameId(owned.id) || owned.title.isEmpty()) {
      continue;
    }
    const QJsonObject detail = product.value(QStringLiteral("productDetail")).toObject();
    owned.imageUrl = httpsUrl(detail.value(QStringLiteral("details"))
                                  .toObject()
                                  .value(QStringLiteral("logoUrl"))
                                  .toString());
    if (owned.imageUrl.isEmpty()) {
      owned.imageUrl = httpsUrl(detail.value(QStringLiteral("iconUrl")).toString());
    }
    games.append(owned);
    if (games.size() >= kMaxOwnedGames) {
      break;
    }
  }
  return games;
}

QUrl gogLibraryPageUrl(int page) {
  return QUrl(QStringLiteral("https://embed.gog.com/account/getFilteredProducts?mediaType=1&page=%1")
                  .arg(qBound(1, page, 1000)));
}

QVector<OwnedStoreGame> parseGogLibraryPage(const QByteArray &json, int *totalPages) {
  const QJsonObject object = jsonFrom(json).object();
  if (totalPages != nullptr) {
    *totalPages = qBound(0, object.value(QStringLiteral("totalPages")).toInt(), 1000);
  }
  QVector<OwnedStoreGame> games;
  for (const QJsonValue &value : object.value(QStringLiteral("products")).toArray()) {
    const QJsonObject product = value.toObject();
    if (!product.value(QStringLiteral("isGame")).toBool(true) ||
        !product.value(QStringLiteral("worksOn")).toObject().value(QStringLiteral("Windows"))
             .toBool()) {
      continue;
    }
    OwnedStoreGame owned;
    const QJsonValue id = product.value(QStringLiteral("id"));
    owned.id = id.isDouble() ? QString::number(id.toInteger()) : id.toString();
    owned.title = product.value(QStringLiteral("title")).toString().simplified().left(256);
    if (!isClientGameId(owned.id) || owned.title.isEmpty()) {
      continue;
    }
    const QString image = product.value(QStringLiteral("image")).toString();
    owned.imageUrl = image.isEmpty() ? QUrl() : httpsUrl(image + QStringLiteral(".jpg"));
    games.append(owned);
    if (games.size() >= kMaxOwnedGames) {
      break;
    }
  }
  return games;
}

std::optional<StoreGameInstall> parseEpicInstalled(const QByteArray &json,
                                                   const QString &appName) {
  for (const QJsonValue &value : jsonFrom(json).array()) {
    const QJsonObject game = value.toObject();
    if (game.value(QStringLiteral("app_name")).toString() != appName) {
      continue;
    }
    StoreGameInstall install;
    install.installDir =
        QDir::cleanPath(game.value(QStringLiteral("install_path")).toString());
    if (!QDir::isAbsolutePath(install.installDir)) {
      return std::nullopt;
    }
    install.executable =
        joinInside(install.installDir, game.value(QStringLiteral("executable")).toString());
    if (install.executable.isEmpty()) {
      return std::nullopt;
    }
    return install;
  }
  return std::nullopt;
}

std::optional<StoreGameInstall> parseAmazonLaunchInfo(const QByteArray &json) {
  const QJsonObject object = jsonFrom(json).object();
  StoreGameInstall install;
  install.installDir =
      QDir::cleanPath(object.value(QStringLiteral("game_directory")).toString());
  install.executable = QDir::cleanPath(object.value(QStringLiteral("command"))
                                           .toObject()
                                           .value(QStringLiteral("instruction"))
                                           .toString());
  if (!QDir::isAbsolutePath(install.installDir) || !QDir::isAbsolutePath(install.executable) ||
      !install.executable.startsWith(install.installDir + QLatin1Char('/'))) {
    return std::nullopt;
  }
  return install;
}

std::optional<StoreGameInstall> parseGogGameInfo(const QByteArray &json,
                                                 const QString &installDir) {
  const QJsonArray tasks = jsonFrom(json).object().value(QStringLiteral("playTasks")).toArray();
  QString chosen;
  for (const QJsonValue &value : tasks) {
    const QJsonObject task = value.toObject();
    if (task.value(QStringLiteral("type")).toString() != QLatin1String("FileTask")) {
      continue;
    }
    const QString path = task.value(QStringLiteral("path")).toString();
    if (task.value(QStringLiteral("isPrimary")).toBool()) {
      chosen = path;
      break;
    }
    if (chosen.isEmpty() && task.value(QStringLiteral("category")).toString() ==
                                QLatin1String("game")) {
      chosen = path;
    }
  }
  StoreGameInstall install;
  install.installDir = QDir::cleanPath(installDir);
  install.executable = joinInside(install.installDir, chosen);
  if (!QDir::isAbsolutePath(install.installDir) || install.executable.isEmpty()) {
    return std::nullopt;
  }
  return install;
}

std::optional<StoreGameInstall> readGogInstall(const QString &baseDir, const QString &gogId) {
  if (!isClientGameId(gogId) || !QDir::isAbsolutePath(baseDir)) {
    return std::nullopt;
  }
  const QString infoName = QStringLiteral("goggame-%1.info").arg(gogId);
  const QFileInfoList folders =
      QDir(baseDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  for (const QFileInfo &folder : folders.mid(0, 1000)) {
    QFile info(QDir(folder.absoluteFilePath()).filePath(infoName));
    if (!info.exists() || info.size() > 1024 * 1024 || !info.open(QIODevice::ReadOnly)) {
      continue;
    }
    std::optional<StoreGameInstall> install =
        parseGogGameInfo(info.readAll(), folder.absoluteFilePath());
    if (install && !QFileInfo(install->executable).isFile()) {
      // GOG's paths are written for Windows' case-insensitive disks.
      QString resolved = install->installDir;
      const QString rest = install->executable.mid(install->installDir.size() + 1);
      for (const QString &segment : rest.split(QLatin1Char('/'))) {
        const QStringList entries = QDir(resolved).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        const auto match = std::find_if(entries.cbegin(), entries.cend(), [&](const QString &e) {
          return e.compare(segment, Qt::CaseInsensitive) == 0;
        });
        resolved += QLatin1Char('/') + (match != entries.cend() ? *match : segment);
      }
      install->executable = resolved;
    }
    return install;
  }
  return std::nullopt;
}

std::optional<double> parseDownloadProgress(const QString &line) {
  static const QRegularExpression pattern(
      QStringLiteral("= Progress: ([0-9]{1,3}(?:\\.[0-9]+)?)"));
  const QRegularExpressionMatch match = pattern.match(line);
  if (!match.hasMatch()) {
    return std::nullopt;
  }
  return qBound(0.0, match.captured(1).toDouble() / 100.0, 1.0);
}

} // namespace QindaQt::QindaLutris
