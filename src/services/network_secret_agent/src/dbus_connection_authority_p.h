// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/connection_authority.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Network::SecretAgent {

class DbusConnectionAuthority final : public ConnectionAuthority {
public:
  explicit DbusConnectionAuthority(const QDBusConnection &connection);

  [[nodiscard]] bool isKnownConnection(const QString &path,
                                       const QString &owner) override;

private:
  QDBusConnection m_connection;
};

} // namespace QindaQt::Network::SecretAgent
