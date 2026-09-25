// SPDX-License-Identifier: GPL-3.0-or-later
#include "install_controller.h"

#include "launcher_install_job.h"
#include "library_controller.h"
#include "network_downloader.h"
#include "prefix_paths.h"
#include "process_runner.h"
#include "store_recipes.h"
#include "umu_installer_planner.h"

#include <QClipboard>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QVariantMap>

namespace QindaQt::QindaLutris {

namespace {

QString today() { return QDate::currentDate().toString(Qt::ISODate); }

QString slugFor(const QString &titleId) {
  return titleId.section(QLatin1Char('/'), 1); // "title/<slug>"
}

} // namespace

InstallController::InstallController(LibraryController *library, const CompatDatabase *database,
                                     QObject *parent)
    : QObject(parent), m_library(library), m_database(database),
      m_downloader(std::make_unique<NetworkDownloader>()),
      m_runner(std::make_unique<QProcessRunner>()) {
  const auto tools = [library] { return library->toolSet(); };
  m_launcherJob = new LauncherInstallJob(m_downloader.get(), m_runner.get(), &m_probe,
                                         makeUmuInstallerPlanner(tools), this);
  m_setupJob =
      new SetupFileInstallJob(m_runner.get(), &m_probe, makeUmuInstallerPlanner(tools), this);
  const auto onProgress = [this](double fraction, const QString &text) {
    m_progress = fraction;
    m_stageText = text;
    Q_EMIT stateChanged();
  };
  connect(m_launcherJob, &LauncherInstallJob::progress, this, onProgress);
  connect(m_setupJob, &SetupFileInstallJob::progress, this, onProgress);
  connect(m_launcherJob, &LauncherInstallJob::finished, this,
          &InstallController::onLauncherFinished);
  connect(m_setupJob, &SetupFileInstallJob::finished, this, &InstallController::onSetupFinished);
  connect(m_library, &LibraryController::libraryChanged, this,
          &InstallController::storesChanged);
}

// AGENT-NOTE: the jobs are QObject children and are destroyed (stopping any
// running process tree) before the runner and downloader they borrow.
InstallController::~InstallController() {
  delete m_launcherJob;
  delete m_setupJob;
}

QVariantList InstallController::stores() const {
  QVariantList rows;
  for (const StoreRecipe &recipe : storeRecipes()) {
    QVariantMap row;
    row.insert(QStringLiteral("id"), recipe.id);
    row.insert(QStringLiteral("name"), recipe.displayName);
    row.insert(QStringLiteral("notes"), recipe.plainNotes);
    QString installedId;
    for (const TitleRecord &title : m_library->titles()) {
      if (title.kind == TitleKind::StoreLauncher && title.storeGameId == recipe.id) {
        installedId = title.id;
      }
    }
    row.insert(QStringLiteral("installed"), !installedId.isEmpty());
    row.insert(QStringLiteral("titleId"), installedId);
    const QString prefix =
        QDir(defaultGamesDirectory()).filePath(recipe.defaultPrefixDirName);
    row.insert(QStringLiteral("adoptable"),
               installedId.isEmpty() &&
                   !firstExistingCandidate(prefix, recipe.launcherExecutableCandidates).isEmpty());
    row.insert(QStringLiteral("prefixPath"), prefix);
    rows.append(row);
  }
  return rows;
}

void InstallController::begin(const QString &stageText) {
  m_busy = true;
  m_progress = 0.0;
  m_stageText = stageText;
  m_resultMessage.clear();
  m_resultNote.clear();
  m_details.clear();
  m_candidateRows.clear();
  Q_EMIT stateChanged();
}

void InstallController::finish(bool ok, const QString &message, const QString &note,
                               const QString &details) {
  m_busy = false;
  m_lastSucceeded = ok;
  m_resultMessage = message;
  m_resultNote = note;
  m_details = details;
  m_progress = ok ? 1.0 : m_progress;
  Q_EMIT stateChanged();
}

QStringList InstallController::takenTitleIds() const {
  QStringList ids;
  for (const TitleRecord &title : m_library->titles()) {
    ids.append(title.id);
  }
  return ids;
}

void InstallController::installStore(const QString &recipeId) {
  if (m_busy) {
    return;
  }
  const auto recipe = findStoreRecipe(recipeId);
  if (!recipe) {
    finish(false, QStringLiteral("That store is not available."), {}, {});
    return;
  }
  const auto advice = adviceForRecipe(m_database, *recipe);
  const auto build = chooseBuildForNewTitle(m_library->toolSet().protonBuilds,
                                            m_library->preferredProtonBuild(), m_database, advice);
  if (!build) {
    finish(false,
           QStringLiteral("No suitable Proton build is installed. Install "
                          "app-emulation/ge-proton-bin or add one in the Proton manager."),
           {}, {});
    return;
  }
  m_pendingRecipe = recipe->id;
  begin(QStringLiteral("Getting %1 ready…").arg(recipe->displayName));
  LauncherInstallRequest request;
  request.recipe = *recipe;
  request.prefixPath = QDir(defaultGamesDirectory()).filePath(recipe->defaultPrefixDirName);
  request.protonBuildName = build->name;
  request.protonBuildPath = build->path;
  request.downloadDirectory = defaultInstallerDownloadDirectory();
  m_launcherJob->start(request);
}

void InstallController::onLauncherFinished(const LauncherInstallResult &result) {
  const QString details = m_launcherJob->detailsText();
  const auto recipe = findStoreRecipe(m_pendingRecipe);
  if (!result.ok || !recipe) {
    finish(false, result.message, {}, details);
    return;
  }
  NewTitle facts;
  facts.title = result.launcher.title;
  facts.kind = TitleKind::StoreLauncher;
  facts.store = gameStoreForRecipe(recipe->id);
  facts.storeGameId = recipe->id;
  facts.prefixPath = result.launcher.prefixPath;
  facts.executable = result.launcher.executableUnixPath;
  facts.umuId = result.launcher.umuId;
  facts.umuStore = result.launcher.umuStore;
  QString buildVersion;
  for (const ProtonBuild &build : m_library->toolSet().protonBuilds) {
    if (build.name == result.launcher.protonBuildName && build.pinnable &&
        (buildVersion.isEmpty() || build.origin == ProtonBuild::Origin::System)) {
      buildVersion = build.versionText;
    }
  }
  QString error;
  if (!registerTitle(facts, result.launcher.protonBuildName, buildVersion, &error)) {
    finish(false, error, {}, details);
    return;
  }
  finish(true, result.message, result.note, details);
}

bool InstallController::registerTitle(const NewTitle &facts, const QString &buildName,
                                      const QString &buildVersion, QString *error) {
  ProtonBuild build;
  build.name = buildName;
  build.versionText = buildVersion;
  const auto advice = facts.kind == TitleKind::StoreLauncher && findStoreRecipe(facts.storeGameId)
                          ? adviceForRecipe(m_database, *findStoreRecipe(facts.storeGameId))
                          : adviceForGame(m_database, facts.title, facts.executable);
  QString why;
  const auto record = makeTitleRecord(facts, build, advice, takenTitleIds(), today(), &why);
  if (!record) {
    *error = QStringLiteral("%1 was installed, but could not be added to your library.")
                 .arg(facts.title);
    m_details += QStringLiteral("\nrecord refused: %1").arg(why);
    return false;
  }
  if (!appendTitle(m_library->configRoot(), *record, error)) {
    return false;
  }
  m_library->refresh();
  Q_EMIT storesChanged();
  return true;
}

void InstallController::installSetupFile(const QString &title, const QString &installerPath) {
  if (m_busy) {
    return;
  }
  const QString name = title.trimmed();
  const QFileInfo installer(installerPath);
  if (name.isEmpty() || !installer.isFile()) {
    finish(false, QStringLiteral("Choose a setup file and give the game a name."), {}, {});
    return;
  }
  const auto advice = adviceForGame(m_database, name, installer.fileName());
  const auto build = chooseBuildForNewTitle(m_library->toolSet().protonBuilds,
                                            m_library->preferredProtonBuild(), m_database, advice);
  if (!build) {
    finish(false,
           QStringLiteral("No suitable Proton build is installed. Install "
                          "app-emulation/ge-proton-bin or add one in the Proton manager."),
           {}, {});
    return;
  }
  begin(QStringLiteral("Installing %1…").arg(name));
  SetupFileInstallRequest request;
  request.installerPath = installer.absoluteFilePath();
  request.title = name;
  request.prefixPath =
      QDir(defaultGamesDirectory()).filePath(slugFor(makeTitleId(name, takenTitleIds())));
  request.protonBuildName = build->name;
  request.protonBuildPath = build->path;
  if (advice) {
    if (!advice->game.keys.umuId.isEmpty()) {
      request.umuId = advice->game.keys.umuId;
    }
    if (!advice->game.umuStore.isEmpty()) {
      request.umuStore = advice->game.umuStore;
    }
  }
  m_setupJob->start(request);
}

void InstallController::onSetupFinished(const SetupFileInstallResult &result) {
  m_setupResult = result;
  m_candidateRows.clear();
  for (const ExecutableCandidate &candidate : result.candidates) {
    m_candidateRows.append(QVariantMap{
        {QStringLiteral("path"), candidate.unixPath},
        {QStringLiteral("name"), QFileInfo(candidate.unixPath).fileName()},
        {QStringLiteral("sizeBytes"), candidate.sizeBytes},
    });
  }
  QString message = result.message;
  if (result.ok && m_candidateRows.isEmpty()) {
    message = QStringLiteral("The installer finished, but no new program was found. Choose "
                             "the game's program yourself with “Add Windows game”.");
  }
  finish(result.ok, message, result.note, m_setupJob->detailsText());
}

bool InstallController::confirmSetupCandidate(const QString &executablePath) {
  bool known = false;
  for (const ExecutableCandidate &candidate : m_setupResult.candidates) {
    known = known || candidate.unixPath == executablePath;
  }
  if (!m_setupResult.ok || !known) {
    return false;
  }
  NewTitle facts;
  facts.title = m_setupResult.title;
  facts.kind = TitleKind::SetupInstalled;
  facts.prefixPath = m_setupResult.prefixPath;
  facts.executable = executablePath;
  facts.umuId = m_setupResult.umuId;
  facts.umuStore = m_setupResult.umuStore;
  QString version;
  for (const ProtonBuild &build : m_library->toolSet().protonBuilds) {
    if (build.name == m_setupResult.protonBuildName && build.pinnable) {
      version = build.versionText;
    }
  }
  QString error;
  const bool ok = registerTitle(facts, m_setupResult.protonBuildName, version, &error);
  finish(ok, ok ? QStringLiteral("%1 is ready to play.").arg(facts.title) : error, {},
         m_details);
  if (ok) {
    m_setupResult = {};
  }
  return ok;
}

bool InstallController::adoptExistingLauncher(const QString &recipeId, const QString &prefixPath) {
  const auto recipe = findStoreRecipe(recipeId);
  const QString prefix = QDir::cleanPath(prefixPath);
  const QString executable =
      recipe ? firstExistingCandidate(prefix, recipe->launcherExecutableCandidates) : QString();
  if (!recipe || executable.isEmpty()) {
    finish(false, QStringLiteral("No installed launcher was found in that folder."), {}, {});
    return false;
  }
  const auto &builds = m_library->toolSet().protonBuilds;
  auto build = buildMatchingPrefix(prefix, builds);
  if (!build) {
    build = chooseBuildForNewTitle(builds, m_library->preferredProtonBuild(), m_database,
                                   adviceForRecipe(m_database, *recipe));
  }
  if (!build) {
    finish(false, QStringLiteral("No suitable Proton build is installed."), {}, {});
    return false;
  }
  NewTitle facts;
  facts.title = recipe->displayName;
  facts.kind = TitleKind::StoreLauncher;
  facts.store = gameStoreForRecipe(recipe->id);
  facts.storeGameId = recipe->id;
  facts.prefixPath = prefix;
  facts.executable = executable;
  facts.umuId = recipe->umuId;
  facts.umuStore = recipe->umuStore;
  QString error;
  const bool ok = registerTitle(facts, build->name, build->versionText, &error);
  finish(ok,
         ok ? QStringLiteral("%1 was added to your library, pinned to %2.")
                  .arg(recipe->displayName, build->displayName)
            : error,
         {}, {});
  return ok;
}

void InstallController::copyText(const QString &text) const {
  if (QClipboard *clipboard = QGuiApplication::clipboard()) {
    clipboard->setText(text);
  }
}

void InstallController::cancel() {
  if (m_launcherJob->isRunning()) {
    m_launcherJob->cancel();
  }
  if (m_setupJob->isRunning()) {
    m_setupJob->cancel();
  }
  m_stageText = QStringLiteral("Stopping…");
  Q_EMIT stateChanged();
}

} // namespace QindaQt::QindaLutris
