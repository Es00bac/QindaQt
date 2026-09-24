// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"
#include "open_with_launcher.h"

#include "qindaqt/application_catalog/application_directory_scan.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <functional>
#include <memory>

namespace QindaQt::Apps::SettingsDefaultApps {
class DefaultApplicationsStore;
}

namespace QindaQt::Apps::FileManager {

// Open With for one File Manager window (ADR-0269): which applications
// handle the selected files, opening the files with one of them, and making
// one the default for a file type.
//
// AGENT-CONTRACT: there is no second MIME authority here. A file's type comes
// from the shared MIME database (QMimeDatabase, shared-mime-info); its
// handlers and default come from Settings -> Default Applications' own store
// (mimeapps.list plus desktop entries, the type's parent types included), and
// Always Open With writes through that same store. The applications are the
// shared catalog's scan including NoDisplay handlers, which is also what the
// launch plans from; the Other Application... chooser lists the Applications
// place's own rows (ADR-0262), so a picker shows exactly what that place
// shows. GUI-thread only; each candidate query rescans synchronously within
// the catalog's bounds.
class OpenWithController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
  using Scanner = std::function<QindaQt::ApplicationCatalog::DirectoryScan()>;
  using ApplicationsSource = std::function<ListingResult()>;

  // `handlers` is the IncludeNoDisplay catalog scan; `applications` is the
  // Applications place's listing (production: ApplicationsController).
  OpenWithController(Scanner handlers, ApplicationsSource applications,
                     std::unique_ptr<SettingsDefaultApps::DefaultApplicationsStore> store,
                     OpenWithLauncher launcher, QObject *parent = nullptr);
  ~OpenWithController() override;

  // Applications that open every one of `paths` (local regular files), the
  // most preferred first, as {id, name, iconName, isDefault} maps; the
  // default of the first file's type is marked. Folders, remote paths and
  // application rows have none.
  Q_INVOKABLE QVariantList candidatesFor(const QStringList &paths);
  // Every application the Applications place lists, A to Z, as
  // {id, name, iconName} maps, for Other Application....
  Q_INVOKABLE QVariantList allApplications();
  // The MIME type all of `paths` share, or empty when they differ.
  Q_INVOKABLE QString mimeTypeFor(const QStringList &paths) const;
  // "PNG image" for "image/png"; empty for an unknown type.
  Q_INVOKABLE QString mimeDescription(const QString &mimeType) const;
  // Opens `paths` with the application `desktopId` ("x.desktop").
  Q_INVOKABLE bool openWith(const QString &desktopId, const QStringList &paths);
  // Always Open With: makes `desktopId` the default for `mimeType`. True only
  // after the store wrote it and a fresh lookup confirms it now resolves.
  Q_INVOKABLE bool setDefault(const QString &mimeType, const QString &desktopId);
  Q_INVOKABLE void clearLastError();

  [[nodiscard]] QString lastError() const { return m_lastError; }

signals:
  void lastErrorChanged();

private:
  void rescan();
  void setLastError(const QString &message);
  [[nodiscard]] const QindaQt::ApplicationCatalog::ScannedApplication *
  application(const QString &desktopId) const;

  Scanner m_handlers;
  ApplicationsSource m_applications;
  std::unique_ptr<SettingsDefaultApps::DefaultApplicationsStore> m_store;
  OpenWithLauncher m_launcher;
  QindaQt::ApplicationCatalog::DirectoryScan m_scan;
  QString m_lastError;
};

} // namespace QindaQt::Apps::FileManager
