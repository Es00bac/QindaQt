// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_network/network_secret_agent_presence.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Apps::SettingsNetwork {
namespace {

constexpr auto kPresenceService = "org.qindaqt.NetworkSecretAgent1";

} // namespace

NetworkSecretAgentPresence::NetworkSecretAgentPresence(
    const QDBusConnection &connection, QObject *parent)
    : QObject(parent), m_connection(connection) {
  if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
    return;
  }
  m_watcher = std::make_unique<QDBusServiceWatcher>(
      QString::fromLatin1(kPresenceService), m_connection,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &newOwner) {
            setRegistered(!newOwner.isEmpty());
          });
  const QDBusReply<bool> registered =
      m_connection.interface()->isServiceRegistered(
          QString::fromLatin1(kPresenceService));
  setRegistered(registered.isValid() && registered.value());
}

NetworkSecretAgentPresence::~NetworkSecretAgentPresence() = default;

bool NetworkSecretAgentPresence::registered() const noexcept {
  return m_registered;
}

void NetworkSecretAgentPresence::setRegistered(const bool registered) {
  if (m_registered == registered) {
    return;
  }
  m_registered = registered;
  Q_EMIT registeredChanged();
}

} // namespace QindaQt::Apps::SettingsNetwork
