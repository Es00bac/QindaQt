// SPDX-License-Identifier: GPL-3.0-or-later
#include "launch_planner.h"

#include "library_store.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::QindaLutris {
namespace {

bool cleanPathValue(const QString &path) {
  if (path.size() > 4096) {
    return false;
  }
  for (const QChar ch : path) {
    if (ch.category() == QChar::Other_Control || ch.isNull()) {
      return false;
    }
  }
  return true;
}

void applySharedOptions(const Game &game, const LaunchOptions &options,
                        const LaunchToolSet &tools,
                        const QVector<DisplayTarget> &displays,
                        LaunchPlan *plan) {
  // The display pin is advisory and SDL-flavoured (ADR-0231): Wayland has no
  // mandatory protocol for placing a foreign window on an output, and the
  // SDL display index is the one convention games and their launchers
  // actually honour. A saved-but-absent display degrades to a note.
  if (!options.targetDisplay.isEmpty()) {
    bool found = false;
    for (const DisplayTarget &display : displays) {
      if (display.key == options.targetDisplay) {
        plan->environment.insert(QStringLiteral("SDL_VIDEO_FULLSCREEN_DISPLAY"),
                                 QString::number(display.sdlDisplayIndex));
        found = true;
        break;
      }
    }
    if (!found) {
      plan->notes.append(QStringLiteral(
          "the saved display is not connected; using the default"));
    }
  }
  if (options.mangohud) {
    plan->environment.insert(QStringLiteral("MANGOHUD"), QStringLiteral("1"));
  }
  for (const QString &line : options.extraEnvironment) {
    // Re-validated: the planner never trusts even our own store (ADR-0231).
    if (!isValidEnvironmentAssignment(line)) {
      plan->notes.append(QStringLiteral("ignored environment line: %1")
                             .arg(line.left(64)));
      continue;
    }
    const qsizetype equals = line.indexOf(QLatin1Char('='));
    plan->environment.insert(line.left(equals), line.mid(equals + 1));
  }
  // gamemode wraps outermost; the mangohud wrapper sits between it and the
  // game (OpenGL needs its LD_PRELOAD; Vulkan reads MANGOHUD=1). On Steam
  // the wrapper would wrap the client, so Steam launches use env only.
  const bool wantsWrapper =
      options.mangohud && game.source != GameSource::Steam;
  if (wantsWrapper && !tools.mangohudBinary.isEmpty()) {
    plan->arguments.prepend(plan->program);
    plan->program = tools.mangohudBinary;
  } else if (wantsWrapper) {
    plan->notes.append(QStringLiteral(
        "MangoHud's wrapper is not installed; relying on MANGOHUD=1"));
  }
  if (options.gamemode) {
    if (!tools.gamemodeRunBinary.isEmpty()) {
      plan->arguments.prepend(plan->program);
      plan->program = tools.gamemodeRunBinary;
    } else {
      plan->notes.append(QStringLiteral(
          "gamemode was requested but gamemoderun is not installed"));
    }
  }
}

LaunchPlan failed(const QString &reason) {
  LaunchPlan plan;
  plan.reason = reason;
  return plan;
}

} // namespace

