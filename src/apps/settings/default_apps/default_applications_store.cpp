// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

#include <utility>

namespace QindaQt::Apps::SettingsDefaultApps {
namespace {
constexpr auto DefaultApplicationsGroup = "Default Applications";
} // namespace

QStringList defaultApplicationCategoryMimeTypes(
    const DefaultApplicationCategory category) {
  switch (category) {
  case DefaultApplicationCategory::Browser:
    return {QStringLiteral("text/html"), QStringLiteral("x-scheme-handler/http"),
           QStringLiteral("x-scheme-handler/https")};
  case DefaultApplicationCategory::Mail:
    return {QStringLiteral("x-scheme-handler/mailto")};
  case DefaultApplicationCategory::FileManager:
    return {QStringLiteral("inode/directory")};
  case DefaultApplicationCategory::TextEditor:
    return {QStringLiteral("text/plain")};
  case DefaultApplicationCategory::ImageViewer:
    return {QStringLiteral("image/jpeg"), QStringLiteral("image/png"),
           QStringLiteral("image/gif"), QStringLiteral("image/webp"),
           QStringLiteral("image/bmp"), QStringLiteral("image/tiff"),
           QStringLiteral("image/svg+xml")};
  case DefaultApplicationCategory::PdfViewer:
    return {QStringLiteral("application/pdf")};
  case DefaultApplicationCategory::VideoPlayer:
    return {QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"),
           QStringLiteral("video/webm"), QStringLiteral("video/mpeg"),
           QStringLiteral("video/x-msvideo")};
  case DefaultApplicationCategory::MusicPlayer:
    return {QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"),
           QStringLiteral("audio/ogg")};
  }
  return {};
}

QString defaultApplicationCategoryId(const DefaultApplicationCategory category) {
  switch (category) {
  case DefaultApplicationCategory::Browser: return QStringLiteral("browser");
  case DefaultApplicationCategory::Mail: return QStringLiteral("mail");
  case DefaultApplicationCategory::FileManager: return QStringLiteral("file-manager");
  case DefaultApplicationCategory::TextEditor: return QStringLiteral("text-editor");
  case DefaultApplicationCategory::ImageViewer: return QStringLiteral("image-viewer");
  case DefaultApplicationCategory::PdfViewer: return QStringLiteral("pdf-viewer");
  case DefaultApplicationCategory::VideoPlayer: return QStringLiteral("video-player");
  case DefaultApplicationCategory::MusicPlayer: return QStringLiteral("music-player");
  }
  return {};
}

QString defaultApplicationCategoryLabel(const DefaultApplicationCategory category) {
  switch (category) {
  case DefaultApplicationCategory::Browser: return QStringLiteral("Web browser");
  case DefaultApplicationCategory::Mail: return QStringLiteral("Mail client");
  case DefaultApplicationCategory::FileManager: return QStringLiteral("File manager");
  case DefaultApplicationCategory::TextEditor: return QStringLiteral("Text editor");
  case DefaultApplicationCategory::ImageViewer: return QStringLiteral("Image viewer");
  case DefaultApplicationCategory::PdfViewer: return QStringLiteral("PDF viewer");
  case DefaultApplicationCategory::VideoPlayer: return QStringLiteral("Video player");
  case DefaultApplicationCategory::MusicPlayer: return QStringLiteral("Music player");
  }
  return {};
}

QString DefaultApplicationPreferences::category(
    const DefaultApplicationCategory category) const {
  switch (category) {
  case DefaultApplicationCategory::Browser: return browser;
  case DefaultApplicationCategory::Mail: return mail;
  case DefaultApplicationCategory::FileManager: return fileManager;
  case DefaultApplicationCategory::TextEditor: return textEditor;
  case DefaultApplicationCategory::ImageViewer: return imageViewer;
  case DefaultApplicationCategory::PdfViewer: return pdfViewer;
  case DefaultApplicationCategory::VideoPlayer: return videoPlayer;
  case DefaultApplicationCategory::MusicPlayer: return musicPlayer;
  }
  return {};
}

bool DefaultApplicationPreferences::isMixed(
    const DefaultApplicationCategory category) const {
  const QStringList mimeTypes = defaultApplicationCategoryMimeTypes(category);
  if (effectiveByMimeType.isEmpty() || mimeTypes.size() < 2) return false;
  const QString first = effectiveByMimeType.value(mimeTypes.first());
  for (const QString &mimeType : mimeTypes)
    if (effectiveByMimeType.value(mimeType) != first) return true;
  return false;
}

void DefaultApplicationPreferences::setCategory(
    const DefaultApplicationCategory category, const QString &desktopId) {
  switch (category) {
  case DefaultApplicationCategory::Browser: browser = desktopId; return;
  case DefaultApplicationCategory::Mail: mail = desktopId; return;
  case DefaultApplicationCategory::FileManager: fileManager = desktopId; return;
  case DefaultApplicationCategory::TextEditor: textEditor = desktopId; return;
  case DefaultApplicationCategory::ImageViewer: imageViewer = desktopId; return;
  case DefaultApplicationCategory::PdfViewer: pdfViewer = desktopId; return;
  case DefaultApplicationCategory::VideoPlayer: videoPlayer = desktopId; return;
  case DefaultApplicationCategory::MusicPlayer: musicPlayer = desktopId; return;
  }
}

QStringList defaultApplicationsLookupPaths(
    const QStringList &configRoots, const QStringList &dataRoots,
    const QStringList &desktopNames) {
  QStringList names;
  for (const QString &desktop : desktopNames) {
    const QString name = desktop.toLower();
    bool valid = !name.isEmpty();
    for (const QChar character : name) {
      if (!((character >= u'a' && character <= u'z')
            || (character >= u'0' && character <= u'9')
            || character == u'-' || character == u'_')) valid = false;
    }
    if (valid && !names.contains(name)) names.append(name);
  }
  QStringList paths;
  const auto appendDirectory = [&](const QString &directory) {
    for (const QString &name : names)
      paths.append(QDir(directory).filePath(name + QStringLiteral("-mimeapps.list")));
    paths.append(QDir(directory).filePath(QStringLiteral("mimeapps.list")));
  };
  for (const QString &root : configRoots) appendDirectory(root);
  for (const QString &root : dataRoots)
    appendDirectory(QDir(root).filePath(QStringLiteral("applications")));
  paths.removeDuplicates();
  return paths;
}

namespace {
struct MimeAppsFile final {
  QString path;
  QMap<QString, QString> defaults;
  QMap<QString, QString> added;
  QMap<QString, QString> removed;
};

bool readMimeAppsFiles(const QStringList &paths, QList<MimeAppsFile> *files,
                       QString *error) {
  for (const QString &path : paths) {
    if (QFileInfo::exists(path)) {
      QFile readable(path);
      if (!readable.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("default-applications-read-failed");
        return false;
      }
    }
    const KConfig config(path, KConfig::SimpleConfig);
    files->append({path, config.entryMap(QString::fromLatin1(DefaultApplicationsGroup)),
                   config.entryMap(QStringLiteral("Added Associations")),
                   config.entryMap(QStringLiteral("Removed Associations"))});
  }
  return true;
}

QStringList desktopIds(const QString &value) {
  QStringList result;
  for (const QString &part : value.split(QLatin1Char(';'))) {
    const QString id = part.trimmed();
    if (!id.isEmpty()) result.append(id);
  }
  return result;
}

// AGENT-CONTRACT: this route's canonical id keeps the ".desktop" suffix - the
// catalog says so where it builds candidate ids, and the freedesktop MIME Apps
// specification says a value is a desktop file ID. Everything crossing this
// boundary is normalised to that form exactly once, here.
//
// AGENT-GUARD: a stored value WITHOUT the suffix is read, not refused. Real
// mimeapps.list files on both machines contain some. Found 2026-09-21:
// `text/plain=org.qindaqt.TextEditor`, `image/gif=lximage-qt` and
// `audio/flac=org.qindaqt.Player`, sitting beside correctly written
// neighbours like `inode/directory=org.qindaqt.FileManager.desktop`. All three
// applications were installed, so the only thing wrong was the missing
// suffix - and because lookup refused them, the Settings page showed "no
// default" for four categories while the file plainly had values, and the
// system could not launch them either.
//
// Read tolerantly, normalise immediately, write strictly. Normalising on read
// matters as much as tolerating: a suffix-free value carried further would
// fail the catalog's `candidate.id == desktopId` match and the page would show
// a raw id where an application name belongs.
QString canonicalDesktopId(const QString &desktopId) {
  const QString trimmed = desktopId.trimmed();
  if (trimmed.isEmpty()) return {};
  return trimmed.endsWith(QStringLiteral(".desktop"))
      ? trimmed : trimmed + QStringLiteral(".desktop");
}

const QindaQt::ApplicationCatalog::ScannedApplication *applicationForDesktopId(
    const QindaQt::ApplicationCatalog::DirectoryScan &scan, const QString &desktopId) {
  // ApplicationCatalog ids omit the suffix, so chop it for the lookup.
  const QString canonical = canonicalDesktopId(desktopId);
  return canonical.isEmpty() ? nullptr : scan.application(canonical.chopped(8));
}

bool isAssociated(const QindaQt::ApplicationCatalog::ScannedApplication &app,
                  const QString &mimeType, const QList<MimeAppsFile> &files) {
  for (const MimeAppsFile &file : files) {
    // Desktop-specific files cannot add/remove MIME associations. Stop at
    // the application's own directory: lower-precedence data cannot alter
    // a higher-priority desktop entry (freedesktop MIME Apps specification).
    if (QFileInfo(file.path).fileName() != QLatin1String("mimeapps.list")) continue;
    const QString desktopId = app.entry.id + QStringLiteral(".desktop");
    if (desktopIds(file.added.value(mimeType)).contains(desktopId)) return true;
    if (desktopIds(file.removed.value(mimeType)).contains(desktopId)) return false;
    const QString relative = QDir(QFileInfo(file.path).absolutePath())
                                 .relativeFilePath(app.desktopFilePath);
    if (!app.desktopFilePath.isEmpty() && !relative.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relative)) break;
  }
  return desktopEntryMimeTypes(app.documentText).contains(mimeType);
}

// The effective default for one MIME type: the first installed, associated
// desktop ID in lookup order (freedesktop MIME Apps specification).
QString effectiveDefault(const QindaQt::ApplicationCatalog::DirectoryScan &scan,
                         const QString &mimeType, const QList<MimeAppsFile> &files) {
  for (const MimeAppsFile &file : files) {
    for (const QString &desktopId : desktopIds(file.defaults.value(mimeType))) {
      const auto *application = applicationForDesktopId(scan, desktopId);
      if (application && isAssociated(*application, mimeType, files))
        return canonicalDesktopId(desktopId);
    }
  }
  return {};
}

// ADR-0269: a key Open With may write -- "type/subtype" in plain ASCII, so a
// value from the MIME database can never become a group header, an
// assignment, or a list separator in the file.
bool validMimeTypeKey(const QString &mimeType) {
  const qsizetype slash = mimeType.indexOf(QLatin1Char('/'));
  if (mimeType.size() > 255 || slash <= 0 || slash == mimeType.size() - 1
      || slash != mimeType.lastIndexOf(QLatin1Char('/'))) return false;
  for (const QChar character : mimeType) {
    const bool alphanumeric = (character >= u'a' && character <= u'z')
        || (character >= u'A' && character <= u'Z') || (character >= u'0' && character <= u'9');
    if (!alphanumeric && !QStringLiteral("/!#$&^_.+-").contains(character)) return false;
  }
  return true;
}
} // namespace

