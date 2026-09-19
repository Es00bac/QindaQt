// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/application_catalog/application_directory_scan.h>

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
  PdfViewer,
  VideoPlayer,
  MusicPlayer,
};

inline constexpr std::array kDefaultApplicationCategories{
    DefaultApplicationCategory::Browser,
    DefaultApplicationCategory::Mail,
    DefaultApplicationCategory::FileManager,
    DefaultApplicationCategory::TextEditor,
    DefaultApplicationCategory::ImageViewer,
    DefaultApplicationCategory::PdfViewer,
    DefaultApplicationCategory::VideoPlayer,
    DefaultApplicationCategory::MusicPlayer,
};

// The MIME type sets managed by this Settings route.
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
  QString pdfViewer;
  QString videoPlayer;
  QString musicPlayer;

  [[nodiscard]] QString category(DefaultApplicationCategory category) const;
  void setCategory(DefaultApplicationCategory category, const QString &desktopId);
};

// Build the freedesktop lookup order from caller-owned roots (home first),
// then each system root. Desktop names are the XDG_CURRENT_DESKTOP components.
// No environment lookup or filesystem access occurs in this helper.
[[nodiscard]] QStringList defaultApplicationsLookupPaths(
    const QStringList &configRoots, const QStringList &dataRoots,
    const QStringList &desktopNames);

// Synchronous, caller-thread local-file boundary. load returns effective
// installed defaults; saveCategory writes only the user's chosen category.
// An empty desktop ID removes that category's override, exposing inherited
// defaults. Errors are returned without publishing a successful model change.
class DefaultApplicationsStore {
public:
  virtual ~DefaultApplicationsStore() = default;
  [[nodiscard]] virtual bool load(DefaultApplicationPreferences *preferences,
                                  QString *error) = 0;
  [[nodiscard]] virtual bool saveCategory(DefaultApplicationCategory category,
                                         const QString &desktopId,
                                         QString *error) = 0;
};

class MimeAppsDefaultApplicationsStore final : public DefaultApplicationsStore {
public:
  // Owns its copied inputs. lookupPaths are in highest-first XDG order;
  // applications is the composition root's already-completed public scan,
  // using IncludeNoDisplay because MIME handlers need not appear in menus.
  // userDesktopPaths names only higher-priority user files in lookupPaths:
  // an existing category override there is edited instead of being masked.
  MimeAppsDefaultApplicationsStore(
      QString filePath, QStringList lookupPaths,
      QindaQt::ApplicationCatalog::DirectoryScan applications,
      QStringList userDesktopPaths = {});
  [[nodiscard]] bool load(DefaultApplicationPreferences *preferences,
                          QString *error) override;
  [[nodiscard]] bool saveCategory(DefaultApplicationCategory category,
                                  const QString &desktopId,
                                  QString *error) override;

private:
  QString m_filePath;
  QStringList m_lookupPaths;
  QindaQt::ApplicationCatalog::DirectoryScan m_applications;
  QStringList m_userDesktopPaths;
};

} // namespace QindaQt::Apps::SettingsDefaultApps
