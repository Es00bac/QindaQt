// SPDX-License-Identifier: LGPL-3.0-or-later
#include "polkit_authority_probe.h"

#include <QtCore/QFile>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Apps::SettingsLoginScreen {

PolkitAuthorityProbe::PolkitAuthorityProbe(QDBusConnection systemBus,
                                           QString actionId,
                                           QString pkexecPath,
                                           QString helperPath,
                                           QObject *parent)
    : LoginScreenAuthorityProbe(parent),
      m_bus(std::move(systemBus)),
      m_actionId(std::move(actionId)),
      m_pkexecPath(std::move(pkexecPath)),
      m_helperPath(std::move(helperPath)) {}

void PolkitAuthorityProbe::probe() {
  if (m_pkexecPath.isEmpty()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "pkexec is not available on this system, so the login screen can "
        "only be viewed here, not changed."));
    return;
  }
  if (m_helperPath.isEmpty() || !QFile::exists(m_helperPath)) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The privileged helper for login screen settings is not installed, "
        "so this page can only show the current configuration."));
    return;
  }
  if (!m_bus.isConnected()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The system authorization service cannot be reached, so the login "
        "screen can only be viewed here, not changed."));
    return;
  }

  QDBusInterface authority(QStringLiteral("org.freedesktop.PolicyKit1"),
                           QStringLiteral("/org/freedesktop/PolicyKit1/Authority"),
                           QStringLiteral("org.freedesktop.PolicyKit1.Authority"),
                           m_bus, this);
  // Flags 0: no user interaction from the probe itself. The answer's
  // challenge bit tells us a prompt WOULD succeed, which keeps the
  // controls writable -- the prompt then happens at save time, exactly
  // once, where the user expects it.
  const QVariantMap details;
  auto *watcher = new QDBusPendingCallWatcher(
      authority.asyncCall(QStringLiteral("CheckAuthorization"), m_actionId,
                          details, uint(0), QString()),
      this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          &PolkitAuthorityProbe::onReply);
}

void PolkitAuthorityProbe::onReply(QDBusPendingCallWatcher *watcher) {
  const QDBusMessage reply = watcher->reply();
  watcher->deleteLater();
  if (reply.type() == QDBusMessage::ErrorMessage) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The system does not know the login screen action (%1). The "
        "QindaQt polkit policy may not be installed, so this page can only "
        "show the current configuration.")
        .arg(reply.errorMessage()));
    return;
  }
  const QList<QVariant> arguments = reply.arguments();
  if (arguments.isEmpty()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The authorization service answered unexpectedly; treating the "
        "login screen as read-only."));
    return;
  }
  const QDBusArgument result = arguments.first().value<QDBusArgument>();
  bool authorized = false;
  bool challenge = false;
  result.beginStructure();
  result >> authorized >> challenge;
  result.endStructure();
  if (authorized || challenge) {
    Q_EMIT probeFinished(true, QString());
    return;
  }
  Q_EMIT probeFinished(false, QStringLiteral(
      "You are not allowed to administer the login screen on this machine, "
      "so this page can only show the current configuration."));
}

} // namespace QindaQt::Apps::SettingsLoginScreen
