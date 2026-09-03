// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QString>

namespace QindaQt::Network::SecretAgent {

// Implementations prove that a connection path belongs to the exact current
// NetworkManager owner. Failure and stale-owner observations return false.
class ConnectionAuthority {
public:
  virtual ~ConnectionAuthority() = default;
  [[nodiscard]] virtual bool isKnownConnection(const QString &path,
                                               const QString &owner) = 0;
};

} // namespace QindaQt::Network::SecretAgent
