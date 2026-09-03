// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbus_connection_authority_p.h"

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusReply>

namespace QindaQt::Network::SecretAgent {
namespace {

constexpr qsizetype kMaximumKnownConnections = 4'096;

} // namespace

DbusConnectionAuthority::DbusConnectionAuthority(
    const QDBusConnection &connection)
    : m_connection(connection) {}

bool DbusConnectionAuthority::isKnownConnection(const QString &path,
                                                const QString &owner) {
  if (!m_connection.isConnected() || owner.isEmpty() || path.isEmpty() ||
      m_connection.interface() == nullptr ||
      m_connection.interface()
              ->serviceOwner(QString::fromLatin1(kNetworkManagerService))
              .value() != owner) {
    return false;
  }
  QDBusMessage request = QDBusMessage::createMethodCall(
      owner, QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
      QStringLiteral("org.freedesktop.NetworkManager.Settings"),
      QStringLiteral("ListConnections"));
  const QDBusReply<QList<QDBusObjectPath>> reply =
      m_connection.call(request, QDBus::Block, 2'000);
  if (!reply.isValid() || reply.value().size() > kMaximumKnownConnections) {
    return false;
  }
  for (const QDBusObjectPath &candidate : reply.value()) {
    if (candidate.path() == path) {
      return true;
    }
  }
  return false;
}

} // namespace QindaQt::Network::SecretAgent
