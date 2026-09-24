// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <array>
#include <memory>

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
// growing it needs a docs update. A choice changes only MIME types the
// selected application effectively supports; other keys retain their values.
[[nodiscard]] QStringList defaultApplicationCategoryMimeTypes(
    DefaultApplicationCategory category);
[[nodiscard]] QString defaultApplicationCategoryId(
    DefaultApplicationCategory category);
[[nodiscard]] QString defaultApplicationCategoryLabel(
    DefaultApplicationCategory category);

struct DefaultApplicationPreferences final {
  // Desktop-entry id (e.g. "org.qindaqt.FileManager.desktop") when every
  // MIME type in a category resolves to the same handler. Empty also covers
  // a mixed category; isMixed() distinguishes it from no default.
  QString browser;
  QString mail;
  QString fileManager;
  QString textEditor;
  QString imageViewer;
  QString pdfViewer;
  QString videoPlayer;
  QString musicPlayer;
  // Effective per-MIME truth and current association eligibility from one
  // lookup snapshot. Stubs may leave these empty to exercise aggregate values.
  QMap<QString, QString> effectiveByMimeType;
  QMap<QString, QStringList> supportedMimeTypesByDesktopId;
  bool associationProjectionAvailable = false;

  [[nodiscard]] bool isMixed(DefaultApplicationCategory category) const;
  [[nodiscard]] QString category(DefaultApplicationCategory category) const;
  void setCategory(DefaultApplicationCategory category, const QString &desktopId);
};

// ADR-0269: one MIME type's handlers, as File Manager's Open With lists them.
struct MimeTypeHandlers final {
  // The effective default desktop ID ("x.desktop"); empty when none resolves.
  QString defaultDesktopId;
  // Every associated desktop ID, most preferred first: the default, then the
  // user's Added Associations in lookup order, then the rest in scan order.
  QStringList desktopIds;

  friend bool operator==(const MimeTypeHandlers &, const MimeTypeHandlers &) = default;
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
  // A refreshed public installed-application scan replaces the store's
  // previously copied snapshot. The default is a no-op for injected stubs.
  virtual void setApplications(QindaQt::ApplicationCatalog::DirectoryScan applications) {
    Q_UNUSED(applications);
  }
  // AGENT-CONTRACT (ADR-0269): File Manager's Open With reads and writes one
  // MIME type at a time through this same store, so Settings and Open With
  // share a single association authority. loadMimeTypeHandlers resolves the
  // type exactly as load() resolves a category's types; saveMimeTypeDefault
  // makes desktopId that type's default in the user's file (recording it as
  // an Added Association when the entry does not declare the type, since
  // lookup skips an unassociated default). The defaults refuse, so category
  // stubs need not implement them.
  [[nodiscard]] virtual bool loadMimeTypeHandlers(const QString &mimeType,
                                                  MimeTypeHandlers *handlers,
                                                  QString *error) {
    Q_UNUSED(mimeType);
    Q_UNUSED(handlers);
    if (error) *error = QStringLiteral("default-applications-unsupported-store");
    return false;
  }
  [[nodiscard]] virtual bool saveMimeTypeDefault(const QString &mimeType,
                                                 const QString &desktopId,
                                                 QString *error) {
    Q_UNUSED(mimeType);
    Q_UNUSED(desktopId);
    if (error) *error = QStringLiteral("default-applications-unsupported-store");
    return false;
  }
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
  void setApplications(QindaQt::ApplicationCatalog::DirectoryScan applications) override;
  [[nodiscard]] bool loadMimeTypeHandlers(const QString &mimeType,
                                          MimeTypeHandlers *handlers,
                                          QString *error) override;
  [[nodiscard]] bool saveMimeTypeDefault(const QString &mimeType,
                                         const QString &desktopId,
                                         QString *error) override;

private:
  QString m_filePath;
  QStringList m_lookupPaths;
  QindaQt::ApplicationCatalog::DirectoryScan m_applications;
  QStringList m_userDesktopPaths;
};

// The session's store over the user's mimeapps.list, composed exactly as
// Settings -> Default Applications composes it: the generic
// $XDG_CONFIG_HOME/mimeapps.list is the write target, a desktop-specific user
// file is edited only where it already owns a key, and lookup follows the XDG
// config roots, then each data root's applications/ directory. A composition
// helper (ADR-0269): unlike the store, it reads QStandardPaths and
// XDG_CURRENT_DESKTOP. `lookupPaths`, when non-null, receives the lookup order
// (the Settings route watches those files).
[[nodiscard]] std::unique_ptr<DefaultApplicationsStore> createSessionDefaultApplicationsStore(
    const QStringList &dataRoots, QindaQt::ApplicationCatalog::DirectoryScan applications,
    QStringList *lookupPaths = nullptr);

} // namespace QindaQt::Apps::SettingsDefaultApps
