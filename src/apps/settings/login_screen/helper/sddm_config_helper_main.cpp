// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-sddm-config-helper: the only process in QindaQt that ever writes
// an SDDM configuration file. It runs exclusively through
// `pkexec qindaqt-sddm-config-helper`, gated by the
// org.qindaqt.settings.loginscreen.configure polkit action, reads one
// strict change-set payload on stdin, re-validates everything (the caller
// is untrusted), and merges the change into the route-owned drop-in --
// never /etc/sddm.conf, never another drop-in, never a key outside the
// owned set. Target paths are compile-time constants on purpose: a helper
// that takes a target directory argument is a root file-writer for hire.

#include <qindaqt/apps/settings_login_screen/sddm_discovery.h>
#include <qindaqt/apps/settings_login_screen/sddm_owned_config.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

namespace {

constexpr qint64 MaximumPayloadBytes = 16 * 1024;

} // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTextStream err(stderr);
  const QString target =
      QStringLiteral("/etc/sddm.conf.d/zzz-qindaqt-settings.conf");

  QFile input;
  if (!input.open(stdin, QIODevice::ReadOnly)) {
    err << "qindaqt-sddm-config-helper: cannot read the change payload.\n";
    return 2;
  }
  const QByteArray payload = input.read(MaximumPayloadBytes + 1);
  if (payload.size() > MaximumPayloadBytes) {
    err << "qindaqt-sddm-config-helper: the change payload is too large.\n";
    return 2;
  }

  QString parseError;
  const auto changes =
      QindaQt::Apps::SettingsLoginScreen::deserializeOwnedChangeSet(
          QString::fromUtf8(payload), &parseError);
  if (!changes.has_value()) {
    err << "qindaqt-sddm-config-helper: " << parseError << '\n';
    return 3;
  }

  // Re-scan and re-validate after elevation. The Settings process already
  // validated for the user's benefit; this pass is the security boundary.
  using QindaQt::Apps::SettingsLoginScreen::listSddmLoginUsers;
  using QindaQt::Apps::SettingsLoginScreen::listSddmSessions;
  using QindaQt::Apps::SettingsLoginScreen::listSddmThemes;
  QStringList themeIds;
  for (const auto &theme : listSddmThemes(QStringLiteral("/usr/share/sddm/themes"))) {
    themeIds.append(theme.id);
  }
  QStringList sessionIds;
  for (const auto &session : listSddmSessions(
           {QStringLiteral("/usr/local/share/wayland-sessions"),
            QStringLiteral("/usr/share/wayland-sessions")},
           {QStringLiteral("/usr/local/share/xsessions"),
            QStringLiteral("/usr/share/xsessions")})) {
    sessionIds.append(session.id);
  }
  const QString invalid = QindaQt::Apps::SettingsLoginScreen::
      validateOwnedChangeSet(*changes, themeIds, sessionIds,
                             listSddmLoginUsers(QStringLiteral("/etc/passwd")));
  if (!invalid.isEmpty()) {
    err << "qindaqt-sddm-config-helper: " << invalid << '\n';
    return 4;
  }

  // The drop-in directory may not exist yet on a system nothing has ever
  // configured; creating it (0755) is this helper's one structural write.
  const QDir targetDir = QFileInfo(target).absoluteDir();
  if (!targetDir.exists() && !QDir().mkpath(targetDir.absolutePath())) {
    err << "qindaqt-sddm-config-helper: cannot create "
        << targetDir.absolutePath() << ".\n";
    return 5;
  }
  QString writeError;
  if (!QindaQt::Apps::SettingsLoginScreen::writeOwnedChangeSetToFile(
          target, *changes, &writeError)) {
    err << "qindaqt-sddm-config-helper: " << writeError << '\n';
    return 6;
  }
  return 0;
}
