// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "launch_planner.h"
#include "store_clients.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <functional>
#include <optional>

class QNetworkAccessManager;

namespace QindaQt::QindaLutris {

class LibraryController;
class ProcessRunner;
struct ProcessRunResult;

// <configRoot>/stores: the one place QindaLutris's store clients keep
// their sign-ins and caches (storeClientEnvironment).
[[nodiscard]] QString storesRootFor(const QString &configRoot);
// Finds legendary, gogdl and nile on PATH and pairs them with the
// environment that pins them to storesRootFor(configRoot).
[[nodiscard]] StoreClientSet discoverStoreClients(const QString &configRoot);
[[nodiscard]] StoreClientBinaries storeClientBinaries(const StoreClientSet &set);

// AGENT-CONTRACT: the signed-in store accounts of ADR-0275 section 8,
// exposed to QML as `Accounts`. Sign-in is the store's own web page -- in
// an embedded browser when QtWebEngine is available (webSignInAvailable),
// otherwise the user's browser plus a "paste the address here" field --
// and ends in captureSignInCode + the client's own code exchange. Nothing
// here types, stores or shows a password.
// Each store runs its client steps one at a time on its own runner, so an
// Epic refresh never waits for GOG. Every step's result is one plain
// sentence in the account's `message`; secret-carrying output is parsed
// and dropped, never logged (store_clients.h guard).
// Installing an owned game is Installs.installOwnedGame (InstallController):
// the same pin, prefix and fixes path as every other install.
class StoreAccounts final : public QObject {
  Q_OBJECT
  // Rows: id, name, available, signedIn, userName, busy, message, gameCount.
  Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
  // storeId -> ownedGames(storeId), for bindings.
  Q_PROPERTY(QVariantMap ownedGamesByStore READ ownedGamesByStore NOTIFY accountsChanged)
  Q_PROPERTY(bool webSignInAvailable READ webSignInAvailable CONSTANT)
public:
  using RunnerFactory = std::function<ProcessRunner *(QObject *parent)>;

  StoreAccounts(LibraryController *library, bool webSignInAvailable,
                RunnerFactory runners = {}, QObject *parent = nullptr);
  ~StoreAccounts() override;

  [[nodiscard]] QVariantList accounts() const;
  [[nodiscard]] QVariantMap ownedGamesByStore() const;
  [[nodiscard]] bool webSignInAvailable() const { return m_webSignIn; }

  // Rows: id, title, imageUrl, installedTitleId (empty when not installed).
  Q_INVOKABLE QVariantList ownedGames(const QString &storeId) const;
  // Emits signInReady with the page to open (Amazon asks nile first).
  Q_INVOKABLE void beginSignIn(const QString &storeId);
  Q_INVOKABLE bool isSignInFinishedUrl(const QString &storeId, const QUrl &url) const;
  // text: the finished address, the page's text (Epic) or a pasted code.
  Q_INVOKABLE void finishSignIn(const QString &storeId, const QString &text);
  Q_INVOKABLE void signOut(const QString &storeId);
  Q_INVOKABLE void refresh(const QString &storeId);
  Q_INVOKABLE void refreshAll();

  static constexpr int kMaxGogPages = 50;
  static constexpr qint64 kMaxGogPageBytes = 8 * 1024 * 1024;
  static constexpr int kGogRequestTimeoutMs = 30 * 1000;

Q_SIGNALS:
  void accountsChanged();
  void signInReady(const QString &storeId, const QUrl &url);

private:
  struct Step {
    StoreClientRun run;
    std::function<void(const ProcessRunResult &)> then;
  };
  struct Account {
    StoreClient client = StoreClient::Epic;
    ProcessRunner *runner = nullptr; // owned child
    QList<Step> queue;
    std::function<void(const ProcessRunResult &)> current;
    bool busy = false;
    bool signedIn = false;
    QString userName;
    QString message;
    QVector<OwnedStoreGame> games;
    std::optional<AmazonSignInStart> amazonStart;
    bool fetching = false; // a GOG library page request is in flight
    quint64 gogGeneration = 0; // bumped to drop stale GOG page replies
  };

  Account *accountFor(const QString &storeId);
  [[nodiscard]] const Account *accountFor(const QString &storeId) const;
  [[nodiscard]] bool available(const Account &account) const;
  void enqueue(Account &account, StoreClientRun run,
               std::function<void(const ProcessRunResult &)> then);
  void pump(Account &account);
  void onFinished(Account &account, const ProcessRunResult &result);
  void queueStatusAndLibrary(Account &account);
  void queueLibrary(Account &account, const QString &gogToken);
  void fetchGogPage(Account &account, const QString &token, int page,
                    QVector<OwnedStoreGame> gathered);
  void settle(Account &account, const QString &message);

  LibraryController *m_library = nullptr;
  bool m_webSignIn = false;
  std::array<Account, 3> m_accounts;
  QNetworkAccessManager *m_network = nullptr; // owned child, made on first GOG use
};

} // namespace QindaQt::QindaLutris
