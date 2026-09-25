// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: one Windows title QindaLutris installed or adopted
// (ADR-0275 sections 1 and 6), persisted by TitleStore in titles-v1.json
// and launched by planTitleLaunch (umu_launch.h). Producers are the
// install/adopt jobs and store recipes (other work packages); they must
// build records that pass validateTitleRecord, because the store refuses to
// write -- and on read refuses WHOLE -- anything that does not.
//
// The fields that matter most are protonBuild and protonBuildVersion: the
// exact build directory name the title was installed with and that
// build's `version` file line at the time (identity = name + version,
// ADR-0275 section 2). Both are REQUIRED, never empty; protonBuild is never
// a floating alias. Nothing in QindaLutris rewrites them except an explicit
// per-title action (confirmPinnedBuild, or a "move to another build"). A
// title never moves by itself.

enum class TitleKind {
  StoreLauncher,  // Battle.net, EA app, ... installed from a store recipe
  StoreGame,      // a game owned through a store (maybe via a launcher)
  SetupInstalled, // a game installed from the user's own setup file
};

// The store a title belongs to. String forms are persisted; never renumber
// or rename without a titles-v2 schema.
enum class GameStore {
  None,
  Steam,
  BattleNet,
  Ea,
  Ubisoft,
  Egs,
  Gog,
  Amazon,
};

[[nodiscard]] QString titleKindId(TitleKind kind); // "storeLauncher" | "storeGame" | "setupInstalled"
[[nodiscard]] std::optional<TitleKind> titleKindForId(const QString &id);
[[nodiscard]] QString gameStoreId(GameStore store); // "none" | "steam" | "battlenet" | "ea" | "ubisoft" | "egs" | "gog" | "amazon"
[[nodiscard]] std::optional<GameStore> gameStoreForId(const QString &id);

struct TitleRecord final {
  QString id;          // "title/<slug>", stable; also the Game id
  QString title;
  TitleKind kind = TitleKind::SetupInstalled;
  GameStore store = GameStore::None;
  QString storeGameId; // the store's own id for the game; may be empty
  QString prefixPath;  // absolute WINEPREFIX directory
  QString protonBuild; // pinned build directory name (see contract above)
  QString protonBuildVersion; // that build's versionText when pinned
  QString umuId;       // GAMEID; empty launches as "umu-0"
  QString umuStore;    // STORE; empty launches as "none"
  QString executable;  // absolute unix path of the Windows program
  QStringList arguments;
  QStringList environment;       // KEY=VALUE, never a reserved key
  QString launcherTitleId;       // "title/<slug>" of the launcher; may be empty
  QStringList winetricksApplied; // verbs already run in the prefix
  QString installedAt;           // ISO date, YYYY-MM-DD

  friend bool operator==(const TitleRecord &, const TitleRecord &) = default;
};

inline constexpr int kMaxTitles = 256;
inline constexpr int kMaxTitleArguments = 64;
inline constexpr int kMaxWinetricksVerbs = 64;
inline constexpr int kMaxTitlePathChars = 4096;
inline constexpr int kMaxTitleSlugChars = 96;

// AGENT-CONTRACT: environment keys a title, compatibility-database advice,
// or a user's extra environment may never set for an umu launch, because
// the umu plan owns them:
//   - set by the plan: WINEPREFIX, PROTONPATH, GAMEID, STORE,
//     UMU_RUNTIME_UPDATE;
//   - removed by the plan (umuUnsetEnvironmentKeys/Prefixes): UMU_NO_PROTON,
//     RUNTIMEPATH, PROTON_VERB, which let umu run something other than the
//     pinned build; LD_PRELOAD, LD_LIBRARY_PATH, LD_AUDIT and every
//     PYTHON* key (prefix match, case-sensitive like POSIX), which steer
//     umu-run's own Python interpreter before any container starts
//     (PYTHONWARNINGS can even import an arbitrary module).
// validateTitleRecord refuses them; planUmuLaunch drops them with a note.
// Producers of environment advice (the compat DB) must filter with this.
[[nodiscard]] bool isReservedUmuEnvironmentKey(const QString &key);

// The reserved keys an umu plan REMOVES from the inherited session
// environment rather than sets: UMU_NO_PROTON, RUNTIMEPATH, PROTON_VERB,
// LD_PRELOAD, LD_LIBRARY_PATH, LD_AUDIT.
[[nodiscard]] QStringList umuUnsetEnvironmentKeys();

// Key prefixes an umu plan removes from the inherited environment: PYTHON.
[[nodiscard]] QStringList umuUnsetEnvironmentPrefixes();

// "title/" + a lower-case [a-z0-9-] slug of at most kMaxTitleSlugChars.
[[nodiscard]] bool isValidTitleId(const QString &id);

// A fresh id for a new title: the normalized title as a slug, suffixed
// "-2", "-3", ... until it is not in takenIds. Pure.
[[nodiscard]] QString makeTitleId(const QString &title, const QStringList &takenIds);

// Every field against the ADR-0275 rules. Empty *why on success; otherwise
// a short developer-facing description of the first violation. Pure: it
// does not stat the prefix or the executable (they may be on a disk that
// is not mounted right now; the launch planner reports that honestly).
// AGENT-NOTE: executable is NOT required to lie under prefixPath -- Wine
// drive mappings let a game live on another disk -- only to be absolute.
[[nodiscard]] bool validateTitleRecord(const TitleRecord &record, QString *why);

} // namespace QindaQt::QindaLutris