QString discoverWineLoader(const QStringList &searchDirectories) {
  // A plain loader is the user's own selection when it exists.
  for (const QString &name : {QStringLiteral("wine"), QStringLiteral("wine64")}) {
    for (const QString &dir : searchDirectories) {
      const QFileInfo candidate(QDir(dir).filePath(name));
      if (candidate.isFile() && candidate.isExecutable()) {
        return candidate.absoluteFilePath();
      }
    }
  }

  // Otherwise take the highest-versioned versioned loader. Gentoo names these
  // wine64-proton-11.0.2, wine64-vanilla-10.0, and so on; compare by the
  // numeric fields so 11.0.2 beats 9.0 rather than losing a string sort.
  const QRegularExpression versioned(
      QStringLiteral("^wine(?:64)?-[a-z]+-([0-9]+(?:\\.[0-9]+)*)$"));
  QString best;
  QList<int> bestFields;
  for (const QString &dir : searchDirectories) {
    const QFileInfoList found = QDir(dir).entryInfoList(
        {QStringLiteral("wine-*"), QStringLiteral("wine64-*")},
        QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : found) {
      if (!info.isExecutable()) {
        continue;
      }
      const QRegularExpressionMatch match = versioned.match(info.fileName());
      if (!match.hasMatch()) {
        continue;
      }
      QList<int> fields;
      const QStringList parts = match.captured(1).split(QLatin1Char('.'));
      for (const QString &part : parts) {
        fields.append(part.toInt());
      }
      // Longer version vectors compare field by field; a missing field is 0.
      bool better = best.isEmpty();
      for (int i = 0; !better && i < qMax(fields.size(), bestFields.size()); ++i) {
        const int mine = i < fields.size() ? fields.at(i) : 0;
        const int theirs = i < bestFields.size() ? bestFields.at(i) : 0;
        if (mine != theirs) {
          better = mine > theirs;
          break;
        }
      }
      if (better) {
        best = info.absoluteFilePath();
        bestFields = fields;
      }
    }
  }
  return best;
}

LaunchPlan planGameLaunch(const Game &game, const LaunchOptions &options,
                          const LaunchToolSet &tools,
                          const QVector<DisplayTarget> &displays,
                          const DesktopDiscovery *desktopDiscovery) {
  LaunchPlan plan;
  switch (game.source) {
  case GameSource::Steam: {
    if (tools.steamBinary.isEmpty()) {
      return failed(QStringLiteral("Steam is not installed"));
    }
    plan.program = tools.steamBinary;
    plan.arguments = {QStringLiteral("steam://rungameid/%1").arg(game.appId)};
    break;
  }
  case GameSource::Lutris: {
    if (tools.lutrisBinary.isEmpty()) {
      return failed(QStringLiteral("Lutris is not installed"));
    }
    plan.program = tools.lutrisBinary;
    plan.arguments = {QStringLiteral("lutris:rungameid/%1").arg(game.sourceRef)};
    break;
  }
  case GameSource::Desktop: {
    if (desktopDiscovery == nullptr) {
      return failed(QStringLiteral("the desktop entry is not available"));
    }
    const DesktopLaunchPlan prepared =
        planDesktopGameLaunch(*desktopDiscovery, game);
    if (!prepared.ok) {
      return failed(prepared.reason);
    }
    plan.program = prepared.program;
    plan.arguments = prepared.arguments;
    plan.workingDirectory = prepared.workingDirectory;
    break;
  }
  case GameSource::Wine: {
    const WineRunner runner =
        options.runnerOverride.value_or(game.wineRunner);
    const QString prefix = options.prefixOverride.isEmpty()
                               ? game.winePrefix : options.prefixOverride;
    if (!cleanPathValue(game.installPath) || !cleanPathValue(prefix)) {
      return failed(QStringLiteral("the recorded paths are not usable"));
    }
    if (runner == WineRunner::Wine) {
      if (tools.wineBinary.isEmpty()) {
        return failed(QStringLiteral("Wine is not installed"));
      }
      plan.program = tools.wineBinary;
      plan.arguments = {game.installPath};
      if (!prefix.isEmpty()) {
        plan.environment.insert(QStringLiteral("WINEPREFIX"), prefix);
      }
    } else {
      QString script;
      for (const ProtonInstall &proton : tools.protons) {
        if (!game.protonPath.isEmpty() && proton.protonScript != game.protonPath) {
          continue;
        }
        script = proton.protonScript;
        break;
      }
      if (script.isEmpty()) {
        return failed(game.protonPath.isEmpty()
                          ? QStringLiteral("no Proton installation was found")
                          : QStringLiteral("the chosen Proton is not installed"));
      }
      if (prefix.isEmpty()) {
        return failed(QStringLiteral("Proton needs a prefix directory"));
      }
      plan.program = script;
      plan.arguments = {QStringLiteral("run"), game.installPath};
      plan.environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), prefix);
    }
    break;
  }
  }

  applySharedOptions(game, options, tools, displays, &plan);
  plan.ok = true;
  return plan;
}

} // namespace QindaQt::QindaLutris
