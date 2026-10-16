// SPDX-License-Identifier: GPL-3.0-or-later
#include "greeter_log.h"
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
namespace {
// AGENT-CONTRACT: every exit path states its reason. The compositor keeps the
// screen locked and black when this process ends, so a silent exit is a black
// screen nobody can diagnose (stability round 2026-10-16).
int refuse(const char *reason) {
  qCCritical(lcGreeter, "refusing to start (exit 2): %s", reason);
  return 2;
}
} // namespace
int main(int argc, char **argv) {
  // AGENT-GUARD: the compositor launches this fixed executable with no CLI
  // policy/import/configuration choices. Hostile self-launch has no privileged
  // global because only the compositor-owned private connection is authorized.
  // These are security preconditions, not recoverable conditions: the checks stay
  // fail-closed and only gained their reason strings.
  if (argc != 1) return refuse("unexpected command-line arguments");
  if (getuid() != geteuid() || getgid() != getegid()) return refuse("running with changed effective identity");
  if (!LockPlatform::protectAuthority()) return refuse("could not disable core dumps and dumpability");
  if (!LockPlatform::restrictedPtracePolicy()) return refuse("kernel ptrace policy (Yama scope 1-3) is not active");
  if (!LockPlatform::trustedQtPaths()) return refuse("Qt plugin or QML paths are not root-managed");
  if (qgetenv("WAYLAND_SOCKET") != "3") return refuse("the compositor-created connection is not inherited on fd 3");
  QCoreApplication::setLibraryPaths({QLibraryInfo::path(QLibraryInfo::PluginsPath)});
  qputenv("QT_QPA_PLATFORM", "wayland");
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "qindaqt-session-lock");
  QGuiApplication app(argc, argv);
  if (app.platformName() != QStringLiteral("wayland")) return refuse("Qt did not select the Wayland platform");
  const long requested = sysconf(_SC_GETPW_R_SIZE_MAX);
  std::vector<char> buffer(static_cast<std::size_t>(requested > 0 && requested < 65536 ? requested : 65536));
  passwd account{}, *result = nullptr;
  if (getpwuid_r(getuid(), &account, buffer.data(), buffer.size(), &result) || !result || !account.pw_name)
    return refuse("the session account could not be resolved");
  LockWorkerClient::WorkerProcess worker;
  Services::SettingsClient::QtSettingsTransport transport(QDBusConnection::sessionBus());
  Services::SettingsClient::SettingsClient settings(transport,
      Session::DesktopControls::Settings1ScreensaverPreferences::scopedKeys());
  Session::DesktopControls::DesktopEntryScreensaverCatalog catalog;
  Session::DesktopControls::Settings1ScreensaverPreferences preferences(settings, catalog);
  LockGreeter::SaverPresentation saver(preferences);
  LockGreeter::NativeWindows windows(app, worker, saver, QString::fromLocal8Bit(account.pw_name));
  // Never fatal: outputs that cannot get a window yet are retried by NativeWindows.
  if (!windows.start()) return refuse("the greeter windows could not be wired");
  // Preference absence degrades to the compiled plain background; it never
  // changes locking/authentication, executable paths or QML import paths.
  preferences.refresh();
  const int code = app.exec();
  qCInfo(lcGreeter, "event loop ended normally (exit %d)", code);
  return code;
}