MimeAppsDefaultApplicationsStore::MimeAppsDefaultApplicationsStore(
    QString filePath, QStringList lookupPaths,
    QindaQt::ApplicationCatalog::DirectoryScan applications,
    QStringList userDesktopPaths)
    : m_filePath(std::move(filePath)), m_lookupPaths(std::move(lookupPaths)),
      m_applications(std::move(applications)),
      m_userDesktopPaths(std::move(userDesktopPaths)) {}

bool MimeAppsDefaultApplicationsStore::load(
    DefaultApplicationPreferences *preferences, QString *error) {
  if (preferences == nullptr || m_filePath.trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("default-applications file location is unavailable");
    return false;
  }
  QList<MimeAppsFile> files;
  if (!readMimeAppsFiles(m_lookupPaths, &files, error)) return false;
  DefaultApplicationPreferences next;
  next.associationProjectionAvailable = true;
  for (const auto &application : m_applications.applications) {
    QStringList associated;
    for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
      for (const QString &mimeType : defaultApplicationCategoryMimeTypes(category)) {
        if (isAssociated(application, mimeType, files) && !associated.contains(mimeType))
          associated.append(mimeType);
      }
    }
    next.supportedMimeTypesByDesktopId.insert(
        application.entry.id + QStringLiteral(".desktop"), associated);
  }
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    const QStringList mimeTypes = defaultApplicationCategoryMimeTypes(category);
    for (const QString &mimeType : mimeTypes)
      next.effectiveByMimeType.insert(mimeType, effectiveDefault(m_applications, mimeType, files));
    // An aggregate choice is truthful only if every managed MIME resolves
    // identically. The page shows mixed state and lets the user choose a
    // partial-scope handler without losing other MIME associations.
    if (!next.isMixed(category))
      next.setCategory(category, next.effectiveByMimeType.value(mimeTypes.first()));
  }
  *preferences = std::move(next);
  if (error) error->clear();
  return true;
}

