// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_windows.h"
#include "process_protection.h"
#include <QDBusConnection>
#include <QLibraryInfo>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <pwd.h>
#include <unistd.h>
#include <vector>
using namespace QindaQt;
int main(int argc, char **argv) {
  // AGENT-GUARD: the compositor launches this fixed executable with no CLI
  // policy/import/configuration choices. Hostile self-launch has no privileged
  // global because only the compositor-owned private connection is authorized.
  if (argc != 1 || getuid() != geteuid() || getgid() != getegid() ||
      !LockPlatform::protectAuthority() || !LockPlatform::restrictedPtracePolicy() ||
      !LockPlatform::trustedQtPaths() || qgetenv("WAYLAND_SOCKET") != "3") return 2;
  QCoreApplication::setLibraryPaths({QLibraryInfo::path(QLibraryInfo::PluginsPath)});
  qputenv("QT_QPA_PLATFORM", "wayland");
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "qindaqt-session-lock");
  QGuiApplication app(argc, argv);
  if (app.platformName() != QStringLiteral("wayland")) return 2;
  const long requested = sysconf(_SC_GETPW_R_SIZE_MAX);
  std::vector<char> buffer(static_cast<std::size_t>(requested > 0 && requested < 65536 ? requested : 65536));
  passwd account{}, *result = nullptr;
  if (getpwuid_r(getuid(), &account, buffer.data(), buffer.size(), &result) || !result || !account.pw_name) return 2;
  LockWorkerClient::WorkerProcess worker;
  Services::SettingsClient::QtSettingsTransport transport(QDBusConnection::sessionBus());
  Services::SettingsClient::SettingsClient settings(transport,
      Session::DesktopControls::Settings1ScreensaverPreferences::scopedKeys());
  Session::DesktopControls::DesktopEntryScreensaverCatalog catalog;
  Session::DesktopControls::Settings1ScreensaverPreferences preferences(settings, catalog);
  LockGreeter::SaverPresentation saver(preferences);
  LockGreeter::NativeWindows windows(app, worker, saver, QString::fromLocal8Bit(account.pw_name));
  if (!windows.start()) return 2;
  // Preference absence degrades to the compiled plain background; it never
  // changes locking/authentication, executable paths or QML import paths.
  preferences.refresh();
  return app.exec();
}
