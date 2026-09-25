// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_launcher.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace QindaQt::QindaLutris {

QProcessEnvironment launchEnvironment(const LaunchPlan &plan,
                                      const QProcessEnvironment &base) {
  QProcessEnvironment environment = base;
  for (const QString &key : plan.unsetEnvironment) {
    environment.remove(key);
  }
  if (!plan.unsetEnvironmentPrefixes.isEmpty()) {
    const QStringList keys = environment.keys();
    for (const QString &key : keys) {
      for (const QString &prefix : plan.unsetEnvironmentPrefixes) {
        if (!prefix.isEmpty() && key.startsWith(prefix)) {
          environment.remove(key);
          break;
        }
      }
    }
  }
  for (auto it = plan.environment.constBegin(); it != plan.environment.constEnd();
       ++it) {
    environment.insert(it.key(), it.value());
  }
  return environment;
}

LaunchOutcome QProcessGameLauncher::launch(const LaunchPlan &plan) {
  LaunchOutcome outcome;
  if (!plan.ok || plan.program.isEmpty()) {
    outcome.diagnostic = QStringLiteral("no launch plan");
    return outcome;
  }
  const QProcessEnvironment environment =
      launchEnvironment(plan, QProcessEnvironment::systemEnvironment());
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
