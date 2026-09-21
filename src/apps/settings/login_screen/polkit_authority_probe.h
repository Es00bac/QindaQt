// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_login_screen/login_screen_settings_model.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCallWatcher>

namespace QindaQt::Apps::SettingsLoginScreen {

// Production LoginScreenAuthorityProbe. Short-circuits on missing local
// pieces (pkexec, the helper binary), otherwise asks polkit's authority
// whether this process may use the route's action. CheckAuthorization with
// no interaction flag answers three ways: authorized (cached or implicit),
// challenge (a prompt would be offered -- still writable, the user simply
// authenticates at save time), or neither (read-only, and the page says
// why). No bus, or an unregistered action (policy not installed), comes
// back read-only with the reason spelled out.
class PolkitAuthorityProbe final : public LoginScreenAuthorityProbe {
  Q_OBJECT
public:
  PolkitAuthorityProbe(QDBusConnection systemBus, QString actionId,
                       QString pkexecPath, QString helperPath,
                       QObject *parent = nullptr);

  void probe() override;

private:
  void onReply(QDBusPendingCallWatcher *watcher);

  QDBusConnection m_bus;
  QString m_actionId;
  QString m_pkexecPath;
  QString m_helperPath;
};

} // namespace QindaQt::Apps::SettingsLoginScreen
