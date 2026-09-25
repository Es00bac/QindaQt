// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_manager.h"

#include "library_controller.h"
#include "network_downloader.h"
#include "preferences_store.h"
#include "process_runner.h"
#include "proton_catalog.h"
#include "proton_install_job.h"
#include "proton_pin.h"
#include "proton_removal.h"
#include "store_io.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kReleasesPerPage = 30;

QString statusId(CompatBuildStatus status) {
  switch (status) {
  case CompatBuildStatus::Tested:
    return QStringLiteral("tested");
  case CompatBuildStatus::KnownIssues:
    return QStringLiteral("known-issues");
  case CompatBuildStatus::Untested:
    break;
  }
  return QStringLiteral("untested");
}

} // namespace

ProtonManager::ProtonManager(LibraryController *library, const CompatDatabase *database,
                             QObject *parent)
    : QObject(parent), m_library(library), m_database(database),
      m_downloader(std::make_unique<NetworkDownloader>()),
      m_releaseDownloader(std::make_unique<NetworkDownloader>()),
      m_runner(std::make_unique<QProcessRunner>()),
      m_releaseFile(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
                    QStringLiteral("/qindalutris/ge-proton-releases.json")) {
  m_installJob = new ProtonInstallJob(defaultUserCompatToolsRoot(), m_downloader.get(),
                                      m_runner.get(), &m_probe, this);
  connect(m_installJob, &ProtonInstallJob::progress, this,
          [this](double fraction, const QString &text) {
            m_progress = fraction;
            m_stageText = text;
            Q_EMIT stateChanged();
          });
  connect(m_installJob, &ProtonInstallJob::finished, this, &ProtonManager::onInstallFinished);
  connect(m_releaseDownloader.get(), &Downloader::finished, this,
          &ProtonManager::onReleasesDownloaded);
  connect(m_library, &LibraryController::libraryChanged, this, &ProtonManager::buildsChanged);
}

ProtonManager::~ProtonManager() { delete m_installJob; }

void ProtonManager::initialize() {
  PreferencesStore::Error error = PreferencesStore::Error::None;
  m_userDefault = PreferencesStore(m_library->configRoot()).read(&error).defaultProtonBuild;
  applyEffectiveDefault();
}

void ProtonManager::applyEffectiveDefault() {
  // User's choice first, then the database's recommendation; the library's
  // chooseDefaultBuild falls back to the newest system build.
  QString preferred = m_userDefault;
  if (preferred.isEmpty() && m_database != nullptr && m_database->isLoaded()) {
    preferred = m_database->recommendedBuild();
  }
  m_library->setPreferredProtonBuild(preferred);
  Q_EMIT buildsChanged();
}

QString ProtonManager::defaultBuild() const {
  const auto chosen =
      chooseDefaultBuild(m_library->toolSet().protonBuilds, m_library->preferredProtonBuild());
  return chosen ? chosen->name : QString();
}

QSet<QString> ProtonManager::pinnedBuildNames() const {
  QSet<QString> names;
  for (const TitleRecord &title : m_library->titles()) {
    names.insert(title.protonBuild);
  }
  for (const WineEntryRecord &entry : m_library->wineRecords()) {
    if (!entry.protonPath.isEmpty()) {
      // A name, or a legacy absolute path of a build or of its `proton` script.
      const QFileInfo pin(entry.protonPath);
      names.insert(pin.fileName() == QLatin1String("proton") ? pin.dir().dirName()
                                                             : pin.fileName());
    }
  }
  return names;
}

QVariantList ProtonManager::builds() const {
  const QString defaultName = defaultBuild();
  QHash<QString, int> usage;
  for (const TitleRecord &title : m_library->titles()) {
    ++usage[title.protonBuild];
  }
  QVariantList rows;
  for (const ProtonBuild &build : m_library->toolSet().protonBuilds) {
    QVariantMap row;
    row.insert(QStringLiteral("name"), build.name);
    row.insert(QStringLiteral("displayName"), build.displayName);
    row.insert(QStringLiteral("version"), protonVersionLabel(build.versionText));
    row.insert(QStringLiteral("origin"), protonOriginId(build.origin));
    row.insert(QStringLiteral("pinnable"), build.pinnable);
    row.insert(QStringLiteral("removable"), build.removable);
    row.insert(QStringLiteral("label"), protonBuildStatusLabel(build));
    const bool known = m_database != nullptr && m_database->isLoaded();
    row.insert(QStringLiteral("status"),
               known ? statusId(m_database->buildStatus(build.name)) : QStringLiteral("untested"));
    row.insert(QStringLiteral("notes"), known ? m_database->buildNotes(build.name) : QString());
    row.insert(QStringLiteral("isDefault"), build.name == defaultName);
    row.insert(QStringLiteral("usedBy"), usage.value(build.name));
    rows.append(row);
  }
  return rows;
}

