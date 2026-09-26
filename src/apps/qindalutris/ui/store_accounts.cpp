// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_accounts.h"

#include "library_controller.h"
#include "process_runner.h"
#include "title_record.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QVariantMap>

namespace QindaQt::QindaLutris {
namespace {

bool succeeded(const ProcessRunResult &result) {
  return result.started && !result.timedOut && !result.cancelled && !result.crashed &&
         result.exitCode == 0;
}

QString gamesSentence(qsizetype count) {
  return count == 1 ? QStringLiteral("1 game in your library.")
                    : QStringLiteral("%1 games in your library.").arg(count);
}

} // namespace

QString storesRootFor(const QString &configRoot) {
  return configRoot + QStringLiteral("/stores");
}

StoreClientSet discoverStoreClients(const QString &configRoot) {
  StoreClientSet set;
  set.legendaryBinary = QStandardPaths::findExecutable(QStringLiteral("legendary"));
  set.gogdlBinary = QStandardPaths::findExecutable(QStringLiteral("gogdl"));
  set.nileBinary = QStandardPaths::findExecutable(QStringLiteral("nile"));
  set.environment = storeClientEnvironment(storesRootFor(configRoot));
  return set;
}

StoreClientBinaries storeClientBinaries(const StoreClientSet &set) {
  return {set.legendaryBinary, set.gogdlBinary, set.nileBinary};
}

StoreAccounts::StoreAccounts(LibraryController *library, bool webSignInAvailable,
                             RunnerFactory runners, QObject *parent)
    : QObject(parent), m_library(library), m_webSignIn(webSignInAvailable) {
  if (!runners) {
    runners = [](QObject *owner) {
      return new QProcessRunner(ProcessContainment::Auto, owner);
    };
  }
  const std::array<StoreClient, 3> clients{StoreClient::Epic, StoreClient::Gog,
                                           StoreClient::Amazon};
  for (std::size_t i = 0; i < clients.size(); ++i) {
    Account &account = m_accounts[i];
    account.client = clients[i];
    account.runner = runners(this);
    connect(account.runner, &ProcessRunner::finished, this,
            [this, &account](const ProcessRunResult &result) { onFinished(account, result); });
  }
  // "Installed" marks follow the library.
  connect(m_library, &LibraryController::libraryChanged, this, &StoreAccounts::accountsChanged);
}

StoreAccounts::~StoreAccounts() = default;

StoreAccounts::Account *StoreAccounts::accountFor(const QString &storeId) {
  for (Account &account : m_accounts) {
    if (storeClientId(account.client) == storeId) {
      return &account;
    }
  }
  return nullptr;
}

const StoreAccounts::Account *StoreAccounts::accountFor(const QString &storeId) const {
  return const_cast<StoreAccounts *>(this)->accountFor(storeId);
}

bool StoreAccounts::available(const Account &account) const {
  return !storeClientBinaries(m_library->toolSet().storeClients)
              .forClient(account.client)
              .isEmpty();
}

QVariantList StoreAccounts::accounts() const {
  QVariantList rows;
  for (const Account &account : m_accounts) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), storeClientId(account.client)},
        {QStringLiteral("name"), storeClientDisplayName(account.client)},
        {QStringLiteral("available"), available(account)},
        {QStringLiteral("signedIn"), account.signedIn},
        {QStringLiteral("userName"), account.userName},
        {QStringLiteral("busy"), account.busy},
        {QStringLiteral("message"), account.message},
        {QStringLiteral("gameCount"), account.games.size()},
    });
  }
  return rows;
}

QVariantList StoreAccounts::ownedGames(const QString &storeId) const {
  const Account *account = accountFor(storeId);
  const std::optional<GameStore> store = gameStoreForId(storeId);
  QVariantList rows;
  if (account == nullptr || !store) {
    return rows;
  }
  for (const OwnedStoreGame &game : account->games) {
    QString titleId;
    for (const TitleRecord &title : m_library->titles()) {
      if (title.store == *store && title.storeGameId == game.id) {
        titleId = title.id;
        break;
      }
    }
    rows.append(QVariantMap{{QStringLiteral("id"), game.id},
                            {QStringLiteral("title"), game.title},
                            {QStringLiteral("imageUrl"), game.imageUrl},
                            {QStringLiteral("installedTitleId"), titleId}});
  }
  return rows;
}

