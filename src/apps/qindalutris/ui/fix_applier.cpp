// SPDX-License-Identifier: GPL-3.0-or-later
#include "fix_applier.h"

#include "process_runner.h"
#include "umu_installer_planner.h"

namespace QindaQt::QindaLutris {

FixApplier::FixApplier(std::function<LaunchToolSet()> tools, QObject *parent)
    : QObject(parent), m_tools(std::move(tools)),
      m_runner(new QProcessRunner(ProcessContainment::Auto, this)) {
  connect(m_runner, &ProcessRunner::finished, this, &FixApplier::onRunFinished);
}

FixApplier::~FixApplier() = default;

bool FixApplier::start(const InstallerPlanRequest &context, const QStringList &verbs,
                       QString *reason) {
  if (m_running) {
    *reason = QStringLiteral("Fixes are already being applied.");
    return false;
  }
  InstallerPlan plan = planUmuWinetricksRun(context, verbs, m_tools ? m_tools() : LaunchToolSet{});
  if (!plan.ok) {
    *reason = plan.reason;
    return false;
  }
  plan.spec.timeoutMs = kTimeoutMs;
  m_running = true;
  m_runner->start(plan.spec);
  return true;
}

void FixApplier::cancel() {
  if (m_running) {
    m_runner->cancel();
  }
}

void FixApplier::onRunFinished(const ProcessRunResult &result) {
  if (!m_running) {
    return;
  }
  m_running = false;
  const bool ok = result.started && !result.timedOut && !result.cancelled &&
                  !result.crashed && result.exitCode == 0;
  Q_EMIT finished(ok, describeProcessResult(result));
}

} // namespace QindaQt::QindaLutris
