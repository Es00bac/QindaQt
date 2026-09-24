// SPDX-License-Identifier: GPL-3.0-or-later
#include "default_applications_route_composition.h"

#include <qindaqt/apps/settings_default_apps/default_applications_settings_model.h>
#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>

namespace QindaQt::Apps::SettingsDefaultApps {
namespace {
QStringList resolveApplicationDataRoots() {
  // ADR-0164: the composition root resolves the XDG data roots; the scanner
  // and catalog never read the environment themselves. Matches the file
  // manager's ApplicationsController resolution exactly.
  QStringList roots;
  for (const auto &location :
       QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
    if (!roots.contains(location)) roots.append(location);
  }
  return roots;
}
} // namespace

class DefaultApplicationsRouteComposition::Private final {
public:
  Private() : Private(resolveApplicationDataRoots()) {}

  explicit Private(const QStringList &roots)
      : dataRoots(roots), scan(QindaQt::ApplicationCatalog::scanApplicationDirectories(
            roots, QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay)),
        model(makeStore(roots), scan) {
    // AGENT-CONTRACT: the route composition owns discovery. The store and
    // model receive the same fresh public scan after install/remove/edit.
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
                     &refreshTimer, qOverload<>(&QTimer::start));
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged,
                     &refreshTimer, qOverload<>(&QTimer::start));
    QObject::connect(&refreshTimer, &QTimer::timeout, &model, [this] { refresh(); });
    fallbackTimer.setInterval(15000);
    QObject::connect(&fallbackTimer, &QTimer::timeout, &model, [this] { refresh(); });
    updateWatchedPaths();
    fallbackTimer.start();
  }

  std::unique_ptr<DefaultApplicationsStore> makeStore(const QStringList &scanRoots) {
    // ADR-0269: one composition, shared with File Manager's Open With.
    return createSessionDefaultApplicationsStore(scanRoots, scan, &mimeLookupPaths);
  }

  void refresh() {
    scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(
        dataRoots, QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay);
    model.setApplications(scan);
    updateWatchedPaths();
  }

  void updateWatchedPaths() {
    QSet<QString> wanted;
    for (const QString &root : dataRoots) {
      if (QFileInfo(root).isDir()) wanted.insert(root);
      const QString applications = QDir(root).filePath(QStringLiteral("applications"));
      if (!QFileInfo(applications).isDir()) continue;
      wanted.insert(applications);
    }
    // Bound watch count to validated scanner entries. The periodic fallback
    // discovers a new nested directory that had no retained application yet.
    for (const auto &application : scan.applications) {
      if (!QFileInfo::exists(application.desktopFilePath)) continue;
      wanted.insert(application.desktopFilePath);
      wanted.insert(QFileInfo(application.desktopFilePath).absolutePath());
    }
    // Changes to MIME policy also change both candidate eligibility and
    // effective defaults, even when no desktop entry was installed.
    for (const QString &path : mimeLookupPaths) {
      if (QFileInfo::exists(path)) wanted.insert(path);
      const QString directory = QFileInfo(path).absolutePath();
      if (QFileInfo(directory).isDir()) wanted.insert(directory);
    }
    const QStringList watched = watcher.directories() + watcher.files();
    for (const QString &path : watched)
      if (!wanted.contains(path)) watcher.removePath(path);
    for (const QString &path : wanted)
      if (!watched.contains(path)) watcher.addPath(path);
  }

  QStringList dataRoots;
  QStringList mimeLookupPaths;
  QindaQt::ApplicationCatalog::DirectoryScan scan;
  QFileSystemWatcher watcher;
  QTimer refreshTimer;
  QTimer fallbackTimer;
  DefaultApplicationsSettingsModel model;
};

DefaultApplicationsRouteComposition::DefaultApplicationsRouteComposition(
    QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

DefaultApplicationsRouteComposition::~DefaultApplicationsRouteComposition() = default;

QObject *DefaultApplicationsRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsDefaultApps
