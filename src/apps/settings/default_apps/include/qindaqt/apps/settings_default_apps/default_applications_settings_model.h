// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <memory>

namespace QindaQt::Apps::SettingsDefaultApps {

// Route-owned projection over one injected local-file store and one already-
// completed installed-application scan. QML receives copied row data only:
// a category id/label, the current default's id/name (empty when unset),
// and its candidate application list. AGENT-CONTRACT: this model never
// re-scans the filesystem itself (the composition root owns the one scan,
// ADR-0164's split of resolution from scanning) and never shells out to
// xdg-mime/xdg-settings; every mutation is a direct, reparsed, synced write
// to the same mimeapps.list group those tools read.
class DefaultApplicationsSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
  Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
  DefaultApplicationsSettingsModel(
      std::unique_ptr<DefaultApplicationsStore> store,
      QindaQt::ApplicationCatalog::DirectoryScan scan, QObject *parent = nullptr);

  [[nodiscard]] QVariantList rows() const;
  [[nodiscard]] bool loadFailed() const noexcept { return m_loadFailed; }
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }

  Q_INVOKABLE bool setDefaultApplication(const QString &categoryId,
                                         const QString &desktopId);
  Q_INVOKABLE bool retry();

Q_SIGNALS:
  void changed();

private:
  void reload();

  std::unique_ptr<DefaultApplicationsStore> m_store;
  QindaQt::ApplicationCatalog::DirectoryScan m_scan;
  DefaultApplicationPreferences m_preferences;
  bool m_loadFailed = false;
  QString m_errorText;
};

} // namespace QindaQt::Apps::SettingsDefaultApps
