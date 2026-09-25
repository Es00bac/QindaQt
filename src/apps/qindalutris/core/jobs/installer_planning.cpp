// SPDX-License-Identifier: GPL-3.0-or-later
#include "installer_planning.h"

namespace QindaQt::QindaLutris {

InstallerPlan planInstallerRun(const InstallerPlanner &planner,
                               const InstallerPlanRequest &request) {
  InstallerPlan plan;
  if (!planner) {
    plan.reason = QStringLiteral("QindaLutris cannot run installers yet on this system.");
    return plan;
  }
  plan = planner(request);
  if (plan.ok && plan.spec.program.isEmpty()) {
    plan.ok = false;
  }
  if (!plan.ok && plan.reason.isEmpty()) {
    plan.reason = QStringLiteral("The installer could not be prepared.");
  }
  if (plan.ok && plan.spec.timeoutMs <= 0) {
    plan.spec.timeoutMs = kInstallerTimeoutMs;
  }
  return plan;
}

QString describeProcessResult(const ProcessRunResult &result) {
  QString line = QStringLiteral("Installer result: started=%1 exit=%2 crashed=%3 timedOut=%4")
                     .arg(result.started ? QStringLiteral("yes") : QStringLiteral("no"))
                     .arg(result.exitCode)
                     .arg(result.crashed ? QStringLiteral("yes") : QStringLiteral("no"),
                          result.timedOut ? QStringLiteral("yes") : QStringLiteral("no"));
  if (!result.error.isEmpty()) {
    line += QStringLiteral("\nInstaller error: ") + result.error;
  }
  const QStringList errors =
      QString::fromUtf8(result.standardError).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  if (!errors.isEmpty()) {
    line += QStringLiteral("\nInstaller output (first lines):\n") + errors.mid(0, 40).join(QLatin1Char('\n'));
  }
  return line;
}

} // namespace QindaQt::QindaLutris
