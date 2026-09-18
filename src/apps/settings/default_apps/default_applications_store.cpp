// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <KConfigGroup>
#include <KSharedConfig>

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
           QStringLiteral("image/gif"), QStringLiteral("image/webp")};
  case DefaultApplicationCategory::VideoPlayer:
    return {QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"),
           QStringLiteral("video/webm")};
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
  case DefaultApplicationCategory::VideoPlayer: videoPlayer = desktopId; return;
  case DefaultApplicationCategory::MusicPlayer: musicPlayer = desktopId; return;
  }
}

MimeAppsDefaultApplicationsStore::MimeAppsDefaultApplicationsStore(QString filePath)
    : m_filePath(std::move(filePath)) {}

bool MimeAppsDefaultApplicationsStore::load(
    DefaultApplicationPreferences *preferences, QString *error) {
  if (preferences == nullptr || m_filePath.trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("default-applications file location is unavailable");
    return false;
  }
  const KConfig config(m_filePath, KConfig::SimpleConfig);
  const KConfigGroup group = config.group(QString::fromLatin1(DefaultApplicationsGroup));
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    const QStringList mimeTypes = defaultApplicationCategoryMimeTypes(category);
    if (mimeTypes.isEmpty()) continue;
    // AGENT-CONTRACT: the category's representative (first) mimetype is the
    // one xdg-mime query default would report for it; read that one only, so
    // a partially-overwritten group (one mimetype set by a foreign tool, the
    // others not) never silently averages into a mismatched displayed value.
    preferences->setCategory(category, group.readEntry(mimeTypes.first(), QString()));
  }
  return true;
}

bool MimeAppsDefaultApplicationsStore::save(
    const DefaultApplicationPreferences &preferences, QString *error) {
  if (m_filePath.trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("default-applications file location is unavailable");
    return false;
  }
  const KSharedConfig::Ptr config = KSharedConfig::openConfig(m_filePath, KConfig::SimpleConfig);
  if (!config || config->accessMode() != KConfigBase::ReadWrite) {
    if (error) *error = QStringLiteral("default-applications-not-writable");
    return false;
  }
  // AGENT-GUARD: reparse before write so a concurrent external edit (another
  // app registering a scheme handler, say) is never clobbered by a stale
  // in-memory copy of unrelated keys.
  config->reparseConfiguration();
  KConfigGroup group = config->group(QString::fromLatin1(DefaultApplicationsGroup));
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    const QString desktopId = preferences.category(category);
    for (const QString &mimeType : defaultApplicationCategoryMimeTypes(category)) {
      if (desktopId.trimmed().isEmpty()) {
        group.deleteEntry(mimeType);
      } else {
        group.writeEntry(mimeType, desktopId);
      }
    }
  }
  if (!config->sync()) {
    if (error) *error = QStringLiteral("default-applications-sync-failed");
    return false;
  }
  return true;
}

} // namespace QindaQt::Apps::SettingsDefaultApps
