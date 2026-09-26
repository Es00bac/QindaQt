// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "installer_planning.h"
#include "launch_planner.h"

#include <QObject>
#include <QStringList>

#include <functional>

namespace QindaQt::QindaLutris {

class QProcessRunner;
struct ProcessRunResult;

// AGENT-CONTRACT: applies a new title's one-time database fixes (ADR-0275
// section 3) -- `umu-run winetricks <verbs>` in its prefix, planned by
// planUmuWinetricksRun (same umu rules and build checks as installers), run
// by the jobs package's scoped, cancellable QProcessRunner. One run at a
// time; `finished` reports whether every verb applied, plus a details log.
class FixApplier final : public QObject {
  Q_OBJECT
public:
  explicit FixApplier(std::function<LaunchToolSet()> tools, QObject *parent = nullptr);
  ~FixApplier() override;

  // False (and *reason set) when nothing started: the plan was refused.
  bool start(const InstallerPlanRequest &context, const QStringList &verbs, QString *reason);
  void cancel();
  [[nodiscard]] bool isRunning() const { return m_running; }

  // Winetricks downloads components (e.g. the Visual C++ runtime), so the
  // bound is generous but finite.
  static constexpr int kTimeoutMs = 30 * 60 * 1000;

Q_SIGNALS:
  void finished(bool ok, const QString &details);

private:
  void onRunFinished(const ProcessRunResult &result);

  std::function<LaunchToolSet()> m_tools;
  QProcessRunner *m_runner = nullptr; // owned child
  bool m_running = false;
};

} // namespace QindaQt::QindaLutris