QVariantMap StoreAccounts::ownedGamesByStore() const {
  QVariantMap out;
  for (const Account &account : m_accounts) {
    const QString id = storeClientId(account.client);
    out.insert(id, ownedGames(id));
  }
  return out;
}

void StoreAccounts::refreshAll() {
  for (Account &account : m_accounts) {
    if (available(account)) {
      queueStatusAndLibrary(account);
    }
  }
  Q_EMIT accountsChanged();
}

void StoreAccounts::refresh(const QString &storeId) {
  Account *account = accountFor(storeId);
  if (account == nullptr) {
    return;
  }
  if (!available(*account)) {
    settle(*account, QStringLiteral("%1 support is not installed.")
                         .arg(storeClientDisplayName(account->client)));
    return;
  }
  queueStatusAndLibrary(*account);
}

void StoreAccounts::beginSignIn(const QString &storeId) {
  Account *account = accountFor(storeId);
  if (account == nullptr || !available(*account)) {
    return;
  }
  switch (account->client) {
  case StoreClient::Epic:
    Q_EMIT signInReady(storeId, epicSignInUrl());
    return;
  case StoreClient::Gog:
    Q_EMIT signInReady(storeId, gogSignInUrl());
    return;
  case StoreClient::Amazon:
    break;
  }
  const QString root = storesRootFor(m_library->configRoot());
  enqueue(*account,
          amazonSignInStartRun(storeClientBinaries(m_library->toolSet().storeClients), root),
          [this, account, storeId](const ProcessRunResult &result) {
            account->amazonStart = succeeded(result)
                                       ? parseAmazonSignInStart(result.standardOutput)
                                       : std::nullopt;
            if (!account->amazonStart) {
              settle(*account, QStringLiteral("Amazon sign-in could not start. Check your "
                                              "internet connection and try again."));
              return;
            }
            settle(*account, {});
            Q_EMIT signInReady(storeId, account->amazonStart->url);
          });
}

bool StoreAccounts::isSignInFinishedUrl(const QString &storeId, const QUrl &url) const {
  const std::optional<StoreClient> client = storeClientForId(storeId);
  return client && QindaLutris::isSignInFinishedUrl(*client, url);
}

void StoreAccounts::finishSignIn(const QString &storeId, const QString &text) {
  Account *account = accountFor(storeId);
  if (account == nullptr || !available(*account)) {
    return;
  }
  const std::optional<QString> code = captureSignInCode(account->client, text);
  if (!code) {
    settle(*account, QStringLiteral("That did not look like a sign-in code. Sign in "
                                    "again and paste the whole address you end up on."));
    return;
  }
  if (account->client == StoreClient::Amazon && !account->amazonStart) {
    settle(*account, QStringLiteral("Start the Amazon sign-in again."));
    return;
  }
  const QString root = storesRootFor(m_library->configRoot());
  StoreClientRun run =
      signInCompleteRun(account->client, storeClientBinaries(m_library->toolSet().storeClients),
                        root, *code, account->amazonStart);
  account->amazonStart.reset();
  enqueue(*account, run, [this, account](const ProcessRunResult &result) {
    if (!succeeded(result)) {
      settle(*account, QStringLiteral("Sign-in did not work. The code may have expired; "
                                      "sign in again."));
      return;
    }
    queueStatusAndLibrary(*account);
  });
}

void StoreAccounts::signOut(const QString &storeId) {
  Account *account = accountFor(storeId);
  if (account == nullptr || !available(*account)) {
    return;
  }
  const QString root = storesRootFor(m_library->configRoot());
  const auto signedOut = [this, account] {
    account->signedIn = false;
    account->userName.clear();
    account->games.clear();
    settle(*account, QStringLiteral("Signed out."));
  };
  const std::optional<StoreClientRun> run = signOutRun(
      account->client, storeClientBinaries(m_library->toolSet().storeClients), root);
  if (!run) { // GOG keeps its sign-in in one file
    QFile::remove(gogAuthConfigPath(root));
    signedOut();
    return;
  }
  enqueue(*account, *run, [signedOut](const ProcessRunResult &) { signedOut(); });
}

