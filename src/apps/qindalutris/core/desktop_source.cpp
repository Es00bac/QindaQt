// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_source.h"

#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/application_catalog/launch_support.h>

namespace QindaQt::QindaLutris {

DesktopDiscovery scanDesktopGames(const QStringList &dataRoots) {
  DesktopDiscovery out;
  QString error;
  const ApplicationCatalog::DirectoryScan scan =
      ApplicationCatalog::scanApplicationDirectories(dataRoots, &error);
  if (!error.isEmpty()) {
    out.warnings.append(QStringLiteral("desktop scan degraded: %1").arg(error));
  }
  for (const ApplicationCatalog::ScannedApplication &app : scan.applications) {
    if (out.games.size() >= kMaxGames) {
      break;
    }
    if (!app.entry.categories.contains(QLatin1String("Game"))) {
      continue;
    }
    // AGENT-GUARD: the library must never list itself, whatever Categories
    // its own desktop file grows later.
    if (app.entry.id == QLatin1String("org.qindaqt.QindaLutris")) {
      continue;
    }
    Game game;
    game.id = QStringLiteral("desktop/%1").arg(app.entry.id);
    game.title = app.entry.name.left(kMaxGameTitleChars);
    game.source = GameSource::Desktop;
    game.sourceRef = app.entry.id;
    game.iconName = app.entry.iconName;
    out.documentTextById.insert(app.entry.id, app.documentText);
    out.games.append(game);
  }
  return out;
}

DesktopLaunchPlan planDesktopGameLaunch(const DesktopDiscovery &discovery,
                                        const Game &game) {
  DesktopLaunchPlan plan;
  const QString documentText =
      discovery.documentTextById.value(game.sourceRef);
  if (documentText.isEmpty()) {
    plan.reason = QStringLiteral("the desktop entry was not retained");
    return plan;
  }
  const ApplicationCatalog::LaunchPreparation prepared =
      ApplicationCatalog::planApplicationLaunch(documentText, QString(),
                                                game.title, game.sourceRef);
  switch (prepared.support) {
  case ApplicationCatalog::LaunchSupport::ProcessSpawn:
    plan.ok = true;
    plan.program = prepared.program;
    plan.arguments = prepared.arguments;
    return plan;
  case ApplicationCatalog::LaunchSupport::TerminalRequired:
    plan.reason = QStringLiteral("the game asks for a terminal");
    return plan;
  case ApplicationCatalog::LaunchSupport::DbusActivatable:
    plan.reason = QStringLiteral("the game needs D-Bus activation");
    return plan;
  case ApplicationCatalog::LaunchSupport::Unsupported:
    plan.reason = prepared.message.isEmpty()
                      ? QStringLiteral("the entry has no usable command")
                      : prepared.message;
    return plan;
  }
  Q_UNREACHABLE();
}

} // namespace QindaQt::QindaLutris
