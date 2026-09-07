// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces_apps/desktop_applications.h"
#include <KIO/ApplicationLauncherJob>
#include <KService>
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
} // namespace
DesktopApplications::DesktopApplications(QObject *parent) : QObject(parent) {}
std::optional<DesktopApplication>
DesktopApplications::find(const QString &id) const {
  Q_ASSERT(thread() == QThread::currentThread());
  const auto entry = service(id);
  if (!entry)
    return std::nullopt;
  return DesktopApplication{id, entry->name(), entry->icon()};
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
