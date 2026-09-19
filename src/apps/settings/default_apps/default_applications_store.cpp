// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

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

const QindaQt::ApplicationCatalog::ScannedApplication *applicationForDesktopId(
    const QindaQt::ApplicationCatalog::DirectoryScan &scan, const QString &desktopId) {
  // The shared catalog deliberately uses suffix-free launcher identity.
  return desktopId.endsWith(QStringLiteral(".desktop"))
      ? scan.application(desktopId.chopped(8)) : nullptr;
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
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    const QString mimeType = defaultApplicationCategoryMimeTypes(category).first();
    QString selected;
    for (const MimeAppsFile &file : files) {
      for (const QString &desktopId : desktopIds(file.defaults.value(mimeType))) {
        const auto *application = applicationForDesktopId(m_applications, desktopId);
        if (application && isAssociated(*application, mimeType, files)) {
          selected = desktopId;
          break;
        }
      }
      if (!selected.isEmpty()) break;
    }
    next.setCategory(category, selected);
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
  if (!desktopId.isEmpty()) {
    const auto *application = applicationForDesktopId(m_applications, desktopId);
    if (!application) {
      if (error) *error = QStringLiteral("default-applications-unknown-application");
      return false;
    }
    QList<MimeAppsFile> files;
    if (!readMimeAppsFiles(m_lookupPaths, &files, error)) return false;
    if (!isAssociated(*application, mimeTypes.first(), files)) {
      if (error) *error = QStringLiteral("default-applications-unsupported-application");
      return false;
    }
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
  for (const QString &mimeType : mimeTypes) {
    if (desktopId.isEmpty()) group.deleteEntry(mimeType);
    else group.writeEntry(mimeType, desktopId + QLatin1Char(';'));
  }
  if (!config->sync()) {
    if (error) *error = QStringLiteral("default-applications-sync-failed");
    return false;
  }
  if (error) error->clear();
  return true;
}

} // namespace QindaQt::Apps::SettingsDefaultApps
