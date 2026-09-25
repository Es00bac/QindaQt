// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "install_preflight.h"
#include "installer_planning.h"
#include "job_log.h"
#include "store_recipes.h"

#include <QObject>

namespace QindaQt::QindaLutris {

class Downloader;

struct LauncherInstallRequest final {
  StoreRecipe recipe;        // usually *findStoreRecipe(id)
  QString title;             // empty: recipe.displayName
  QString prefixPath;        // absolute, e.g. ~/Games/<defaultPrefixDirName>
  QString protonBuildName;   // the build this launcher is pinned to
  QString protonBuildPath;   // its absolute directory
  QString downloadDirectory; // absolute; the installer is deleted afterwards
};

// AGENT-CONTRACT (with TitleRecord): everything the titles-v1.json record of
// an installed launcher needs. executableUnixPath is the detected launcher
// inside prefixPath; protonBuildName is the pin, which never floats.
struct InstalledLauncher final {
  QString recipeId;
  QString title;
  QString prefixPath;
  QString executableUnixPath;
  QString protonBuildName;
  QString umuId;
  QString umuStore;

  friend bool operator==(const InstalledLauncher &, const InstalledLauncher &) = default;
};

struct LauncherInstallResult final {
  bool ok = false;
  bool cancelled = false;
  QString message; // ONE plain sentence
  QString note;    // optional second line on success (odd exit code, adopted)
  InstalledLauncher launcher;
};

// ~/Games and $XDG_CACHE_HOME/qindalutris/installers.
[[nodiscard]] QString defaultGamesDirectory();
[[nodiscard]] QString defaultInstallerDownloadDirectory();

// AGENT-CONTRACT: the one-click store-launcher install of ADR-0275 section 4.
// Stages: Preflight (recipe valid, pinned build, umu, Vulkan, free space) ->
// adopt when the prefix already holds the launcher (no download) ->
// Download the vendor installer (allowlisted HTTPS only) -> Run it through
// the injected planner + InstallerRunner -> Detect the launcher (first
// existing recipe candidate) -> finished(). Vendor installers often return
// odd exit codes, so presence of the launcher decides success; the code only
// shapes the message. Cancel stops the running stage; a partly-written prefix
// is left in place (it may already hold the user's data) and the downloaded
// installer is removed only once its process tree is confirmed gone.
// finished() is always queued, never emitted from inside start()/cancel(),
// and isRunning() stays true until it is delivered. Seams are borrowed and
// dedicated to this job while it runs. Threading: owner thread only.
class LauncherInstallJob final : public QObject {
  Q_OBJECT
public:
  enum class Stage { Idle, Preflight, Downloading, Installing, Stopping, Concluding };

  LauncherInstallJob(Downloader *downloader, InstallerRunner *runner, const SystemProbe *probe,
                     InstallerPlanner planner, QObject *parent = nullptr);
  ~LauncherInstallJob() override;

  void start(const LauncherInstallRequest &request);
  // Stops the running stage; finished(cancelled) follows asynchronously --
  // after the installer's whole process tree is gone when it was running.
  void cancel();

  [[nodiscard]] Stage stage() const { return m_stage; }
  [[nodiscard]] bool isRunning() const { return m_stage != Stage::Idle; }
  [[nodiscard]] QString detailsText() const { return m_log.text(); }

Q_SIGNALS:
  void progress(double fraction, const QString &stageText);
  void finished(const QindaQt::QindaLutris::LauncherInstallResult &result);

private:
  void onDownloadProgress(qint64 received, qint64 total);
  void onDownloadFinished(bool ok, const QString &reason);
  void onInstallerFinished(const ProcessRunResult &result);
  void succeed(const QString &executable, const QString &note);
  void fail(const QString &plain, const QString &detail);
  void conclude(LauncherInstallResult result);
  void concludeCancelled();

  Downloader *m_downloader = nullptr;
  InstallerRunner *m_runner = nullptr;
  const SystemProbe *m_probe = nullptr;
  InstallerPlanner m_planner;
  LauncherInstallRequest m_request;
  QString m_name;
  QString m_umuRun;
  QString m_installerFile;
  Stage m_stage = Stage::Idle;
  bool m_keepInstaller = false;
  quint64 m_generation = 0;
  JobLog m_log;
};

} // namespace QindaQt::QindaLutris