bool MimeAppsDefaultApplicationsStore::saveCategory(
    const DefaultApplicationCategory category, const QString &desktopId,
    QString *error) {
  const QStringList mimeTypes = defaultApplicationCategoryMimeTypes(category);
  if (m_filePath.trimmed().isEmpty() || mimeTypes.isEmpty()) {
    if (error) *error = QStringLiteral("default-applications file location is unavailable");
    return false;
  }
  if (desktopId.isEmpty()) {
    // "Use inherited default" clears every user-level override for these
    // MIME types. A desktop-specific and generic user file can both own keys;
    // clearing only the first would reveal another user override.
    QStringList userPaths = m_userDesktopPaths;
    userPaths.append(m_filePath);
    userPaths.removeDuplicates();
    QVector<KSharedConfig::Ptr> configs;
    for (const QString &path : userPaths) {
      if (!QFileInfo::exists(path)) continue;
      auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
      if (!config || config->accessMode() != KConfigBase::ReadWrite) {
        if (error) *error = QStringLiteral("default-applications-not-writable");
        return false;
      }
      configs.append(config);
    }
    for (const auto &config : configs) {
      config->reparseConfiguration();
      KConfigGroup group = config->group(QString::fromLatin1(DefaultApplicationsGroup));
      for (const QString &mimeType : mimeTypes) group.deleteEntry(mimeType);
      if (!config->sync()) {
        if (error) *error = QStringLiteral("default-applications-sync-failed");
        return false;
      }
    }
    if (error) error->clear();
    return true;
  }
  QStringList writeMimeTypes;
  const auto *application = applicationForDesktopId(m_applications, desktopId);
  if (!application) {
    if (error) *error = QStringLiteral("default-applications-unknown-application");
    return false;
  }
  QList<MimeAppsFile> files;
  if (!readMimeAppsFiles(m_lookupPaths, &files, error)) return false;
  for (const QString &mimeType : mimeTypes)
    if (isAssociated(*application, mimeType, files)) writeMimeTypes.append(mimeType);
  if (writeMimeTypes.isEmpty()) {
    if (error) *error = QStringLiteral("default-applications-unsupported-application");
    return false;
  }
  QString target = m_filePath;
  for (const QString &path : m_userDesktopPaths) {
    const KConfig current(path, KConfig::SimpleConfig);
    const KConfigGroup group = current.group(QString::fromLatin1(DefaultApplicationsGroup));
    bool ownsCategory = false;
    for (const QString &mimeType : mimeTypes) ownsCategory |= group.hasKey(mimeType);
    if (ownsCategory) {
      target = path;
      break;
    }
  }
  const KSharedConfig::Ptr config = KSharedConfig::openConfig(target, KConfig::SimpleConfig);
  if (!config || config->accessMode() != KConfigBase::ReadWrite) {
    if (error) *error = QStringLiteral("default-applications-not-writable");
    return false;
  }
  // AGENT-GUARD: Reparse and edit only the selected category. Rewriting a
  // loaded snapshot would copy inherited defaults into user policy and
  // clobber another application's intervening choices in other categories.
  config->reparseConfiguration();
  KConfigGroup group = config->group(QString::fromLatin1(DefaultApplicationsGroup));
  // Write strictly: whatever spelling the caller used, the file gets a real
  // desktop file ID. This is also the repair path for the legacy suffix-free
  // values described above - choosing anything in the page rewrites them.
  const QString canonical = canonicalDesktopId(desktopId);
  for (const QString &mimeType : writeMimeTypes)
    group.writeEntry(mimeType, canonical + QLatin1Char(';'));
  if (!config->sync()) {
    if (error) *error = QStringLiteral("default-applications-sync-failed");
    return false;
  }
  if (error) error->clear();
  return true;
}

