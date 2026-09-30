// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces_apps/desktop_applications.h"

#include <qindaqt/application_catalog/application_directory_scan.h>

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
namespace QindaQt::WorkspacesApps {
namespace {
KService::Ptr service(const QString &id) {
  if (id.isEmpty() || id.contains(QLatin1Char('/')) ||
      id.contains(QLatin1Char('\\')))
    return {};
  auto result = KService::serviceByStorageId(id);
  if (!result && !id.endsWith(QStringLiteral(".desktop")))
    result = KService::serviceByStorageId(id + QStringLiteral(".desktop"));
  // A standalone QindaQt session may have no Plasma applications menu and
  // therefore no indexed KSycoca entry yet. Honor the XDG desktop file itself.
  if (!result) {
    const auto fileName = id.endsWith(QStringLiteral(".desktop"))
                              ? id
                              : id + QStringLiteral(".desktop");
    const auto path =
        QStandardPaths::locate(QStandardPaths::ApplicationsLocation, fileName);
    if (!path.isEmpty())
      result = KService::Ptr(new KService(path));
  }
  return result && result->isValid() && result->isApplication() &&
                 !result->isDeleted()
             ? result
             : KService::Ptr{};
}
QString normalizedDesktopId(QString id) {
  id = id.trimmed();
  if (id.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive))
    id.chop(QStringLiteral(".desktop").size());
  return id;
}
} // namespace
DesktopApplications::DesktopApplications(QObject *parent) : QObject(parent) {
  const auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation));
  m_catalogApplications.reserve(scan.applications.size());
  for (const auto &application : scan.applications) {
    const qsizetype index = m_catalogApplications.size();
    m_catalogApplications.append(
        {application.entry.id, application.entry.name, application.entry.iconName});
    m_catalogByExactId.insert(application.entry.id, index);
    const QString alias = application.entry.startupWmClass.trimmed().toCaseFolded();
    if (!alias.isEmpty()) {
      auto &ids = m_idsByStartupWmClass[alias];
      if (!ids.contains(application.entry.id))
        ids.append(application.entry.id);
    }
  }
}
std::optional<DesktopApplication>
DesktopApplications::find(const QString &id) const {
  Q_ASSERT(thread() == QThread::currentThread());
  const auto entry = service(id);
  if (!entry)
    return std::nullopt;
  return DesktopApplication{id, entry->name(), entry->icon()};
}
std::optional<DesktopApplication> DesktopApplications::findForWindow(
    const QString &desktopEntryId, const QString &applicationId,
    const QString &resourceClass) const {
  Q_ASSERT(thread() == QThread::currentThread());
  const QStringList exactIds{desktopEntryId, applicationId, resourceClass};
  for (const QString &value : exactIds) {
    const QString id = normalizedDesktopId(value);
    if (id.isEmpty())
      continue;
    if (const auto entry = service(id))
      return DesktopApplication{id, entry->name(), entry->icon()};
    const auto found = m_catalogByExactId.constFind(id);
    if (found != m_catalogByExactId.cend())
      return m_catalogApplications.at(found.value());
  }

  QSet<QString> matches;
  for (const QString &value : exactIds) {
    const QString alias = normalizedDesktopId(value).toCaseFolded();
    if (alias.isEmpty())
      continue;
    const auto found = m_idsByStartupWmClass.constFind(alias);
    if (found != m_idsByStartupWmClass.cend()) {
      for (const QString &id : found.value())
        matches.insert(id);
    }
  }
  if (matches.size() != 1)
    return std::nullopt;
  const auto found = m_catalogByExactId.constFind(*matches.cbegin());
  return found == m_catalogByExactId.cend()
      ? std::nullopt
      : std::optional<DesktopApplication>(m_catalogApplications.at(found.value()));
}
bool DesktopApplications::launch(const QString &id, const QStringList &urls,
                                 const QByteArray &activationToken,
                                 QString *error) {
  Q_ASSERT(thread() == QThread::currentThread());
  if (error)
    error->clear();
  const auto entry = service(id);
  if (!entry) {
    if (error)
      *error = tr("%1 is not installed. Choose another application or window.")
                   .arg(id);
    return false;
  }
  QList<QUrl> locations;
  for (const auto &text : urls) {
    const QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.isRelative()) {
      if (error)
        *error = tr("The saved application location is invalid.");
      return false;
    }
    locations.append(url);
  }
  auto *job = new KIO::ApplicationLauncherJob(entry, this);
  job->setUrls(locations);
  job->setStartupId(activationToken);
  // AGENT-CONTRACT: KIO owns asynchronous desktop-entry expansion/startup.
  // Workspace UI must show failures and refresh live windows independently.
  connect(job, &KJob::result, this, [this, id](KJob *completed) {
    emit launchFinished(id, completed->error() == 0, completed->errorString());
  });
  job->start();
  return true;
}
} // namespace QindaQt::WorkspacesApps
