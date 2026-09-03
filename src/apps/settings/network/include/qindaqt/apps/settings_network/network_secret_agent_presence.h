// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

class QDBusServiceWatcher;

namespace QindaQt::Apps::SettingsNetwork {

// Presence-only observer for the first-party agent's public session-bus name.
// It has no object path, methods, or credential payload. The injected
// connection remains owned by the caller and must share this object's thread.
class NetworkSecretAgentPresence final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)

public:
  explicit NetworkSecretAgentPresence(const QDBusConnection &connection,
                                      QObject *parent = nullptr);
  ~NetworkSecretAgentPresence() override;

  [[nodiscard]] bool registered() const noexcept;

Q_SIGNALS:
  void registeredChanged();

private:
  void setRegistered(bool registered);

  QDBusConnection m_connection;
  std::unique_ptr<QDBusServiceWatcher> m_watcher;
  bool m_registered = false;
};

} // namespace QindaQt::Apps::SettingsNetwork
