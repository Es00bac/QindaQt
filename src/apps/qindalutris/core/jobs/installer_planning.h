// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "process_runner.h"
#include "store_recipes.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the seam between the install jobs (this package) and the
// umu launch-plan builder (the Proton catalog package, ADR-0275 section 1).
// The job gathers facts; the injected planner turns them into ONE argv plan
// that runs `umu-run` with PROTONPATH = protonBuildPath (absolute, never a
// floating alias), UMU_RUNTIME_UPDATE=0, GAMEID = umuId, STORE = umuStore
// and WINEPREFIX = prefixPath. The job never builds umu environment itself,
// so there is exactly one place that knows umu's rules.
//
// Wiring: the composition root adapts the umu plan builder to
// `InstallerPlanner`; tests inject a fake returning a recording spec.
using InstallerRunSpec = ProcessRunSpec;
using InstallerRunner = ProcessRunner;

struct InstallerPlanRequest final {
  QString umuRunBinary;    // resolved by preflight
  QString protonBuildName; // e.g. "GE-Proton11-6-x86_64"
  QString protonBuildPath; // absolute directory of that build
  QString prefixPath;      // WINEPREFIX
  QString umuId;           // GAMEID
  QString umuStore;        // STORE
  QString installerPath;   // local unix path of the downloaded/chosen installer
  InstallerKind installerKind = InstallerKind::Exe;
  // What runs inside the prefix, i.e. the argv after `umu-run`:
  // installerCommand(kind, installerPath, arguments).
  QStringList windowsCommand;

  friend bool operator==(const InstallerPlanRequest &, const InstallerPlanRequest &) = default;
};

struct InstallerPlan final {
  bool ok = false;
  QString reason; // plain sentence when !ok
  InstallerRunSpec spec;
};

using InstallerPlanner = std::function<InstallerPlan(const InstallerPlanRequest &)>;

// AGENT-NOTE: installers are interactive (the user may sign in, pick a
// folder, wait for a web installer), so the bound is generous. The job
// applies it when the planner leaves spec.timeoutMs unset.
inline constexpr int kInstallerTimeoutMs = 2 * 60 * 60 * 1000;

// Calls the planner and normalizes its answer: a missing planner, or a plan
// without a program, is refused with a plain reason; an unset timeout gets
// kInstallerTimeoutMs. Shared by both install jobs.
[[nodiscard]] InstallerPlan planInstallerRun(const InstallerPlanner &planner,
                                             const InstallerPlanRequest &request);

// One details-log line for a finished installer (exit, stop reason, and
// the first lines of its error output).
[[nodiscard]] QString describeProcessResult(const ProcessRunResult &result);

} // namespace QindaQt::QindaLutris
