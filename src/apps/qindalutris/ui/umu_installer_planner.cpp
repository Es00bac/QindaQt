// SPDX-License-Identifier: GPL-3.0-or-later
#include "umu_installer_planner.h"

#include "proton_catalog.h"
#include "proton_pin.h"
#include "title_record.h"
#include "umu_launch.h"

#include <QDir>
#include <QFileInfo>

namespace QindaQt::QindaLutris {

namespace {

InstallerPlan refused(const QString &reason) {
  InstallerPlan plan;
  plan.reason = reason;
  return plan;
}

bool sameDirectory(const QString &a, const QString &b) {
  return !a.isEmpty() && QDir::cleanPath(a) == QDir::cleanPath(b);
}

} // namespace

// One umu run: installer (needsInstallerFile) or an in-prefix tool.
static InstallerPlan planRun(const InstallerPlanRequest &request, const LaunchToolSet &tools,
                      bool needsInstallerFile) {
  const QString umuRun =
      !request.umuRunBinary.isEmpty() ? request.umuRunBinary : tools.umuRunBinary;
  if (umuRun.isEmpty()) {
    return refused(QStringLiteral("umu is not installed. Install games-util/umu-launcher."));
  }
  const ProtonBuild *build = nullptr;
  for (const ProtonBuild &candidate : tools.protonBuilds) {
    if (candidate.name == request.protonBuildName &&
        sameDirectory(candidate.path, request.protonBuildPath)) {
      build = &candidate;
      break;
    }
  }
  // AGENT-GUARD: no fallback to another build of the same name or to the
  // default: the job named one exact directory, and only that one may run.
  if (build == nullptr || !protonBuildStillPresent(*build)) {
    return refused(protonNotInstalledReason(request.protonBuildName));
  }
  if (!build->pinnable) {
    return refused(QStringLiteral("%1 is updated by Steam and cannot be pinned. Choose "
                                  "another Proton build.")
                       .arg(build->displayName));
  }
  if (request.prefixPath.isEmpty() || !QDir::isAbsolutePath(request.prefixPath)) {
    return refused(QStringLiteral("The install folder is not usable."));
  }
  if (request.windowsCommand.isEmpty()) {
    return refused(QStringLiteral("There is nothing to run for this installer."));
  }
  const QFileInfo installer(request.installerPath);
  if (needsInstallerFile && (!installer.isAbsolute() || !installer.isFile())) {
    return refused(QStringLiteral("The installer file is missing."));
  }

  InstallerPlan plan;
  plan.spec.program = umuRun;
  plan.spec.arguments = request.windowsCommand;
  plan.spec.workingDirectory =
      needsInstallerFile ? installer.absolutePath() : request.prefixPath;
  plan.spec.unsetEnvironment = umuUnsetEnvironmentKeys();
  plan.spec.unsetEnvironmentPrefixes = umuUnsetEnvironmentPrefixes();
  plan.spec.environment = umuRunEnvironment(request.prefixPath, build->path,
                                            request.umuId, request.umuStore);
  plan.ok = true;
  return plan;
}

InstallerPlan planUmuInstallerRun(const InstallerPlanRequest &request,
                                  const LaunchToolSet &tools) {
  return planRun(request, tools, /*needsInstallerFile=*/true);
}

InstallerPlan planUmuWinetricksRun(const InstallerPlanRequest &context,
                                   const QStringList &verbs, const LaunchToolSet &tools) {
  if (verbs.isEmpty()) {
    return refused(QStringLiteral("There are no fixes to apply."));
  }
  for (const QString &verb : verbs) {
    // AGENT-GUARD: defence in depth over the database's verb allowlist: an
    // option-shaped word would change what winetricks does, not add a fix.
    if (verb.isEmpty() || verb.startsWith(QLatin1Char('-'))) {
      return refused(QStringLiteral("A fix from the compatibility database was not usable."));
    }
  }
  InstallerPlanRequest request = context;
  request.windowsCommand = QStringList{QStringLiteral("winetricks")} + verbs;
  return planRun(request, tools, /*needsInstallerFile=*/false);
}

InstallerPlanner makeUmuInstallerPlanner(std::function<LaunchToolSet()> tools) {
  return [tools = std::move(tools)](const InstallerPlanRequest &request) {
    return planUmuInstallerRun(request, tools ? tools() : LaunchToolSet{});
  };
}

} // namespace QindaQt::QindaLutris
