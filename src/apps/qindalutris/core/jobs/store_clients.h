// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "process_runner.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: Epic, GOG and Amazon accounts through their open-source
// clients (ADR-0275 section 8): legendary, gogdl and nile, the same tools
// Heroic Games Launcher uses. Everything here is pure -- sign-in addresses,
// reading the one-time code a sign-in page hands back, the exact argv of
// every client run, and parsers for what the clients print -- so it is
// tested against recorded output without a network or an account.
// Formats verified against legendary 0.20.34, gogdl 1.3.0 and nile 1.2.0
// source on 2026-09-25.
//
// AGENT-GUARD: sign-in codes and tokens are secrets. They go to the client
// as argv (visible only to the same user, as with Heroic) and are never
// written to a job log, a details text, a note or a status line. Callers
// must drop the stdout of every run marked `carriesSecrets`.
//
// Every client reads ONLY QindaLutris's own configuration directory
// (storeClientEnvironment), never the user's own ~/.config/legendary etc.,
// so a hand-made setup can neither leak into nor redirect QindaLutris.

enum class StoreClient { Epic, Gog, Amazon };

[[nodiscard]] QString storeClientId(StoreClient client); // "egs" | "gog" | "amazon"
[[nodiscard]] std::optional<StoreClient> storeClientForId(const QString &id);
[[nodiscard]] QString storeClientDisplayName(StoreClient client);

struct StoreClientBinaries final {
  QString legendary;
  QString gogdl;
  QString nile;
  [[nodiscard]] QString forClient(StoreClient client) const;
};

// <storesRoot> = <QindaLutris config root>/stores:
//   LEGENDARY_CONFIG_PATH = <storesRoot>/legendary
//   GOGDL_CONFIG_PATH     = <storesRoot>   (gogdl appends heroic_gogdl)
//   NILE_CONFIG_PATH      = <storesRoot>   (nile appends nile)
[[nodiscard]] QHash<QString, QString> storeClientEnvironment(const QString &storesRoot);
[[nodiscard]] QString gogAuthConfigPath(const QString &storesRoot);
[[nodiscard]] QString nileConfigDirectory(const QString &storesRoot);

// ---- Sign-in -----------------------------------------------------------
// Epic and GOG sign in on the store's own web page; the page finishes on a
// known address that carries a one-time code. Amazon first asks nile for a
// sign-in address (amazonSignInStartSpec), because the code only works
// with the device identity nile generated for it.
[[nodiscard]] QUrl epicSignInUrl();
[[nodiscard]] QUrl gogSignInUrl();

struct AmazonSignInStart final {
  QUrl url;
  QString clientId;
  QString codeVerifier; // secret
  QString serial;
};
[[nodiscard]] std::optional<AmazonSignInStart> parseAmazonSignInStart(const QByteArray &json);

// True when a sign-in page has reached the address that carries the code:
//   Epic   https://www.epicgames.com/id/api/redirect...  (code in the page)
//   GOG    https://embed.gog.com/on_login_success?...code=...
//   Amazon https://www.amazon.com/...?openid.oa2.authorization_code=...
[[nodiscard]] bool isSignInFinishedUrl(StoreClient client, const QUrl &url);

// The one-time code from whatever the user or the embedded page provides:
// the finished address, the page's JSON (Epic: {"authorizationCode": ...}),
// or the bare code. nullopt when nothing code-shaped is there.
[[nodiscard]] std::optional<QString> captureSignInCode(StoreClient client,
                                                       const QString &text);

// ---- Client runs --------------------------------------------------------
struct StoreClientRun final {
  ProcessRunSpec spec;
  bool carriesSecrets = false; // argv or stdout holds a code or token
};

// nile auth --login --non-interactive  (prints AmazonSignInStart JSON)
[[nodiscard]] StoreClientRun amazonSignInStartRun(const StoreClientBinaries &clients,
                                                  const QString &storesRoot);
// Epic   legendary auth --code <code> --disable-webview
// GOG    gogdl --auth-config-path <file> auth --code <code>
// Amazon nile register --code <c> --client-id <id> --code-verifier <v> --serial <s>
// amazon must be set for Amazon.
[[nodiscard]] StoreClientRun signInCompleteRun(StoreClient client,
                                               const StoreClientBinaries &clients,
                                               const QString &storesRoot, const QString &code,
                                               const std::optional<AmazonSignInStart> &amazon);
// Epic   legendary status --json --offline
// GOG    gogdl --auth-config-path <file> auth       (prints a fresh token)
// Amazon nile auth --status
[[nodiscard]] StoreClientRun accountStatusRun(StoreClient client,
                                              const StoreClientBinaries &clients,
                                              const QString &storesRoot);
