// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/prompt_port.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

class QDBusServiceWatcher;

namespace QindaQt::Network::SecretAgent {

class DbusConnectionAuthority;
class SecretAgentController;
class SecretAgentObject;

enum class ResidentStartStatus {
  Started,
  InvalidSystemConnection,
  InvalidPresenceConnection,
  ObjectRegistrationFailed,
};

// Owns registration on one injected system-bus connection and presence-name
// truth on one injected session-bus connection. NetworkManager owner changes
// cancel all work before re-registration. stop() is idempotent and unregisters
// before releasing the D-Bus object.
class ResidentSecretAgent final : public QObject {
  Q_OBJECT

public:
  explicit ResidentSecretAgent(PromptPort &prompt,
                               const QDBusConnection &systemConnection,
                               const QDBusConnection &presenceConnection,
                               int promptTimeoutMilliseconds = 120'000,
                               QObject *parent = nullptr);
  ~ResidentSecretAgent() override;

  [[nodiscard]] ResidentStartStatus start();
  void stop();
  [[nodiscard]] bool isRegistered() const noexcept;
  [[nodiscard]] const QString &networkManagerOwner() const noexcept;

Q_SIGNALS:
  void registrationChanged(bool registered);

private:
  void handleOwnerChanged(const QString &oldOwner, const QString &newOwner);
  void registerWithOwner(const QString &owner);
  void releaseRegistration(bool notifyManager);
  [[nodiscard]] bool callAgentManager(const QString &owner,
                                      const QString &method,
                                      const QVariantList &arguments = {});

  PromptPort &m_prompt;
  QDBusConnection m_systemConnection;
  QDBusConnection m_presenceConnection;
  std::unique_ptr<DbusConnectionAuthority> m_authority;
  std::unique_ptr<SecretAgentController> m_controller;
  std::unique_ptr<SecretAgentObject> m_object;
  std::unique_ptr<QDBusServiceWatcher> m_watcher;
  QString m_networkManagerOwner;
  bool m_objectRegistered = false;
  bool m_agentRegistered = false;
  bool m_presenceRegistered = false;
};

} // namespace QindaQt::Network::SecretAgent