void MimeAppsDefaultApplicationsStore::setApplications(
    QindaQt::ApplicationCatalog::DirectoryScan applications) {
  m_applications = std::move(applications);
}

bool MimeAppsDefaultApplicationsStore::loadMimeTypeHandlers(
    const QString &mimeType, MimeTypeHandlers *handlers, QString *error) {
  if (handlers == nullptr || !validMimeTypeKey(mimeType)) {
    if (error) *error = QStringLiteral("default-applications-invalid-mime-type");
    return false;
  }
  QList<MimeAppsFile> files;
  if (!readMimeAppsFiles(m_lookupPaths, &files, error)) return false;
  MimeTypeHandlers next;
  next.defaultDesktopId = effectiveDefault(m_applications, mimeType, files);
  if (!next.defaultDesktopId.isEmpty()) next.desktopIds.append(next.defaultDesktopId);
  const auto append = [&](const QindaQt::ApplicationCatalog::ScannedApplication &application) {
    const QString desktopId = application.entry.id + QStringLiteral(".desktop");
    if (!next.desktopIds.contains(desktopId) && isAssociated(application, mimeType, files))
      next.desktopIds.append(desktopId);
  };
  // The MIME Apps specification orders Added Associations by preference;
  // desktop-specific files cannot add associations (see isAssociated).
  for (const MimeAppsFile &file : files) {
    if (QFileInfo(file.path).fileName() != QLatin1String("mimeapps.list")) continue;
    for (const QString &desktopId : desktopIds(file.added.value(mimeType)))
      if (const auto *application = applicationForDesktopId(m_applications, desktopId))
        append(*application);
  }
  for (const auto &application : m_applications.applications) append(application);
  *handlers = std::move(next);
  if (error) error->clear();
  return true;
}

