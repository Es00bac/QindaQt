// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_menu/desktop_menu_types.h"

#include <QObject>
#include <QString>

namespace QindaQt::Shell::DesktopMenu {

// The desktop menu's only reach into the shell: one port the shell
// composition implements over the EXISTING controllers (system menu, places,
// workspaces, launcher, clipboard applet, Settings routes, gather overview,
// shortcut note, desktop-icons surface). The controller never sees those
// objects, a bus, a process, or a path.
//
// AGENT-CONTRACT: GUI-thread only. perform() dispatches exactly one request
// to the one controller that owns `command` and returns true only when that
// controller accepted it; it never retries, never falls back to another
// route, and never executes anything the command does not name.
// factsChanged() fires whenever any value facts() reports may have changed.
class DesktopMenuTargets : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~DesktopMenuTargets() override = default;

  [[nodiscard]] virtual DesktopMenuFacts facts() const = 0;
  virtual bool perform(const DesktopMenuCommand &command) = 0;
  // The owning controller's last refusal, for diagnostics; empty after success.
  [[nodiscard]] virtual QString lastFailure() const = 0;

Q_SIGNALS:
  void factsChanged();
};

} // namespace QindaQt::Shell::DesktopMenu
