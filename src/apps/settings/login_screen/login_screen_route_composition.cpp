// SPDX-License-Identifier: LGPL-3.0-or-later
#include "login_screen_route_composition.h"

#include "pkexec_sddm_config_writer.h"
#include "polkit_authority_probe.h"

#include <qindaqt/apps/settings_login_screen/login_screen_settings_model.h>

#include <QtCore/QProcessEnvironment>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsLoginScreen {
namespace {

[[nodiscard]] LoginScreenPaths productionPaths() {
  LoginScreenPaths paths;
  paths.themesDirectory = QStringLiteral("/usr/share/sddm/themes");
  paths.waylandSessionDirectories = {
      QStringLiteral("/usr/local/share/wayland-sessions"),
      QStringLiteral("/usr/share/wayland-sessions"),
  };
  paths.xSessionDirectories = {
      QStringLiteral("/usr/local/share/xsessions"),
      QStringLiteral("/usr/share/xsessions"),
  };
  paths.passwdFile = QStringLiteral("/etc/passwd");
  // Precedence exactly as sddm.conf(5) documents it: the system
  // configuration directory, then the local one, then the legacy file.
  paths.sddmConfigScanDirectories = {
      QStringLiteral("/usr/lib/sddm/sddm.conf.d"),
      QStringLiteral("/etc/sddm.conf.d"),
  };
  paths.sddmLegacyMainFile = QStringLiteral("/etc/sddm.conf");
  paths.ownedConfigFile =
      QStringLiteral("/etc/sddm.conf.d/zzz-qindaqt-settings.conf");
  return paths;
}

} // namespace

class LoginScreenRouteComposition::Private final {
public:
  Private()
      : model(productionPaths(),
              std::make_unique<PkexecSddmConfigWriter>(
                  QStandardPaths::findExecutable(QStringLiteral("pkexec")),
                  QStringLiteral(QINDAQT_SDDM_HELPER_PATH)),
              std::make_unique<PolkitAuthorityProbe>(
                  QDBusConnection::systemBus(),
                  QStringLiteral("org.qindaqt.settings.loginscreen.configure"),
                  QStandardPaths::findExecutable(QStringLiteral("pkexec")),
                  QStringLiteral(QINDAQT_SDDM_HELPER_PATH)),
              QProcessEnvironment::systemEnvironment()
                  .value(QStringLiteral("USER"))) {}

  LoginScreenSettingsModel model;
};

LoginScreenRouteComposition::LoginScreenRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

LoginScreenRouteComposition::~LoginScreenRouteComposition() = default;

QObject *LoginScreenRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsLoginScreen
