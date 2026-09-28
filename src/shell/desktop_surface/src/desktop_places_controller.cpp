// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_places_controller.h"

#include "public/desktop_file_boundary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <sys/stat.h>

#include <utility>

namespace QindaQt::Shell::DesktopSurface {

namespace {

struct PlaceSpec final {
  const char *id;
  const char *label;
  const char *iconName;
  const char *settingKey;
  bool shownByDefault;
};

// Desktop order: the machine, the home folder and its standard folders, then
// the Trash, the order other desktops use for their standard icons.
constexpr PlaceSpec Places[] = {
    {"computer", "Computer", "computer", "showComputerIcon", false},
    {"home", "Home", "user-home", "showHomeIcon", true},
    {"documents", "Documents", "folder-documents", "showDocumentsIcon", false},
    {"downloads", "Downloads", "folder-download", "showDownloadsIcon", false},
    {"pictures", "Pictures", "folder-pictures", "showPicturesIcon", false},
    {"videos", "Videos", "folder-videos", "showVideosIcon", false},
    {"music", "Music", "folder-music", "showMusicIcon", false},
    {"trash", "Trash", "user-trash", "showTrashIcon", true},
};

const PlaceSpec *spec(const QString &placeId) {
  for (const auto &place : Places) {
    if (placeId == QLatin1String(place.id)) {
      return &place;
    }
  }
  return nullptr;
}

QString rowIdFor(const QString &placeId) { return QStringLiteral("place:") + placeId; }

QString placeIdOf(const QString &rowId) {
  return rowId.startsWith(QLatin1String("place:")) ? rowId.mid(6) : QString();
}

bool directoryHasEntries(const QString &path) {
  const QDir directory(path);
  return directory.exists()
      && !directory.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)
              .isEmpty();
}

std::optional<QindaQt::Apps::FileManager::Desktop::ListedIdentity> identityOf(const QString &path) {
  struct stat info {};
  if (::lstat(QFile::encodeName(path).constData(), &info) != 0) {
    return std::nullopt;
  }
  return QindaQt::Apps::FileManager::Desktop::ListedIdentity{
      static_cast<quint64>(info.st_dev), static_cast<quint64>(info.st_ino)};
}

} // namespace

DesktopPlaceLocations DesktopPlaceLocations::standard() {
  const auto location = [](QStandardPaths::StandardLocation which) {
    return QStandardPaths::writableLocation(which);
  };
  const QString dataHome = location(QStandardPaths::GenericDataLocation);
  return {
      .home = QDir::homePath(),
      .documents = location(QStandardPaths::DocumentsLocation),
      .downloads = location(QStandardPaths::DownloadLocation),
      .pictures = location(QStandardPaths::PicturesLocation),
      .videos = location(QStandardPaths::MoviesLocation),
      .music = location(QStandardPaths::MusicLocation),
      .trashFiles = dataHome.isEmpty() ? QString()
                                       : QDir(dataHome).filePath(QStringLiteral("Trash/files")),
      .computer = QStringLiteral("/"),
  };
}

DesktopPlacesController::DesktopPlacesController(QObject *parent)
    : DesktopPlacesController(DesktopPlaceLocations::standard(), std::nullopt, parent) {}

DesktopPlacesController::DesktopPlacesController(DesktopPlaceLocations locations,
                                                 std::optional<QStringList> fileManagerPrograms,
                                                 QObject *parent)
    : QObject(parent), m_locations(std::move(locations)),
      m_fileManagerPrograms(std::move(fileManagerPrograms)) {
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
          &DesktopPlacesController::refresh);
  refresh();
}

DesktopPlacesController::~DesktopPlacesController() = default;

QStringList DesktopPlacesController::placeIds() {
  QStringList ids;
  for (const auto &place : Places) {
    ids.append(QString::fromLatin1(place.id));
  }
  return ids;
}

QString DesktopPlacesController::settingKey(const QString &placeId) {
  const auto *place = spec(placeId);
  return place ? QString::fromLatin1(place->settingKey) : QString();
}

bool DesktopPlacesController::shownByDefault(const QString &placeId) {
  const auto *place = spec(placeId);
  return place != nullptr && place->shownByDefault;
}

void DesktopPlacesController::setSettings(const QVariantMap &settings) {
  if (settings == m_settings) {
    return;
  }
  m_settings = settings;
  Q_EMIT settingsChanged();
  refresh();
}

QString DesktopPlacesController::locationFor(const QString &placeId) const {
  if (placeId == QLatin1String("computer")) return m_locations.computer;
  if (placeId == QLatin1String("home")) return m_locations.home;
  if (placeId == QLatin1String("documents")) return m_locations.documents;
  if (placeId == QLatin1String("downloads")) return m_locations.downloads;
  if (placeId == QLatin1String("pictures")) return m_locations.pictures;
  if (placeId == QLatin1String("videos")) return m_locations.videos;
  if (placeId == QLatin1String("music")) return m_locations.music;
  if (placeId == QLatin1String("trash")) return m_locations.trashFiles;
  return {};
}

bool DesktopPlacesController::shown(const QString &placeId) const {
  const QVariant value = m_settings.value(settingKey(placeId));
  // A missing or malformed value takes the manifest default, like every other
  // desktop-icons setting.
  return value.typeId() == QMetaType::Bool ? value.toBool() : shownByDefault(placeId);
}

void DesktopPlacesController::refresh() {
  QVariantList rows;
  bool trashFull = false;
  const QString home = QDir::cleanPath(m_locations.home);
  for (const auto &place : Places) {
    const QString placeId = QString::fromLatin1(place.id);
    const QString path = locationFor(placeId);
    if (!shown(placeId) || path.isEmpty()) {
      continue;
    }
    const bool trash = placeId == QLatin1String("trash");
    if (!trash) {
      // AGENT-GUARD: an unset XDG user directory resolves to the home folder
      // itself; a second "Home" labelled Documents would mislead.
      if (!QFileInfo(path).isDir()
          || (placeId != QLatin1String("home") && QDir::cleanPath(path) == home)) {
        continue;
      }
    }
    QString label = QString::fromUtf8(place.label);
    QString iconName = QString::fromLatin1(place.iconName);
    QString accessibleName = QStringLiteral("%1, %2").arg(label, path);
    if (trash) {
      trashFull = directoryHasEntries(path);
      iconName = trashFull ? QStringLiteral("user-trash-full") : QStringLiteral("user-trash");
      accessibleName = trashFull ? QStringLiteral("Trash, contains items")
                                 : QStringLiteral("Trash, empty");
    }
    rows.append(QVariantMap{
        {QStringLiteral("id"), rowIdFor(placeId)},
        {QStringLiteral("placeId"), placeId},
        {QStringLiteral("label"), label},
        {QStringLiteral("path"), path},
        {QStringLiteral("iconName"), iconName},
        {QStringLiteral("accessibleName"), accessibleName},
        {QStringLiteral("isDirectory"), true},
        {QStringLiteral("isPlace"), true},
        {QStringLiteral("layoutKey"), rowIdFor(placeId)},
    });
  }
  watchTrash();
  if (rows != m_rows || trashFull != m_trashFull) {
    m_rows = std::move(rows);
    m_trashFull = trashFull;
    Q_EMIT rowsChanged();
  }
}

void DesktopPlacesController::watchTrash() {
  // Follow the Trash so its icon fills and empties with it. Only existing
  // directories are watched (addPath warns about a missing one, and the
  // surface's offscreen rows treat warnings as fatal); the Trash folder that
  // appears on first use is picked up through its parent.
  if (m_locations.trashFiles.isEmpty()) {
    return;
  }
  // The files folder itself, plus the nearest existing folder above it, so a
  // Trash (or data home) created on first use is noticed too.
  const QString files = QDir::cleanPath(m_locations.trashFiles);
  QStringList wanted;
  if (QFileInfo(files).isDir()) {
    wanted.append(files);
  }
  for (QString parent = QFileInfo(files).absolutePath(); !parent.isEmpty();) {
    if (QFileInfo(parent).isDir()) {
      wanted.append(parent);
      break;
    }
    const QString next = QFileInfo(parent).absolutePath();
    parent = next == parent ? QString() : next;
  }
  for (const QString &path : std::as_const(wanted)) {
    if (!m_watcher.directories().contains(path)) {
      m_watcher.addPath(path);
    }
  }
}

