// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_launcher.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace QindaQt::QindaLutris {

LaunchOutcome QProcessGameLauncher::launch(const LaunchPlan &plan) {
  LaunchOutcome outcome;
  if (!plan.ok || plan.program.isEmpty()) {
    outcome.diagnostic = QStringLiteral("no launch plan");
    return outcome;
  }
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  for (auto it = plan.environment.constBegin(); it != plan.environment.constEnd();
       ++it) {
    environment.insert(it.key(), it.value());
  }
  QProcess process;
  process.setProgram(plan.program);
  process.setArguments(plan.arguments);
  process.setProcessEnvironment(environment);
  if (!plan.workingDirectory.isEmpty()) {
    process.setWorkingDirectory(plan.workingDirectory);
  }
  // startDetached: a game outlives the library window, and must never be
  // reaped with it.
  outcome.ok = process.startDetached();
  if (!outcome.ok) {
    outcome.diagnostic = process.errorString();
  }
  return outcome;
}

} // namespace QindaQt::QindaLutris
