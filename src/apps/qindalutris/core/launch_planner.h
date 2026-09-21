// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "desktop_source.h"
#include "game.h"
#include "steam_source.h"

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
struct LaunchToolSet final {
  QString steamBinary;
  QString lutrisBinary;
  QString wineBinary;
  QString gamemodeRunBinary;   // gamemoderun wrapper
  QString mangohudBinary;      // mangohud wrapper (OpenGL; Vulkan uses env)
  QVector<ProtonInstall> protons;

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

// Plans one launch. desktopDiscovery is required only for Desktop games and
// supplies the retained document text for the shared Exec planner.
[[nodiscard]] LaunchPlan planGameLaunch(
    const Game &game, const LaunchOptions &options,
    const LaunchToolSet &tools, const QVector<DisplayTarget> &displays,
    const DesktopDiscovery *desktopDiscovery);

} // namespace QindaQt::QindaLutris
