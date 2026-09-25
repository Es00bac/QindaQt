// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game_launcher.h"
#include "process_tree.h"

#include <QString>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the production GameProcessLauncher (ADR-0275 section 4b).
// Every game QindaLutris starts itself runs in its own transient systemd user
// scope `qindalutris-game-<uuid>`, so Force quit can stop the game, its
// launcher, Wine's services and umu's container together -- wineserver -k
// cannot reach a session inside umu's container. Plans whose program is the
// Steam or Lutris client are NOT scoped: those clients own their games, and
// stopping the scope would stop the whole client.
// Without a systemd user manager it degrades to QProcessGameLauncher's plain
// detached start (and Force quit is then unavailable for that launch).
// Environment: launchEnvironment(plan, session) exactly as the plain launcher.
class ScopedGameLauncher final : public GameProcessLauncher {
public:
  ScopedGameLauncher(); // probes the user manager once (bounded)
  explicit ScopedGameLauncher(UserScopeTools tools); // tests

  LaunchOutcome launch(const LaunchPlan &plan) override;

  // The scope of the most recent successful launch; empty when unscoped.
  [[nodiscard]] QString lastScopeUnit() const { return m_lastUnit; }
  [[nodiscard]] qint64 lastProcessId() const { return m_lastPid; }
  [[nodiscard]] const UserScopeTools &tools() const { return m_tools; }

  // Pure: whether a plan should run in its own scope (see contract above).
  [[nodiscard]] static bool shouldScope(const LaunchPlan &plan);

private:
  UserScopeTools m_tools;
  QString m_lastUnit;
  qint64 m_lastPid = 0;
};

} // namespace QindaQt::QindaLutris
