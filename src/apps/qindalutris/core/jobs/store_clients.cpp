// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_clients.h"

#include "store_clients_detail.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace QindaQt::QindaLutris {

using namespace store_detail;

namespace {

StoreClientRun run(const QString &program, const QStringList &arguments,
                   const QString &storesRoot, int timeoutMs, bool secrets) {
  StoreClientRun out;
  out.spec.program = program;
  out.spec.arguments = arguments;
  out.spec.environment = storeClientEnvironment(storesRoot);
  // Python programs: nothing from the session may steer their interpreter.
  out.spec.unsetEnvironment = {QStringLiteral("LD_PRELOAD"), QStringLiteral("LD_AUDIT"),
                               QStringLiteral("LD_LIBRARY_PATH")};
  out.spec.unsetEnvironmentPrefixes = {QStringLiteral("PYTHON")};
  out.spec.timeoutMs = timeoutMs;
  out.carriesSecrets = secrets;
  return out;
}

} // namespace

QString storeClientId(StoreClient client) {
  switch (client) {
  case StoreClient::Epic: return QStringLiteral("egs");
  case StoreClient::Gog: return QStringLiteral("gog");
  case StoreClient::Amazon: return QStringLiteral("amazon");
  }
  return {};
}

std::optional<StoreClient> storeClientForId(const QString &id) {
  for (StoreClient client : {StoreClient::Epic, StoreClient::Gog, StoreClient::Amazon}) {
    if (storeClientId(client) == id) {
      return client;
    }
  }
  return std::nullopt;
}

QString storeClientDisplayName(StoreClient client) {
  switch (client) {
  case StoreClient::Epic: return QStringLiteral("Epic Games Store");
  case StoreClient::Gog: return QStringLiteral("GOG");
  case StoreClient::Amazon: return QStringLiteral("Amazon Games");
  }
  return {};
}

QString StoreClientBinaries::forClient(StoreClient client) const {
  switch (client) {
  case StoreClient::Epic: return legendary;
  case StoreClient::Gog: return gogdl;
  case StoreClient::Amazon: return nile;
  }
  return {};
}

QHash<QString, QString> storeClientEnvironment(const QString &storesRoot) {
  return {{QStringLiteral("LEGENDARY_CONFIG_PATH"), storesRoot + QStringLiteral("/legendary")},
          {QStringLiteral("GOGDL_CONFIG_PATH"), storesRoot},
          {QStringLiteral("NILE_CONFIG_PATH"), storesRoot}};
}

QString gogAuthConfigPath(const QString &storesRoot) {
  return storesRoot + QStringLiteral("/gog-auth.json");
}

QString nileConfigDirectory(const QString &storesRoot) {
  return storesRoot + QStringLiteral("/nile");
}

// ---- Sign-in ---------------------------------------------------------------

QUrl epicSignInUrl() {
  // legendary/api/egs.py get_auth_url(): the launcher's public client id.
  const QString redirect = QStringLiteral(
      "https://www.epicgames.com/id/api/redirect?clientId=34a02cf8f4414e29b15921876da36f9a"
      "&responseType=code");
  return QUrl(QStringLiteral("https://www.epicgames.com/id/login?redirectUrl=") +
              QString::fromLatin1(QUrl::toPercentEncoding(redirect)));
}

QUrl gogSignInUrl() {
  // gogdl/auth.py: GOG Galaxy's public client id and embed redirect.
  return QUrl(QStringLiteral(
      "https://auth.gog.com/auth?client_id=46899977096215655&redirect_uri="
      "https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"
      "&response_type=code&layout=galaxy"));
}

std::optional<AmazonSignInStart> parseAmazonSignInStart(const QByteArray &json) {
  const QJsonObject object = jsonFrom(json).object();
  AmazonSignInStart start;
  start.url = httpsUrl(object.value(QStringLiteral("url")).toString());
  start.clientId = object.value(QStringLiteral("client_id")).toString();
  start.codeVerifier = object.value(QStringLiteral("code_verifier")).toString();
  start.serial = object.value(QStringLiteral("serial")).toString();
  if (!start.url.isValid() || !start.url.host().endsWith(QLatin1String("amazon.com")) ||
      !isCode(start.clientId) || !isCode(start.codeVerifier) || !isCode(start.serial)) {
    return std::nullopt;
  }
  return start;
}

