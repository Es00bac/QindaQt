// SPDX-License-Identifier: GPL-3.0-or-later
#include "scoped_game_launcher.h"

#include <QFileInfo>
#include <QProcess>

namespace QindaQt::QindaLutris {

ScopedGameLauncher::ScopedGameLauncher() : m_tools(detectUserScopeTools()) {}

ScopedGameLauncher::ScopedGameLauncher(UserScopeTools tools) : m_tools(std::move(tools)) {}

bool ScopedGameLauncher::shouldScope(const LaunchPlan &plan) {
  const QString base = QFileInfo(plan.program).fileName();
  return base != QLatin1String("steam") && base != QLatin1String("lutris");
}

LaunchOutcome ScopedGameLauncher::launch(const LaunchPlan &plan) {
  m_lastUnit.clear();
  m_lastPid = 0;
  if (!plan.ok || plan.program.isEmpty()) {
    return {false, QStringLiteral("nothing to start")};
  }
  QProcess process;
  process.setProcessEnvironment(
      launchEnvironment(plan, QProcessEnvironment::systemEnvironment()));
  if (!plan.workingDirectory.isEmpty()) {
    process.setWorkingDirectory(plan.workingDirectory);
  }
  QString unit;
  if (m_tools.available() && shouldScope(plan)) {
    unit = newScopeUnitName(QStringLiteral("qindalutris-game"));
    process.setProgram(m_tools.systemdRun);
    process.setArguments(systemdRunScopeArguments(unit, plan.program, plan.arguments));
  } else {
    process.setProgram(plan.program);
    process.setArguments(plan.arguments);
  }
  qint64 pid = 0;
  if (!process.startDetached(&pid)) {
    return {false, process.errorString()};
  }
  m_lastUnit = unit;
  m_lastPid = pid;
  return {true, {}};
}

} // namespace QindaQt::QindaLutris