void StoreAccounts::queueStatusAndLibrary(Account &account) {
  const QString root = storesRootFor(m_library->configRoot());
  const StoreClientRun status = accountStatusRun(
      account.client, storeClientBinaries(m_library->toolSet().storeClients), root);
  enqueue(account, status, [this, &account](const ProcessRunResult &result) {
    // Not signed in is an answer, not a failure (gogdl exits non-zero).
    const StoreAccountStatus parsed = parseAccountStatus(account.client, result.standardOutput);
    account.signedIn = parsed.signedIn;
    account.userName = parsed.userName;
    if (!parsed.signedIn) {
      account.games.clear();
      settle(account, result.timedOut ? QStringLiteral("The store did not answer. Try again.")
                                      : QStringLiteral("Not signed in."));
      return;
    }
    queueLibrary(account, parsed.accessToken);
  });
}

void StoreAccounts::queueLibrary(Account &account, const QString &gogToken) {
  if (account.client == StoreClient::Gog) {
    fetchGogPage(account, gogToken, 1, {});
    return;
  }
  const QVector<StoreClientRun> runs = libraryRuns(
      account.client, storeClientBinaries(m_library->toolSet().storeClients),
      storesRootFor(m_library->configRoot()));
  for (qsizetype i = 0; i < runs.size(); ++i) {
    const bool last = i + 1 == runs.size();
    enqueue(account, runs.at(i), [this, &account, last](const ProcessRunResult &result) {
      if (!succeeded(result)) {
        account.queue.clear();
        settle(account, QStringLiteral("Your library could not be loaded. Try again."));
        return;
      }
      if (!last) {
        return;
      }
      account.games = account.client == StoreClient::Epic
                          ? parseEpicLibrary(result.standardOutput)
                          : parseAmazonLibrary(result.standardOutput);
      settle(account, gamesSentence(account.games.size()));
    });
  }
}

void StoreAccounts::fetchGogPage(Account &account, const QString &token, int page,
                                 QVector<OwnedStoreGame> gathered) {
  if (m_network == nullptr) {
    m_network = new QNetworkAccessManager(this);
  }
  QNetworkRequest request(gogLibraryPageUrl(page));
  request.setRawHeader("Authorization", "Bearer " + token.toLatin1());
  request.setTransferTimeout(kGogRequestTimeoutMs);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  account.fetching = true;
  QNetworkReply *reply = m_network->get(request);
  connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
    if (received > kMaxGogPageBytes) {
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this,
          [this, &account, reply, token, page, gathered]() mutable {
            reply->deleteLater();
            account.fetching = false;
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || status != 200) {
              settle(account, QStringLiteral("Your GOG library could not be loaded. Try again."));
              return;
            }
            int totalPages = 0;
            gathered += parseGogLibraryPage(reply->readAll(), &totalPages);
            if (page < totalPages && page < kMaxGogPages) {
              fetchGogPage(account, token, page + 1, gathered);
              return;
            }
            account.games = gathered;
            settle(account, gamesSentence(account.games.size()));
          });
}

void StoreAccounts::enqueue(Account &account, StoreClientRun run,
                            std::function<void(const ProcessRunResult &)> then) {
  account.queue.append(Step{std::move(run), std::move(then)});
  account.busy = true;
  Q_EMIT accountsChanged();
  pump(account);
}

void StoreAccounts::pump(Account &account) {
  if (account.current || account.queue.isEmpty()) {
    return;
  }
  Step step = account.queue.takeFirst();
  // gogdl writes its sign-in file there without creating the folder.
  QDir().mkpath(storesRootFor(m_library->configRoot()));
  account.current = std::move(step.then);
  account.runner->start(step.run.spec);
}

void StoreAccounts::onFinished(Account &account, const ProcessRunResult &result) {
  auto then = std::move(account.current);
  account.current = nullptr;
  if (then) {
    then(result); // AGENT-GUARD: result output may hold secrets; never log it
  }
  pump(account);
  settle(account, account.message);
}

void StoreAccounts::settle(Account &account, const QString &message) {
  account.message = message;
  account.busy = bool(account.current) || !account.queue.isEmpty() || account.fetching;
  Q_EMIT accountsChanged();
}

} // namespace QindaQt::QindaLutris