bool isSignInFinishedUrl(StoreClient client, const QUrl &url) {
  if (url.scheme() != QLatin1String("https")) {
    return false;
  }
  const QUrlQuery query(url);
  switch (client) {
  case StoreClient::Epic:
    return url.host() == QLatin1String("www.epicgames.com") &&
           url.path().startsWith(QLatin1String("/id/api/redirect"));
  case StoreClient::Gog:
    return url.host() == QLatin1String("embed.gog.com") &&
           url.path() == QLatin1String("/on_login_success") &&
           query.hasQueryItem(QStringLiteral("code"));
  case StoreClient::Amazon:
    return (url.host() == QLatin1String("www.amazon.com") ||
            url.host() == QLatin1String("amazon.com")) &&
           query.hasQueryItem(QStringLiteral("openid.oa2.authorization_code"));
  }
  return false;
}

std::optional<QString> captureSignInCode(StoreClient client, const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty() || trimmed.size() > 64 * 1024) {
    return std::nullopt;
  }
  QString code;
  if (client == StoreClient::Epic) {
    static const QRegularExpression field(
        QStringLiteral("\"authorizationCode\"\\s*:\\s*\"([^\"]+)\""));
    const QRegularExpressionMatch match = field.match(trimmed);
    code = match.hasMatch() ? match.captured(1) : trimmed;
  } else {
    const QString key = client == StoreClient::Gog
                            ? QStringLiteral("code")
                            : QStringLiteral("openid.oa2.authorization_code");
    const QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty() && url.hasQuery()) {
      code = QUrlQuery(url).queryItemValue(key, QUrl::FullyDecoded);
    } else {
      code = trimmed;
    }
  }
  if (!isCode(code)) {
    return std::nullopt;
  }
  return code;
}

// ---- Client runs -------------------------------------------------------------

StoreClientRun amazonSignInStartRun(const StoreClientBinaries &clients,
                                    const QString &storesRoot) {
  return run(clients.nile,
             {QStringLiteral("auth"), QStringLiteral("--login"),
              QStringLiteral("--non-interactive")},
             storesRoot, kStoreAuthTimeoutMs, /*secrets=*/true);
}

StoreClientRun signInCompleteRun(StoreClient client, const StoreClientBinaries &clients,
                                 const QString &storesRoot, const QString &code,
                                 const std::optional<AmazonSignInStart> &amazon) {
  switch (client) {
  case StoreClient::Epic:
    return run(clients.legendary,
               {QStringLiteral("auth"), QStringLiteral("--code"), code,
                QStringLiteral("--disable-webview")},
               storesRoot, kStoreAuthTimeoutMs, true);
  case StoreClient::Gog:
    return run(clients.gogdl,
               {QStringLiteral("--auth-config-path"), gogAuthConfigPath(storesRoot),
                QStringLiteral("auth"), QStringLiteral("--code"), code},
               storesRoot, kStoreAuthTimeoutMs, true);
  case StoreClient::Amazon:
    break;
  }
  const AmazonSignInStart start = amazon.value_or(AmazonSignInStart{});
  return run(clients.nile,
             {QStringLiteral("register"), QStringLiteral("--code"), code,
              QStringLiteral("--client-id"), start.clientId,
              QStringLiteral("--code-verifier"), start.codeVerifier,
              QStringLiteral("--serial"), start.serial},
             storesRoot, kStoreAuthTimeoutMs, true);
}

StoreClientRun accountStatusRun(StoreClient client, const StoreClientBinaries &clients,
                                const QString &storesRoot) {
  switch (client) {
  case StoreClient::Epic:
    return run(clients.legendary,
               {QStringLiteral("status"), QStringLiteral("--json"), QStringLiteral("--offline")},
               storesRoot, kStoreAuthTimeoutMs, false);
  case StoreClient::Gog:
    return run(clients.gogdl,
               {QStringLiteral("--auth-config-path"), gogAuthConfigPath(storesRoot),
                QStringLiteral("auth")},
               storesRoot, kStoreAuthTimeoutMs, true);
  case StoreClient::Amazon:
    break;
  }
  return run(clients.nile, {QStringLiteral("auth"), QStringLiteral("--status")}, storesRoot,
             kStoreAuthTimeoutMs, false);
}