// Epic   legendary auth --delete ; Amazon nile auth --logout ; GOG has no
// command (the caller removes gogAuthConfigPath) -> nullopt.
[[nodiscard]] std::optional<StoreClientRun> signOutRun(StoreClient client,
                                                       const StoreClientBinaries &clients,
                                                       const QString &storesRoot);
// Epic   legendary list --json          (refreshes from Epic)
// Amazon nile library sync, then nile library list --json (two runs)
// GOG    none: gogLibraryPageUrl with the status run's token.
[[nodiscard]] QVector<StoreClientRun> libraryRuns(StoreClient client,
                                                  const StoreClientBinaries &clients,
                                                  const QString &storesRoot);
// Epic   legendary -y install <app> --base-path <dir> --platform Windows --skip-sdl
// GOG    gogdl --auth-config-path <file> download <id> --platform windows --path <dir> --skip-dlcs
// Amazon nile install <id> --base-path <dir>
// nullopt when the id is not isSafeStoreGameId-shaped or baseDir is relative.
[[nodiscard]] std::optional<StoreClientRun> installRun(StoreClient client,
                                                       const StoreClientBinaries &clients,
                                                       const QString &storesRoot,
                                                       const QString &gameId,
                                                       const QString &baseDir);
// Where the installed game's program is, asked of the client itself:
// Epic   legendary list-installed --json
// Amazon nile launch <id> --json      (a dry run: prints the command only)
// GOG    none: readGogInstall reads goggame-<id>.info.
[[nodiscard]] std::optional<StoreClientRun> installedInfoRun(StoreClient client,
                                                             const StoreClientBinaries &clients,
                                                             const QString &storesRoot,
                                                             const QString &gameId);

inline constexpr int kStoreAuthTimeoutMs = 90 * 1000;
// A first `legendary list` of a large library fetches metadata per game.
inline constexpr int kStoreLibraryTimeoutMs = 15 * 60 * 1000;
inline constexpr int kStoreInstallTimeoutMs = 24 * 60 * 60 * 1000;

// ---- Parsers ------------------------------------------------------------
struct StoreAccountStatus final {
  bool signedIn = false;
  QString userName;    // may be empty (GOG does not report one)
  QString accessToken; // GOG only; secret
};
[[nodiscard]] StoreAccountStatus parseAccountStatus(StoreClient client, const QByteArray &out);

struct OwnedStoreGame final {
  QString id;       // the client's id: Epic app name, GOG product id, Amazon product id
  QString title;
  QUrl imageUrl;    // https only; may be empty
  friend bool operator==(const OwnedStoreGame &, const OwnedStoreGame &) = default;
};
inline constexpr int kMaxOwnedGames = 5000;
// legendary list --json. Third-party titles (EA app / Ubisoft Connect
// games sold on Epic) are left out: they install through their own
// launcher, which Get games offers separately.
[[nodiscard]] QVector<OwnedStoreGame> parseEpicLibrary(const QByteArray &json);
// nile library list --json
[[nodiscard]] QVector<OwnedStoreGame> parseAmazonLibrary(const QByteArray &json);
// https://embed.gog.com/account/getFilteredProducts?mediaType=1&page=<n>
// (Authorization: Bearer <token>). Only Windows-capable games are kept.
[[nodiscard]] QUrl gogLibraryPageUrl(int page);
[[nodiscard]] QVector<OwnedStoreGame> parseGogLibraryPage(const QByteArray &json,
                                                          int *totalPages);

struct StoreGameInstall final {
  QString installDir; // absolute
  QString executable; // absolute unix path of the game's program
};
[[nodiscard]] std::optional<StoreGameInstall> parseEpicInstalled(const QByteArray &json,
                                                                 const QString &appName);
[[nodiscard]] std::optional<StoreGameInstall> parseAmazonLaunchInfo(const QByteArray &json);
// Parses goggame-<id>.info's primary play task.
[[nodiscard]] std::optional<StoreGameInstall> parseGogGameInfo(const QByteArray &json,
                                                               const QString &installDir);
// Finds <baseDir>/<folder>/goggame-<id>.info among baseDir's direct
// children (gogdl names the folder) and parses it. Bounded; no deep walk.
[[nodiscard]] std::optional<StoreGameInstall> readGogInstall(const QString &baseDir,
                                                             const QString &gogId);

// "... = Progress: 12.34% (...)" / "= Progress: 12.34 1/2 ..." -> 0.1234.
[[nodiscard]] std::optional<double> parseDownloadProgress(const QString &line);

} // namespace QindaQt::QindaLutris
