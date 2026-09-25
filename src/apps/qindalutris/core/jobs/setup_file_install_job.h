// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "executable_candidates.h"
#include "install_preflight.h"
#include "installer_planning.h"
#include "job_log.h"

#include <QObject>

namespace QindaQt::QindaLutris {

struct SetupFileInstallRequest final {
  QString installerPath;   // local .exe or .msi the user chose
  QString title;           // what the user calls the game
  QString prefixPath;      // absolute, e.g. ~/Games/<slug>
  QString protonBuildName; // the pin
  QString protonBuildPath;
  QString umuId = QStringLiteral("umu-0");   // from the compat database when known
  QString umuStore = QStringLiteral("none");
  qint64 minimumFreeBytes = qint64(4) * 1024 * 1024 * 1024;
};

struct SetupFileInstallResult final {
  bool ok = false;
  bool cancelled = false;
  QString message; // ONE plain sentence
  QString note;
  QString title;
  QString prefixPath;
  QString protonBuildName;
  QString umuId;
  QString umuStore;
  // Ranked suggestions for the UI to confirm; empty on failure.
  QVector<ExecutableCandidate> candidates;
};

// AGENT-CONTRACT: the "any setup file" flow of ADR-0275 section 4. Stages:
// Preflight (installer is a readable .exe/.msi, pinned build, umu, Vulkan,
// free space) -> create the prefix -> snapshot executables -> run the
// installer through the injected planner + InstallerRunner -> snapshot again
// -> rank the new executables. Success means "at least one candidate"; the
// UI then asks the user to confirm which is the game. The installer file is
// the user's and is never deleted. Cancel stops the installer and leaves the
// prefix in place. Seams are borrowed and dedicated to this job while it
// runs. Threading: owner thread only; the two snapshots are bounded
// synchronous walks.
class SetupFileInstallJob final : public QObject {
  Q_OBJECT
public:
  SetupFileInstallJob(InstallerRunner *runner, const SystemProbe *probe,
                      InstallerPlanner planner, QObject *parent = nullptr);
  ~SetupFileInstallJob() override;

  void start(const SetupFileInstallRequest &request);
  void cancel();

  [[nodiscard]] bool isRunning() const { return m_running; }
  [[nodiscard]] QString detailsText() const { return m_log.text(); }

Q_SIGNALS:
  void progress(double fraction, const QString &stageText);
  void finished(const QindaQt::QindaLutris::SetupFileInstallResult &result);

private:
  void onInstallerFinished(const ProcessRunResult &result);
  void fail(const QString &plain, const QString &detail);
  void conclude(SetupFileInstallResult result);

  InstallerRunner *m_runner = nullptr;
  const SystemProbe *m_probe = nullptr;
  InstallerPlanner m_planner;
  SetupFileInstallRequest m_request;
  ExecutableSnapshot m_before;
  bool m_running = false;
  bool m_installing = false;
  JobLog m_log;
};

} // namespace QindaQt::QindaLutris