std::optional<StoreClientRun> signOutRun(StoreClient client, const StoreClientBinaries &clients,
                                         const QString &storesRoot) {
  switch (client) {
  case StoreClient::Epic:
    return run(clients.legendary, {QStringLiteral("auth"), QStringLiteral("--delete")},
               storesRoot, kStoreAuthTimeoutMs, false);
  case StoreClient::Amazon:
    return run(clients.nile, {QStringLiteral("auth"), QStringLiteral("--logout")}, storesRoot,
               kStoreAuthTimeoutMs, false);
  case StoreClient::Gog:
    break;
  }
  return std::nullopt;
}

QVector<StoreClientRun> libraryRuns(StoreClient client, const StoreClientBinaries &clients,
                                    const QString &storesRoot) {
  switch (client) {
  case StoreClient::Epic: {
    StoreClientRun list = run(clients.legendary, {QStringLiteral("list"), QStringLiteral("--json")},
                              storesRoot, kStoreLibraryTimeoutMs, false);
    list.spec.maxOutputBytes = kMaxClientJsonBytes;
    return {list};
  }
  case StoreClient::Amazon: {
    StoreClientRun list = run(clients.nile,
                              {QStringLiteral("library"), QStringLiteral("list"),
                               QStringLiteral("--json")},
                              storesRoot, kStoreLibraryTimeoutMs, false);
    list.spec.maxOutputBytes = kMaxClientJsonBytes;
    return {run(clients.nile, {QStringLiteral("library"), QStringLiteral("sync")}, storesRoot,
                kStoreLibraryTimeoutMs, false),
            list};
  }
  case StoreClient::Gog:
    break;
  }
  return {};
}

std::optional<StoreClientRun> installRun(StoreClient client, const StoreClientBinaries &clients,
                                         const QString &storesRoot, const QString &gameId,
                                         const QString &baseDir) {
  if (!isClientGameId(gameId) || !QDir::isAbsolutePath(baseDir)) {
    return std::nullopt;
  }
  switch (client) {
  case StoreClient::Epic:
    return run(clients.legendary,
               {QStringLiteral("-y"), QStringLiteral("install"), gameId,
                QStringLiteral("--base-path"), baseDir, QStringLiteral("--platform"),
                QStringLiteral("Windows"), QStringLiteral("--skip-sdl")},
               storesRoot, kStoreInstallTimeoutMs, false);
  case StoreClient::Gog:
    return run(clients.gogdl,
               {QStringLiteral("--auth-config-path"), gogAuthConfigPath(storesRoot),
                QStringLiteral("download"), gameId, QStringLiteral("--platform"),
                QStringLiteral("windows"), QStringLiteral("--path"), baseDir,
                QStringLiteral("--skip-dlcs")},
               storesRoot, kStoreInstallTimeoutMs, false);
  case StoreClient::Amazon:
    break;
  }
  return run(clients.nile,
             {QStringLiteral("install"), gameId, QStringLiteral("--base-path"), baseDir},
             storesRoot, kStoreInstallTimeoutMs, false);
}

std::optional<StoreClientRun> installedInfoRun(StoreClient client,
                                               const StoreClientBinaries &clients,
                                               const QString &storesRoot,
                                               const QString &gameId) {
  if (!isClientGameId(gameId)) {
    return std::nullopt;
  }
  switch (client) {
  case StoreClient::Epic: {
    StoreClientRun list = run(clients.legendary,
                              {QStringLiteral("list-installed"), QStringLiteral("--json")},
                              storesRoot, kStoreLibraryTimeoutMs, false);
    list.spec.maxOutputBytes = kMaxClientJsonBytes;
    return list;
  }
  case StoreClient::Amazon:
    return run(clients.nile, {QStringLiteral("launch"), gameId, QStringLiteral("--json")},
               storesRoot, kStoreLibraryTimeoutMs, false);
  case StoreClient::Gog:
    break;
  }
  return std::nullopt;
}

} // namespace QindaQt::QindaLutris
