// SPDX-License-Identifier: GPL-3.0-or-later

#include "color_route_composition.h"

#include <qindaqt/apps/settings_color/color_settings_model.h>
#include <qindaqt/services/display_client/qt_display_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>

#include <QtDBus/QDBusConnection>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

namespace QindaQt::Apps::SettingsColor {
namespace {

using QindaQt::DisplayColor::DiscoveryOrigin;
using QindaQt::DisplayColor::DiscoveryRoot;

// AGENT-CONTRACT: Production discovery roots (see the class contract). The
// per-user ICC directory is the import destination and the only UserImported
// root; every remaining standard data location contributes a System root.
// Tests redirect the XDG locations, so no host profile directory is read.
QList<DiscoveryRoot> productionDiscoveryRoots() {
  const QString writable = QStandardPaths::writableLocation(
      QStandardPaths::GenericDataLocation);
  QList<DiscoveryRoot> roots;
  for (const QString &location : QStandardPaths::standardLocations(
           QStandardPaths::GenericDataLocation)) {
    roots.append({location + QStringLiteral("/color/icc"),
                  location == writable ? DiscoveryOrigin::UserImported
                                       : DiscoveryOrigin::System});
  }
  return roots;
}

// AGENT-CONTRACT: The C1 import writer fails closed on a missing user root
// (ImportRootAccess::open requires an existing, EUID-owned,
// non-group/other-writable directory), so the composition must provision the
// UserImported root before the model can accept an import on a fresh XDG
// home. Only a root this process creates is tightened to mode 0700; an
// existing directory keeps the user's own permissions and stays subject to
// the writer's fail-closed validation. Provisioning failure is logged and
// left to the writer's rejection rather than guessed around.
void provisionUserImportRoot(const QList<DiscoveryRoot> &roots) {
  for (const DiscoveryRoot &root : roots) {
    if (root.origin != DiscoveryOrigin::UserImported) {
      continue;
    }
    if (QFileInfo::exists(root.path)) {
      return;
    }
    if (!QDir().mkpath(root.path)) {
      qWarning("qindaqt-settings: could not create the user ICC profile "
               "directory %s; imports will be refused",
               qPrintable(root.path));
      return;
    }
    if (!QFile::setPermissions(root.path, QFileDevice::ReadUser |
                                              QFileDevice::WriteUser |
                                              QFileDevice::ExeUser)) {
      qWarning("qindaqt-settings: could not restrict the user ICC profile "
               "directory %s to mode 0700; imports will be refused",
               qPrintable(root.path));
    }
    return;
  }
}

} // namespace

class ColorRouteComposition::Private final {
public:
  Private()
      : displayTransport(QDBusConnection::sessionBus()),
        displayClient(&displayTransport),
        settingsTransport(QDBusConnection::sessionBus()),
        settingsClient(
            settingsTransport,
            {QLatin1String(QindaQt::DisplayColor::ColorAssignmentsSettingsKey)}),
        store(settingsClient), discovery(productionDiscoveryRoots()),
        model(displayClient, settingsClient, store, discovery) {
    provisionUserImportRoot(discovery.roots());
    displayClient.start();
    QString error;
    if (!settingsClient.start(&error)) {
      qWarning("qindaqt-settings: color Settings1 client unavailable: %s",
               qPrintable(error));
    }
  }

  ~Private() {
    settingsClient.stop();
    displayClient.stop();
  }

  QindaQt::DisplayClient::QtDisplayTransport displayTransport;
  QindaQt::DisplayClient::Client displayClient;
  QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport;
  QindaQt::Services::SettingsClient::SettingsClient settingsClient;
  QindaQt::DisplayColor::SettingsAssignmentStore store;
  QindaQt::DisplayColor::ProfileDiscovery discovery;
  ColorSettingsModel model;
};

ColorRouteComposition::ColorRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

ColorRouteComposition::~ColorRouteComposition() = default;

QObject *ColorRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsColor
