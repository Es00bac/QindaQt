// SPDX-License-Identifier: GPL-3.0-or-later
#include "default_applications_route_composition.h"

#include <qindaqt/apps/settings_default_apps/default_applications_settings_model.h>
#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtCore/QDir>
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

QString resolveMimeAppsListPath() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
      .filePath(QStringLiteral("mimeapps.list"));
}
} // namespace

class DefaultApplicationsRouteComposition::Private final {
public:
  Private()
      : model(std::make_unique<MimeAppsDefaultApplicationsStore>(
                  resolveMimeAppsListPath()),
              QindaQt::ApplicationCatalog::scanApplicationDirectories(
                  resolveApplicationDataRoots())) {}

  DefaultApplicationsSettingsModel model;
};

DefaultApplicationsRouteComposition::DefaultApplicationsRouteComposition(
    QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

DefaultApplicationsRouteComposition::~DefaultApplicationsRouteComposition() = default;

QObject *DefaultApplicationsRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsDefaultApps
