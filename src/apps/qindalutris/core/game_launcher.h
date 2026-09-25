// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "launch_planner.h"

#include <QProcessEnvironment>
#include <QString>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the process-start seam of QindaLutris (ADR-0231), in the
// posture of the shell launcher's LaunchSpawner (ADR-0062): this is the only
// place a game process may be started, implementations never invoke a shell,
// and tests inject a recording fake. The plan is already argv-shaped; the
// spawner adds no interpretation of its own.

struct LaunchOutcome final {
  bool ok = false;
  QString diagnostic;

  friend bool operator==(const LaunchOutcome &, const LaunchOutcome &) = default;
};

// The environment a plan's process starts with: base minus
// plan.unsetEnvironment and every key starting with one of
// plan.unsetEnvironmentPrefixes, then plan.environment inserted. Pure, so the unset
// contract is testable without starting a process.
[[nodiscard]] QProcessEnvironment launchEnvironment(
    const LaunchPlan &plan, const QProcessEnvironment &base);

class GameProcessLauncher {
public:
  virtual ~GameProcessLauncher() = default;
  virtual LaunchOutcome launch(const LaunchPlan &plan) = 0;
};

// Production spawner: QProcess::startDetached with the session environment
// plus the plan's typed overlays (games legitimately need the session's
// DISPLAY/WAYLAND_DISPLAY/XDG variables; the overlays are validated at plan
// time). Confined to the caller's thread.
class QProcessGameLauncher final : public GameProcessLauncher {
public:
  LaunchOutcome launch(const LaunchPlan &plan) override;
};

} // namespace QindaQt::QindaLutris