bool DesktopPlacesController::isPlace(const QString &rowId) const {
  for (const QVariant &row : m_rows) {
    if (row.toMap().value(QStringLiteral("id")).toString() == rowId) {
      return true;
    }
  }
  return false;
}

bool DesktopPlacesController::open(const QString &rowId) {
  using QindaQt::Apps::FileManager::Desktop::FileBoundary;
  const QString placeId = placeIdOf(rowId);
  if (!isPlace(rowId)) {
    publishFeedback(QStringLiteral("%1 is not a desktop icon").arg(rowId));
    return false;
  }
  const QString path = locationFor(placeId);
  if (placeId == QLatin1String("trash") && !QFileInfo(path).isDir()) {
    // The Trash appears on first use (XDG trash spec: files/ and info/ side
    // by side); before that it is empty, so open it empty.
    QDir().mkpath(path);
    QDir().mkpath(QDir(QFileInfo(path).absolutePath()).filePath(QStringLiteral("info")));
  }
  const auto identity = identityOf(path);
  if (!identity) {
    publishFeedback(QStringLiteral("%1 could not be opened").arg(path));
    return false;
  }
  const auto result = m_fileManagerPrograms
                          ? FileBoundary::openLocalFolder(path, *identity, *m_fileManagerPrograms)
                          : FileBoundary::openLocalFolder(path, *identity);
  if (!result.ok()) {
    publishFeedback(result.diagnostic.isEmpty()
                        ? QStringLiteral("%1 could not be opened").arg(path)
                        : result.diagnostic);
    return false;
  }
  publishFeedback({});
  return true;
}

bool DesktopPlacesController::trashUrls(const QVariantList &urls) {
  using QindaQt::Apps::FileManager::Desktop::FileBoundary;
  const QString trashRoot = QDir::cleanPath(QFileInfo(m_locations.trashFiles).absolutePath());
  QVariantList batch;
  for (const QVariant &value : urls) {
    const QUrl url = value.toUrl();
    const QString path = url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : QString();
    // AGENT-GUARD: only local files the listing of their own folder still
    // reports are trashed, with that listing-time identity, and never
    // anything already inside the Trash. A refusal refuses the whole drop.
    if (path.isEmpty() || path == trashRoot || path.startsWith(trashRoot + QLatin1Char('/'))) {
      publishFeedback(QStringLiteral("Only local files outside the Trash can be moved to the Trash"));
      return false;
    }
    const auto listing = FileBoundary::listLocalFolder(QFileInfo(path).absolutePath());
    const QindaQt::Apps::FileManager::DirectoryEntry *found = nullptr;
    for (const auto &entry : listing.entries) {
      if (QDir::cleanPath(entry.absolutePath) == path) {
        found = &entry;
        break;
      }
    }
    if (!listing.ok() || found == nullptr) {
      publishFeedback(QStringLiteral("%1 is no longer there").arg(path));
      return false;
    }
    batch.append(QVariantMap{
        {QStringLiteral("path"), found->absolutePath},
        {QStringLiteral("device"), QString::number(found->device)},
        {QStringLiteral("inode"), QString::number(found->inode)},
        {QStringLiteral("identitySize"), QString::number(found->identitySize)},
        {QStringLiteral("modifiedNanoseconds"), QString::number(found->modifiedNanoseconds)},
        {QStringLiteral("mode"), QString::number(found->mode)},
    });
  }
  if (batch.isEmpty()) {
    return false;
  }
  if (!m_mutation) {
    m_mutation = FileBoundary::createLocalMutationController(this);
    connect(m_mutation.get(), &QindaQt::Apps::FileManager::MutationController::mutationCommitted,
            this, &DesktopPlacesController::refresh);
  }
  if (!m_mutation->trashItems(batch)) {
    publishFeedback(m_mutation->failureMessage().isEmpty()
                        ? QStringLiteral("The files could not be moved to the Trash")
                        : m_mutation->failureMessage());
    return false;
  }
  publishFeedback({});
  return true;
}

void DesktopPlacesController::publishFeedback(const QString &message) {
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopSurface
