// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <array>

namespace QindaQt::Apps::SettingsDefaultApps {

// One user-facing default-application category. Categories with no
// freedesktop-standard association mechanism (there is no "default
// terminal" concept in either xdg-mime or xdg-settings) are not listed here;
// see docs/wiki/apps/default-applications.md for the deferral.
enum class DefaultApplicationCategory {
  Browser,
  Mail,
  FileManager,
  TextEditor,
  ImageViewer,
  VideoPlayer,
  MusicPlayer,
};

inline constexpr std::array kDefaultApplicationCategories{
    DefaultApplicationCategory::Browser,
    DefaultApplicationCategory::Mail,
    DefaultApplicationCategory::FileManager,
    DefaultApplicationCategory::TextEditor,
    DefaultApplicationCategory::ImageViewer,
    DefaultApplicationCategory::VideoPlayer,
    DefaultApplicationCategory::MusicPlayer,
};

// The exact mimetype set xdg-mime/xdg-settings associate with each category.
// AGENT-CONTRACT: this is the complete, closed write set for its category;
// growing it needs a docs update, since every listed mimetype is written
// together on one choice (matching xdg-settings' own default-web-browser,
// which sets text/html plus both http(s) scheme handlers as one unit).
[[nodiscard]] QStringList defaultApplicationCategoryMimeTypes(
    DefaultApplicationCategory category);
[[nodiscard]] QString defaultApplicationCategoryId(
    DefaultApplicationCategory category);
[[nodiscard]] QString defaultApplicationCategoryLabel(
    DefaultApplicationCategory category);

struct DefaultApplicationPreferences final {
  // Desktop-entry id (e.g. "org.qindaqt.FileManager.desktop") or empty when
  // no default is configured for that category. Keyed by the category's
  // first (representative) mimetype on load; written to every mimetype in
  // the category's set on save.
  QString browser;
  QString mail;
  QString fileManager;
  QString textEditor;
  QString imageViewer;
  QString videoPlayer;
  QString musicPlayer;

  [[nodiscard]] QString category(DefaultApplicationCategory category) const;
  void setCategory(DefaultApplicationCategory category, const QString &desktopId);
};

// Local-file boundary for the freedesktop "Default Applications" mechanism
// (the same [Default Applications] group of mimeapps.list that xdg-mime and
// xdg-settings themselves read and write; verified against real xdg-mime
// query default / xdg-settings get output on qinda-top). Implementations
// preserve every unrelated group and key, including [Added Associations]
// and every mimetype this route does not manage.
class DefaultApplicationsStore {
public:
  virtual ~DefaultApplicationsStore() = default;
  [[nodiscard]] virtual bool load(DefaultApplicationPreferences *preferences,
                                  QString *error) = 0;
  [[nodiscard]] virtual bool save(const DefaultApplicationPreferences &preferences,
                                  QString *error) = 0;
};

class MimeAppsDefaultApplicationsStore final : public DefaultApplicationsStore {
public:
  explicit MimeAppsDefaultApplicationsStore(QString filePath);
  [[nodiscard]] bool load(DefaultApplicationPreferences *preferences,
                          QString *error) override;
  [[nodiscard]] bool save(const DefaultApplicationPreferences &preferences,
                          QString *error) override;

private:
  QString m_filePath;
};

} // namespace QindaQt::Apps::SettingsDefaultApps