bool MimeAppsDefaultApplicationsStore::saveMimeTypeDefault(
    const QString &mimeType, const QString &desktopId, QString *error) {
  if (m_filePath.trimmed().isEmpty() || !validMimeTypeKey(mimeType)) {
    if (error) *error = QStringLiteral("default-applications-invalid-mime-type");
    return false;
  }
  const auto *application = applicationForDesktopId(m_applications, desktopId);
  if (!application) {
    if (error) *error = QStringLiteral("default-applications-unknown-application");
    return false;
  }
  QList<MimeAppsFile> files;
  if (!readMimeAppsFiles(m_lookupPaths, &files, error)) return false;
  // As in saveCategory: a desktop-specific user file that already names this
  // type's default outranks the generic file, so the choice is edited there.
  QString target = m_filePath;
  for (const QString &path : m_userDesktopPaths) {
    const KConfig current(path, KConfig::SimpleConfig);
    if (current.group(QString::fromLatin1(DefaultApplicationsGroup)).hasKey(mimeType)) {
      target = path;
      break;
    }
  }
  const QString canonical = canonicalDesktopId(desktopId);
  // AGENT-GUARD: lookup skips a default that is not an associated handler, so
  // an application picked through Other Application... that does not declare
  // the type is also made the user's first Added Association. Only the
  // generic file may add associations; a desktop-specific one cannot.
  const bool associate = !isAssociated(*application, mimeType, files);
  QStringList paths{target};
  if (associate && target != m_filePath) paths.append(m_filePath);
  for (const QString &path : paths) {
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    if (!config || config->accessMode() != KConfigBase::ReadWrite) {
      if (error) *error = QStringLiteral("default-applications-not-writable");
      return false;
    }
    // Reparse and edit one key only, never a loaded snapshot (see saveCategory).
    config->reparseConfiguration();
    if (path == target) {
      KConfigGroup defaults = config->group(QString::fromLatin1(DefaultApplicationsGroup));
      defaults.writeEntry(mimeType, canonical + QLatin1Char(';'));
    }
    if (associate && path == m_filePath) {
      KConfigGroup added = config->group(QStringLiteral("Added Associations"));
      QStringList preferred = desktopIds(added.readEntry(mimeType, QString()));
      preferred.removeAll(canonical);
      preferred.prepend(canonical);
      added.writeEntry(mimeType, preferred.join(QLatin1Char(';')) + QLatin1Char(';'));
    }
    if (!config->sync()) {
      if (error) *error = QStringLiteral("default-applications-sync-failed");
      return false;
    }
  }
  if (error) error->clear();
  return true;
}

std::unique_ptr<DefaultApplicationsStore> createSessionDefaultApplicationsStore(
    const QStringList &dataRoots, QindaQt::ApplicationCatalog::DirectoryScan applications,
    QStringList *lookupPaths) {
  const QString configHome =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP")
                                   .split(QLatin1Char(':'), Qt::SkipEmptyParts);
  QStringList userDesktopPaths = defaultApplicationsLookupPaths({configHome}, {}, desktops);
  userDesktopPaths.removeLast(); // The generic user file is the normal write target.
  const QStringList lookup = defaultApplicationsLookupPaths(
      QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation), dataRoots,
      desktops);
  if (lookupPaths) *lookupPaths = lookup;
  return std::make_unique<MimeAppsDefaultApplicationsStore>(
      QDir(configHome).filePath(QStringLiteral("mimeapps.list")), lookup,
      std::move(applications), userDesktopPaths);
}

} // namespace QindaQt::Apps::SettingsDefaultApps
