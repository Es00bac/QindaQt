// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "desktop_source.h"
#include "game.h"
#include "proton_catalog.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: launch planning for ADR-0231, in the posture of the shell
// launcher's bounded execution seam (ADR-0062): a plan is one program plus
// one argv vector plus typed environment overlays. There is NO shell anywhere
// in this boundary -- no system(), no command string, no interpolation.
// Every input is either repository-validated data (a manifest name, a pga id,
// a planned desktop Exec) or store-validated user data (re-validated here;
// the planner never trusts), and every unavailable ingredient degrades to a
// typed reason or a note, never to a guess.

// The host tools a launch may need, resolved once per refresh by composition
// (findExecutable); empty means "not installed". Injected, so tests never
// depend on the real machine.
// AGENT-CONTRACT: the Wine loader is NOT always called "wine". Gentoo's
// app-emulation/wine-proton installs versioned loaders -
// /usr/bin/wine64-proton-11.0.2 and friends - and ships no plain `wine` unless
// app-eselect/eselect-wine happens to be installed. Searching only for "wine"
// therefore reported "Wine is not installed" on a machine with a complete
// wine-proton stack, which is how both of this project's own machines are set
// up. Pure: the caller passes the directories to look in, so a test never
// depends on the host.
//
// Preference order: a plain `wine` (an eselect symlink or vanilla install)
// first, because that is the user's own choice when it exists; then `wine64`;
// then the highest-versioned `wine64-<flavour>-<version>` found. Returns empty
// when nothing qualifies, and the planner's "Wine is not installed" refusal
// then means what it says.
[[nodiscard]] QString discoverWineLoader(const QStringList &searchDirectories);

struct LaunchToolSet final {
  QString steamBinary;
  QString lutrisBinary;
  QString wineBinary;
  QString gamemodeRunBinary;   // gamemoderun wrapper
  QString mangohudBinary;      // mangohud wrapper (OpenGL; Vulkan uses env)
  // umu-run (games-util/umu-launcher), resolved by discoverUmuRun over
  // defaultUmuSearchPath (umu_launch.h); empty = not installed.
  QString umuRunBinary;
  // Every installed Proton build (discoverProtonBuilds). Pins resolve
  // against exactly this list; nothing falls back outside it.
  QVector<ProtonBuild> protonBuilds;

  friend bool operator==(const LaunchToolSet &, const LaunchToolSet &) = default;
};

// One display a game may be pinned to. `key` is the persisted identity
// (connector name, which QScreen keeps stable per session topology);
// `sdlDisplayIndex` follows QGuiApplication::screens() order, the ordering
// SDL documents for SDL_VIDEO_FULLSCREEN_DISPLAY.
struct DisplayTarget final {
  QString key;
  QString label;
  int sdlDisplayIndex = -1;

  friend bool operator==(const DisplayTarget &, const DisplayTarget &) = default;
};

struct LaunchPlan final {
  bool ok = false;
  // Presentation-safe reason when ok is false ("Steam is not installed").
  QString reason;
  QString program;
  QStringList arguments;
  QString workingDirectory;
  // Typed overlays over the session environment (never a replacement): the
  // display pin, MANGOHUD, WINEPREFIX/Proton paths, and the user's own
  // validated assignments.
  QHash<QString, QString> environment;
  // Non-fatal degradations worth one status line ("gamemode requested but
  // gamemoderun is not installed").
  QStringList notes;

  friend bool operator==(const LaunchPlan &, const LaunchPlan &) = default;
};

// Applies the per-game options every launch path shares: the SDL display
// pin, MANGOHUD=1, the user's re-validated extra environment, and the
// gamemoderun / mangohud wrappers around plan->program. allowMangohudWrapper
// is false where the wrapper would wrap the wrong process (the Steam client;
// umu-run, whose container does not pass the wrapper's LD_PRELOAD to the
// game -- MANGOHUD=1 reaches DXVK/VKD3D there instead). Shared by
// planGameLaunch and planUmuLaunch so the behaviours cannot drift.
void applyLaunchOptions(const LaunchOptions &options, const LaunchToolSet &tools,
                        const QVector<DisplayTarget> &displays,
                        bool allowMangohudWrapper, LaunchPlan *plan);

// Plans one launch. desktopDiscovery is required only for Desktop games and
// supplies the retained document text for the shared Exec planner. A Wine
// entry whose runner is Proton is planned through planUmuLaunch with its
// pinned build (ADR-0275). An Installed title needs its TitleRecord, which a
// Game does not carry: plan it with planTitleLaunch (umu_launch.h); passed
// here it is refused with a reason, never guessed.
[[nodiscard]] LaunchPlan planGameLaunch(
    const Game &game, const LaunchOptions &options,
    const LaunchToolSet &tools, const QVector<DisplayTarget> &displays,
    const DesktopDiscovery *desktopDiscovery);

} // namespace QindaQt::QindaLutris
