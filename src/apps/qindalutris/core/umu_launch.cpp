// SPDX-License-Identifier: GPL-3.0-or-later
#include "umu_launch.h"

#include "library_store.h"
#include "store_launch.h"
#include "store_io.h"

#include <QDir>
#include <QFileInfo>

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxUmuPathChars = 4096;

// The reserved keys the plan SETS (the rest of the reserved set is removed).
const QStringList kPlanSetKeys{
    QStringLiteral("WINEPREFIX"), QStringLiteral("PROTONPATH"),
    QStringLiteral("GAMEID"), QStringLiteral("STORE"),
    QStringLiteral("UMU_RUNTIME_UPDATE")};

LaunchPlan refused(const QString &reason) {
  LaunchPlan plan;
  plan.reason = reason;
  return plan;
}

bool usableAbsolutePath(const QString &path) {
  return !path.isEmpty() && QDir::isAbsolutePath(path)
         && StoreIo::isCleanLine(path, kMaxUmuPathChars);
}

QString keyOf(const QString &assignment) {
  return assignment.left(assignment.indexOf(QLatin1Char('=')));
}

} // namespace

LaunchPlan planUmuLaunch(const UmuLaunchRequest &request,
                         const LaunchOptions &options,
                         const LaunchToolSet &tools,
                         const QVector<DisplayTarget> &displays) {
  if (tools.umuRunBinary.isEmpty()) {
    return refused(QStringLiteral(
        "umu is not installed. Install games-util/umu-launcher."));
  }
  const PinnedBuildResolution pin = resolvePinnedBuild(
      {request.protonBuild, request.protonBuildVersion}, tools.protonBuilds);
  if (!pin.ok()) {
    return refused(pin.reason); // AGENT-GUARD: no fallback build, ever
  }
  // The catalog is a snapshot from the last refresh; the build may have
  // been removed since. Re-check the one entry point umu will run.
  if (!protonBuildStillPresent(*pin.build)) {
    return refused(protonNotInstalledReason(request.protonBuild));
  }
  if (!usableAbsolutePath(request.executable)
      || !usableAbsolutePath(request.prefixPath)) {
    return refused(QStringLiteral(
        "This game's saved location is not usable. Choose its program and "
        "Wine prefix again."));
  }
  const QFileInfo prefix(request.prefixPath);
  if (request.prefixMustExist && !prefix.isDir()) {
    return refused(QStringLiteral(
        "This game's Wine prefix is missing. Reinstall the game or choose "
        "its prefix again."));
  }
  if (!request.prefixMustExist && prefix.exists() && !prefix.isDir()) {
    return refused(QStringLiteral(
        "This game's Wine prefix is not a folder. Choose its prefix again."));
  }
  const QFileInfo executable(request.executable);
  if (!executable.isFile()) {
    return refused(QStringLiteral(
        "This game's program file is missing. Reinstall the game or choose "
        "its program again."));
  }

  LaunchPlan plan;
  plan.program = tools.umuRunBinary;
  plan.arguments = QStringList{request.executable};
  for (const QString &argument : request.arguments.mid(0, kMaxTitleArguments)) {
    if (!StoreIo::isCleanLine(argument, kMaxUmuPathChars)) {
      plan.notes.append(QStringLiteral("ignored an unprintable argument"));
      continue;
    }
    plan.arguments.append(argument);
  }
  plan.workingDirectory = executable.absolutePath();

  // The record's own environment first; the user's options may refine it.
  for (const QString &line :
       request.environment.mid(0, kMaxExtraEnvironmentEntries)) {
    if (!isValidEnvironmentAssignment(line)
        || isReservedUmuEnvironmentKey(keyOf(line))) {
      plan.notes.append(QStringLiteral("ignored environment line: %1")
                            .arg(line.left(64)));
      continue;
    }
    plan.environment.insert(keyOf(line), line.mid(keyOf(line).size() + 1));
  }
  applyLaunchOptions(options, tools, displays,
                     /*allowMangohudWrapper=*/false, &plan);
  for (const QString &line : options.extraEnvironment) {
    if (!isValidEnvironmentAssignment(line)
        || !isReservedUmuEnvironmentKey(keyOf(line))) {
      continue;
    }
    const QString key = keyOf(line);
    plan.environment.remove(key); // applyLaunchOptions inserted it
    plan.notes.append(
        kPlanSetKeys.contains(key)
            ? QStringLiteral("%1 is set by QindaLutris for this game; your "
                             "value was ignored").arg(key)
            : QStringLiteral("%1 cannot be used with umu launches; your value "
                             "was ignored").arg(key));
  }

  // Written last so nothing above can override them (see header guard).
  plan.unsetEnvironment = umuUnsetEnvironmentKeys();
  plan.unsetEnvironmentPrefixes = umuUnsetEnvironmentPrefixes();
  const QHash<QString, QString> umu = umuRunEnvironment(
      request.prefixPath, pin.build->path, request.umuId, request.umuStore);
  for (auto it = umu.cbegin(); it != umu.cend(); ++it) {
    plan.environment.insert(it.key(), it.value());
  }
  plan.ok = true;
  return plan;
}

QHash<QString, QString> umuRunEnvironment(const QString &prefixPath,
                                          const QString &buildPath,
                                          const QString &umuId,
                                          const QString &umuStore) {
  return {
      {QStringLiteral("WINEPREFIX"), prefixPath},
      {QStringLiteral("PROTONPATH"), buildPath},
      {QStringLiteral("GAMEID"), umuId.isEmpty() ? QStringLiteral("umu-0") : umuId},
      {QStringLiteral("STORE"), umuStore.isEmpty() ? QStringLiteral("none") : umuStore},
      {QStringLiteral("UMU_RUNTIME_UPDATE"), QStringLiteral("0")},
  };
}

UmuLaunchRequest umuRequestForTitle(const TitleRecord &title) {
  UmuLaunchRequest request;
  request.executable = title.executable;
  request.arguments = title.arguments;
  request.prefixPath = title.prefixPath;
  request.protonBuild = title.protonBuild;
  request.protonBuildVersion = title.protonBuildVersion;
  request.umuId = title.umuId;
  request.umuStore = title.umuStore;
  request.environment = title.environment;
  request.prefixMustExist = true;
  return request;
}

LaunchPlan planTitleLaunch(const TitleRecord &title,
                           const LaunchOptions &options,
                           const LaunchToolSet &tools,
                           const QVector<DisplayTarget> &displays) {
  if (title.kind == TitleKind::StoreGame && launchesThroughStoreClient(title.store)) {
    return planStoreGameLaunch(title, options, tools, displays);
  }
  return planUmuLaunch(umuRequestForTitle(title), options, tools, displays);
}

QStringList defaultUmuSearchPath(const QString &home,
                                 const QStringList &pathDirectories) {
  QStringList out{QStringLiteral("/usr/bin")};
  for (const QString &dir : pathDirectories) {
    if (!dir.isEmpty() && !out.contains(dir)) {
      out.append(dir);
    }
  }
  const QString user = home + QStringLiteral("/.local/bin");
  if (!home.isEmpty() && !out.contains(user)) {
    out.append(user);
  }
  return out;
}

QString discoverUmuRun(const QStringList &searchDirectories) {
  for (const QString &dir : searchDirectories) {
    if (dir.isEmpty()) {
      continue;
    }
    const QFileInfo candidate(QDir(dir).filePath(QStringLiteral("umu-run")));
    if (candidate.isFile() && candidate.isExecutable()) {
      return candidate.absoluteFilePath();
    }
  }
  return {};
}

} // namespace QindaQt::QindaLutris
