// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"
#include "launch_planner.h"
#include "title_record.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: every Windows title runs through umu-run with ONE exact,
// recorded Proton build (ADR-0275 section 1). The plan is the ordinary
// LaunchPlan shape, handed to the same GameProcessLauncher seam:
//   program     = umu-run (or gamemoderun wrapping it)
//   arguments   = [executable, arguments...]
//   environment = WINEPREFIX=<prefix>, PROTONPATH=<absolute build dir>,
//                 GAMEID=<umuId|umu-0>, STORE=<umuStore|none>,
//                 UMU_RUNTIME_UPDATE=0, plus the shared launch options.
// AGENT-GUARD: PROTONPATH is ALWAYS the absolute directory of a build found
// in LaunchToolSet::protonBuilds by resolvePinnedBuild. Never pass a pin
// string through, never substitute, never omit it (umu would then pick its
// own). The five reserved keys are written LAST, so neither a record's
// environment nor the user's extra environment can override them; an
// attempt is dropped with a note. UMU_RUNTIME_UPDATE=0 is present on every
// plan so the Steam Runtime changes only when someone updates it on purpose.
// Refusals carry one plain-language sentence (ADR-0275 section 5).

struct UmuLaunchRequest final {
  QString executable;        // absolute unix path of the Windows program
  QStringList arguments;
  QString prefixPath;        // WINEPREFIX; must be absolute
  QString protonBuild;       // the pin: build name or absolute build path
  QString umuId;             // GAMEID; empty => "umu-0"
  QString umuStore;          // STORE; empty => "none"
  QStringList environment;   // record-level KEY=VALUE, re-validated here
  // Titles QindaLutris installed must still have their prefix; a hand-added
  // entry's prefix may not exist yet (umu creates it on first run).
  bool prefixMustExist = true;

  friend bool operator==(const UmuLaunchRequest &, const UmuLaunchRequest &) = default;
};

// Plans one umu launch. Stats the executable (and the prefix when required)
// -- bounded, no walk -- so "the file is gone" is a refusal with a reason
// rather than a failed spawn. Refusal order: umu missing, pin (not chosen /
// floating alias / not installed), unusable paths, prefix missing,
// executable missing. The working directory is the executable's directory,
// which Windows programs commonly assume.
[[nodiscard]] LaunchPlan planUmuLaunch(const UmuLaunchRequest &request,
                                       const LaunchOptions &options,
                                       const LaunchToolSet &tools,
                                       const QVector<DisplayTarget> &displays);

// The request a stored title makes. launcherTitleId does not change the
// plan: a launcher-started game's record already names the executable and
// arguments that start it inside the shared prefix.
[[nodiscard]] UmuLaunchRequest umuRequestForTitle(const TitleRecord &title);

// planUmuLaunch(umuRequestForTitle(title), ...). The controller's path for
// GameSource::Installed. Runner/prefix overrides in options do not apply to
// titles: their prefix and build are recorded, and moving either is an
// explicit action, not a launch option.
[[nodiscard]] LaunchPlan planTitleLaunch(const TitleRecord &title,
                                         const LaunchOptions &options,
                                         const LaunchToolSet &tools,
                                         const QVector<DisplayTarget> &displays);

// Directories to search for umu-run, highest preference first: /usr/bin
// (the games-util/umu-launcher package), then pathDirectories (PATH), then
// home/.local/bin (a user copy). Duplicates collapse. Pure.
[[nodiscard]] QStringList defaultUmuSearchPath(const QString &home,
                                               const QStringList &pathDirectories);

// The first executable regular file named umu-run in searchDirectories, as
// an absolute path, or empty. Pure over the injected directories.
[[nodiscard]] QString discoverUmuRun(const QStringList &searchDirectories);

} // namespace QindaQt::QindaLutris
