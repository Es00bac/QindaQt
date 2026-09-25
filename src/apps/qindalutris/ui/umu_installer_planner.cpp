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

InstallerPlan planUmuInstallerRun(const InstallerPlanRequest &request,
                                  const LaunchToolSet &tools) {
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
  if (!installer.isAbsolute() || !installer.isFile()) {
    return refused(QStringLiteral("The installer file is missing."));
  }

  InstallerPlan plan;
  plan.spec.program = umuRun;
  plan.spec.arguments = request.windowsCommand;
  plan.spec.workingDirectory = installer.absolutePath();
  plan.spec.unsetEnvironment = umuUnsetEnvironmentKeys();
  plan.spec.unsetEnvironmentPrefixes = umuUnsetEnvironmentPrefixes();
  plan.spec.environment = umuRunEnvironment(request.prefixPath, build->path,
                                            request.umuId, request.umuStore);
  plan.ok = true;
  return plan;
}

InstallerPlanner makeUmuInstallerPlanner(std::function<LaunchToolSet()> tools) {
  return [tools = std::move(tools)](const InstallerPlanRequest &request) {
    return planUmuInstallerRun(request, tools ? tools() : LaunchToolSet{});
  };
}

} // namespace QindaQt::QindaLutris
