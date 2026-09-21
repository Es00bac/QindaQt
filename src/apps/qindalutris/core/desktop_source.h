// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game.h"
#include <QHash>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: native Linux games are the `Categories=Game` slice of the
// installed .desktop inventory (ADR-0231). Discovery and launch planning are
// NOT re-implemented here: scanning is the shared apps-side
// QindaQt::ApplicationCatalog (injected roots, bounded, validated), and argv
// planning is its planApplicationLaunch over the retained document text, so
// an Exec line is expanded by the same fail-closed grammar the shell
// launcher uses -- never by us, never through a shell.

struct DesktopDiscovery final {
  QVector<Game> games;
  // Retained per entry id for launch planning (the exact validated document
  // text the scanner produced; see planDesktopGameLaunch).
  QHash<QString, QString> documentTextById;
  QStringList warnings;
};

// dataRoots are the XDG data directories to scan (their applications/ trees),
// injected by composition. Entries whose Categories carry the case-sensitive
// "Game" key become games; everything else is ignored. QindaLutris's own
// desktop file is excluded defensively so the library can never list itself.
[[nodiscard]] DesktopDiscovery scanDesktopGames(const QStringList &dataRoots);

// The argv for one discovered desktop game, planned from the retained
// document through QindaQt::ApplicationCatalog. Entries that require a
// terminal or D-Bus activation are reported not-launchable with a reason
// rather than being approximated.
struct DesktopLaunchPlan final {
  bool ok = false;
  QString reason;      // why ok is false, presentation-safe
  QString program;
  QStringList arguments;
  QString workingDirectory;
};

[[nodiscard]] DesktopLaunchPlan planDesktopGameLaunch(
    const DesktopDiscovery &discovery, const Game &game);

} // namespace QindaQt::QindaLutris