QVariantList ProtonManager::releases() const {
  QSet<QString> installed;
  for (const ProtonBuild &build : m_library->toolSet().protonBuilds) {
    installed.insert(build.name);
  }
  QVariantList rows;
  for (const GeProtonRelease &release : m_releases) {
    rows.append(QVariantMap{
        {QStringLiteral("toolName"), release.toolName},
        {QStringLiteral("tag"), release.tagName},
        {QStringLiteral("published"), release.publishedAt.date().toString(Qt::ISODate)},
        {QStringLiteral("sizeText"), plainSize(release.tarballBytes)},
        {QStringLiteral("installed"), installed.contains(release.toolName)},
        {QStringLiteral("status"), m_database != nullptr && m_database->isLoaded()
                                       ? statusId(m_database->buildStatus(release.toolName))
                                       : QStringLiteral("untested")},
    });
  }
  return rows;
}

void ProtonManager::finish(bool ok, const QString &message, const QString &details) {
  Q_UNUSED(ok)
  m_busy = false;
  m_resultMessage = message;
  m_details = details;
  Q_EMIT stateChanged();
}

void ProtonManager::checkForReleases() {
  if (m_busy) {
    return;
  }
  m_busy = true;
  m_fetchingReleases = true;
  m_progress = 0.0;
  m_stageText = QStringLiteral("Checking for GE-Proton releases…");
  m_resultMessage.clear();
  Q_EMIT stateChanged();
  QDir().mkpath(QFileInfo(m_releaseFile).absolutePath());
  m_releaseDownloader->start(geProtonReleasesApiUrl(kReleasesPerPage), m_releaseFile);
}

void ProtonManager::onReleasesDownloaded(bool ok, const QString &reason) {
  if (!m_fetchingReleases) {
    return;
  }
  m_fetchingReleases = false;
  if (!ok) {
    finish(false, QStringLiteral("Could not check for new Proton builds. %1").arg(reason), {});
    return;
  }
  StoreIo::ReadStatus status = StoreIo::ReadStatus::Ok;
  const QByteArray bytes =
      StoreIo::readBoundedFile(m_releaseFile, kMaxReleaseDocumentBytes, &status);
  const GeProtonReleaseList list = parseGeProtonReleases(bytes);
  if (status != StoreIo::ReadStatus::Ok || !list.ok) {
    finish(false, QStringLiteral("The list of Proton builds could not be read."), list.error);
    return;
  }
  m_releases.clear();
  for (const GeProtonRelease &release : list.releases) {
    if (!release.prerelease) {
      m_releases.append(release);
    }
  }
  Q_EMIT releasesChanged();
  finish(true, QStringLiteral("Found %1 GE-Proton releases.").arg(m_releases.size()), {});
}

void ProtonManager::installRelease(const QString &toolName) {
  if (m_busy) {
    return;
  }
  for (const GeProtonRelease &release : std::as_const(m_releases)) {
    if (release.toolName == toolName) {
      m_busy = true;
      m_progress = 0.0;
      m_stageText = QStringLiteral("Getting %1…").arg(release.tagName);
      m_resultMessage.clear();
      Q_EMIT stateChanged();
      m_installJob->start(release);
      return;
    }
  }
}

void ProtonManager::onInstallFinished(const ProtonJobResult &result) {
  finish(result.ok, result.message, m_installJob->detailsText());
  if (result.ok) {
    m_library->refresh();
    Q_EMIT releasesChanged();
  }
}

void ProtonManager::removeBuild(const QString &name) {
  if (m_busy) {
    return;
  }
  ProtonRemovalRequest request;
  request.userRoot = defaultUserCompatToolsRoot();
  request.buildName = name;
  request.pinnedBuildNames = pinnedBuildNames();
  const ProtonRemovalResult result = removeProtonBuild(request);
  finish(result.ok, result.message, {});
  if (result.ok) {
    if (m_userDefault == name) {
      setDefaultBuild({});
    }
    m_library->refresh();
    Q_EMIT releasesChanged();
  }
}

bool ProtonManager::setDefaultBuild(const QString &name) {
  if (!name.isEmpty()) {
    bool usable = false;
    for (const ProtonBuild &build : m_library->toolSet().protonBuilds) {
      usable = usable || (build.name == name && build.pinnable);
    }
    if (!usable) {
      return false;
    }
  }
  Preferences preferences;
  preferences.defaultProtonBuild = name;
  if (PreferencesStore(m_library->configRoot()).write(preferences) !=
      PreferencesStore::Error::None) {
    finish(false, QStringLiteral("Your default Proton build could not be saved."), {});
    return false;
  }
  m_userDefault = name;
  applyEffectiveDefault();
  return true;
}

void ProtonManager::cancel() {
  if (m_fetchingReleases) {
    m_releaseDownloader->cancel();
  } else if (m_installJob->isRunning()) {
    m_installJob->cancel();
  }
}

} // namespace QindaQt::QindaLutris
