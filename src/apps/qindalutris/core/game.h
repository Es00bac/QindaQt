// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the value vocabulary of QindaLutris, the native QindaQt
// game library (ADR-0231). One Game is one launchable thing on the machine,
// merged from up to four discovery sources. Every source is optional and
// every read is bounded; a machine with no games yields an empty list, never
// an error. Nothing in this header performs I/O.

// Where a game was discovered. The string forms are the stable persistence
// and filter vocabulary; never renumber without a store migration.
enum class GameSource {
  Steam,   // appmanifest_*.acf beneath a libraryfolders.vdf root
  Lutris,  // installed = 1 rows of Lutris's pga.db
  Desktop, // a Categories=Game .desktop entry (native Linux game)
  Wine,    // hand-added Windows executable + prefix
  // A title QindaLutris installed or adopted (ADR-0275): one TitleRecord in
  // titles-v1.json, launched through umu with its pinned Proton build.
  Installed,
};

[[nodiscard]] QString gameSourceId(GameSource source); // "steam" | "lutris" | "desktop" | "wine" | "installed"
[[nodiscard]] std::optional<GameSource> gameSourceForId(const QString &id);
// The user-facing source name ("Steam", "Lutris", "Native", "Wine",
// "Installed"), shared by the list model and the detail panel.
[[nodiscard]] QString gameSourceLabel(GameSource source);

// How a manual Wine entry is started. Proton runs through umu-run with the
// entry's pinned build as PROTONPATH (ADR-0275); Wine through the Wine
// loader.
enum class WineRunner {
  Wine,
  Proton,
};

[[nodiscard]] QString wineRunnerId(WineRunner runner); // "wine" | "proton"
[[nodiscard]] std::optional<WineRunner> wineRunnerForId(const QString &id);

// One discovered game. `id` is stable across refreshes and is the launch
// option store key: "steam/<appid>", "lutris/<pga id>", "desktop/<entry id>",
// "wine/<slug>", "title/<slug>" (Installed: the TitleRecord id). A Lutris row that names the same title as a Steam game is
// folded into the Steam record (ADR-0231), so its id never appears.
struct Game {
  QString id;
  QString title;
  GameSource source = GameSource::Desktop;
  QString sourceRef;   // appid, pga id, desktop entry id, or store slug
  QString installPath; // best-known directory or file; may be empty
  QString coverPath;   // local cover image when one was found; may be empty
  QString iconName;    // desktop-entry icon name (Desktop source only)
  quint64 appId = 0;   // Steam appid when source is Steam
  // Manual Wine entries and Installed titles:
  QString winePrefix;
  WineRunner wineRunner = WineRunner::Wine; // Installed titles: always Proton
  // The pinned Proton build (ADR-0275; see proton_pin.h): a build directory
  // name, or -- for hand-added entries saved before ADR-0275 -- an absolute
  // path of a build or its `proton` script, plus the build's version text
  // when it was pinned. Empty protonPath means NO build is chosen; a Proton
  // launch then refuses. It never means "any Proton".
  QString protonPath;
  QString protonVersion;
  // Install size is only ever shown when honestly known (ADR-0231): manual
  // Wine entries report the executable's byte size. Nullopt everywhere else;
  // the UI must say "not tracked", never invent a number.
  std::optional<quint64> installSizeBytes;

  friend bool operator==(const Game &, const Game &) = default;
};

// Per-game launch tuning, persisted app-locally (ADR-0198 precedent). All
// defaults match a plain launch, so a game with no stored options launches
// exactly as its source intends.
struct LaunchOptions {
  bool gamemode = false;          // prefix with gamemoderun when installed
  bool mangohud = false;          // MANGOHUD=1 (+ mangohud wrapper off-Steam)
  QString targetDisplay;          // stable display key; empty = compositor default
  QStringList extraEnvironment;   // validated KEY=VALUE entries
  // Manual Wine entries may switch runner/prefix after the fact.
  std::optional<WineRunner> runnerOverride;
  QString prefixOverride;

  friend bool operator==(const LaunchOptions &, const LaunchOptions &) = default;
};

// Ceilings shared by every source reader and the store. They mirror the
// bounded-input posture of the compositor's Steam/VDF readers (ADR-0230):
// hostile or corrupt local data degrades to fewer games, never to a crash,
// a hang, or an error dialog.
inline constexpr int kMaxGameTitleChars = 256;
inline constexpr int kMaxGames = 4096;
inline constexpr qint64 kMaxStoreBytes = qint64(1024) * 1024;
inline constexpr int kMaxWineEntries = 256;
inline constexpr int kMaxExtraEnvironmentEntries = 32;
inline constexpr int kMaxEnvironmentValueChars = 1024;

// The case-folded, punctuation-stripped comparison form used by the
// Steam/Lutris de-duplication rule. Bounded input; control characters and
// symbols Valve/Lutris decorate names with (trademark signs, colons) do not
// survive, so "Half-Life 2: Update" and "half life 2 update" fold together.
[[nodiscard]] QString normalizedTitleForMatch(const QString &title);

} // namespace QindaQt::QindaLutris
