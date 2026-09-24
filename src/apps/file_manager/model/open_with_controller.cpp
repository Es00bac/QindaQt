// SPDX-License-Identifier: GPL-3.0-or-later
#include "open_with_controller.h"

#include "applications_listing.h"

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QCollator>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

using SettingsDefaultApps::MimeTypeHandlers;

[[nodiscard]] QString withSuffix(const QString &entryId) {
  return entryId + QStringLiteral(".desktop");
}

// The store reports machine keys; the window shows sentences.
[[nodiscard]] QString describeStoreError(const QString &key) {
  if (key == QLatin1String("default-applications-not-writable")) {
    return QStringLiteral("Your file associations (mimeapps.list) cannot be written");
  }
  if (key == QLatin1String("default-applications-unknown-application")) {
    return QStringLiteral("That application is no longer installed");
  }
  if (key == QLatin1String("default-applications-invalid-mime-type")) {
    return QStringLiteral("This kind of file cannot have a default application");
  }
  return QStringLiteral("The default application could not be saved (%1)").arg(key);
}

} // namespace

OpenWithController::OpenWithController(
    Scanner handlers, ApplicationsSource applications,
    std::unique_ptr<SettingsDefaultApps::DefaultApplicationsStore> store,
    OpenWithLauncher launcher, QObject *parent)
    : QObject(parent), m_handlers(std::move(handlers)),
      m_applications(std::move(applications)), m_store(std::move(store)),
      m_launcher(std::move(launcher)) {}

OpenWithController::~OpenWithController() = default;

void OpenWithController::rescan() {
  m_scan = m_handlers ? m_handlers() : QindaQt::ApplicationCatalog::DirectoryScan{};
  if (m_store) {
    m_store->setApplications(m_scan);
  }
}

const QindaQt::ApplicationCatalog::ScannedApplication *
OpenWithController::application(const QString &desktopId) const {
  const QString entryId = desktopId.endsWith(QStringLiteral(".desktop"))
      ? desktopId.chopped(8) : desktopId;
  return m_scan.application(entryId);
}

QVariantList OpenWithController::candidatesFor(const QStringList &paths) {
  rescan();
  if (paths.isEmpty() || paths.size() > OpenWithLauncher::maximumFiles || !m_store) {
    return {};
  }
  QMimeDatabase database;
  QHash<QString, MimeTypeHandlers> handlersByType;
  QStringList shared;
  QString preferred;
  for (qsizetype index = 0; index < paths.size(); ++index) {
    const QString &path = paths.at(index);
    // Remote URLs and Applications rows are not absolute local paths.
    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.isFile()) {
      return {};
    }
    // The type's own handlers first, then its parents' (text/x-csrc opens
    // with text/plain editors), each type looked up once per request.
    const QMimeType type = database.mimeTypeForFile(path);
    QStringList ids;
    QString fileDefault;
    for (const QString &name : QStringList{type.name()} + type.allAncestors()) {
      auto found = handlersByType.find(name);
      if (found == handlersByType.end()) {
        MimeTypeHandlers handlers;
        QString error;
        if (!m_store->loadMimeTypeHandlers(name, &handlers, &error)) {
          handlers = {};
        }
        found = handlersByType.insert(name, handlers);
      }
      if (fileDefault.isEmpty()) {
        fileDefault = found->defaultDesktopId;
      }
      for (const QString &id : std::as_const(found->desktopIds)) {
        if (!ids.contains(id)) {
          ids.append(id);
        }
      }
    }
    if (index == 0) {
      shared = ids;
      preferred = fileDefault;
    } else {
      shared.erase(std::remove_if(shared.begin(), shared.end(),
                                  [&ids](const QString &id) { return !ids.contains(id); }),
                   shared.end());
    }
  }
  QVariantList candidates;
  for (const QString &id : std::as_const(shared)) {
    const auto *scanned = application(id);
    if (scanned == nullptr) {
      continue;
    }
    // ApplicationsListing's row keeps the name and icon fallback rules the
    // Applications place uses (ADR-0262), so both show an app the same way.
    const DirectoryEntry row = ApplicationsListing::row(*scanned, QString(), false);
    candidates.append(QVariantMap{{QStringLiteral("id"), id},
                                  {QStringLiteral("name"), row.name},
                                  {QStringLiteral("iconName"), row.iconName},
                                  {QStringLiteral("isDefault"), id == preferred}});
  }
  return candidates;
}

QVariantList OpenWithController::allApplications() {
  QVector<DirectoryEntry> rows = m_applications ? m_applications().entries : QVector<DirectoryEntry>{};
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::stable_sort(rows.begin(), rows.end(), [&collator](const DirectoryEntry &left,
                                                         const DirectoryEntry &right) {
    return collator.compare(left.name, right.name) < 0;
  });
  QVariantList list;
  for (const DirectoryEntry &row : std::as_const(rows)) {
    if (row.applicationId.isEmpty()) {
      continue;
    }
    list.append(QVariantMap{{QStringLiteral("id"), withSuffix(row.applicationId)},
                            {QStringLiteral("name"), row.name},
                            {QStringLiteral("iconName"), row.iconName}});
  }
  return list;
}

QString OpenWithController::mimeTypeFor(const QStringList &paths) const {
  QMimeDatabase database;
  QString shared;
  for (const QString &path : paths) {
    const QString name = database.mimeTypeForFile(path).name();
    if (!shared.isEmpty() && name != shared) {
      return {};
    }
    shared = name;
  }
  return shared;
}

QString OpenWithController::mimeDescription(const QString &mimeType) const {
  const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
  return type.isValid() ? type.comment() : QString();
}

bool OpenWithController::openWith(const QString &desktopId, const QStringList &paths) {
  // A fresh scan, so an application installed after the menu was built (or
  // picked from the Applications place's newer listing) still resolves.
  rescan();
  const auto *scanned = application(desktopId);
  if (scanned == nullptr) {
    setLastError(QStringLiteral("That application is no longer installed"));
    return false;
  }
  const LaunchResult result = m_launcher.launch(*scanned, paths);
  setLastError(result.ok() ? QString() : result.diagnostic);
  return result.ok();
}

bool OpenWithController::setDefault(const QString &mimeType, const QString &desktopId) {
  if (!m_store) {
    setLastError(QStringLiteral("File associations are unavailable"));
    return false;
  }
  rescan();
  QString error;
  if (!m_store->saveMimeTypeDefault(mimeType, desktopId, &error)) {
    setLastError(describeStoreError(error));
    return false;
  }
  // "Never report done before the authority confirms it": the store is the
  // authority, so the new default must resolve through a fresh lookup.
  MimeTypeHandlers handlers;
  const QString canonical = desktopId.endsWith(QStringLiteral(".desktop"))
      ? desktopId : withSuffix(desktopId);
  if (!m_store->loadMimeTypeHandlers(mimeType, &handlers, &error) ||
      handlers.defaultDesktopId != canonical) {
    setLastError(QStringLiteral("The choice was saved, but another setting still decides "
                                "how these files open"));
    return false;
  }
  setLastError({});
  return true;
}

void OpenWithController::clearLastError() {
  setLastError({});
}

void OpenWithController::setLastError(const QString &message) {
  if (m_lastError == message) {
    return;
  }
  m_lastError = message;
  emit lastErrorChanged();
}

} // namespace QindaQt::Apps::FileManager
