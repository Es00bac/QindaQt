// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>

#include "dbus_connection_authority_p.h"
#include "secret_agent_object_p.h"

#include <qindaqt/services/network_secret_agent/secret_agent_controller.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Network::SecretAgent {

ResidentSecretAgent::ResidentSecretAgent(
    PromptPort &prompt, const QDBusConnection &systemConnection,
    const QDBusConnection &presenceConnection,
    const int promptTimeoutMilliseconds, QObject *parent)
    : QObject(parent), m_prompt(prompt), m_systemConnection(systemConnection),
      m_presenceConnection(presenceConnection) {
  m_authority = std::make_unique<DbusConnectionAuthority>(m_systemConnection);
  m_controller = std::make_unique<SecretAgentController>(
      m_prompt, *m_authority, promptTimeoutMilliseconds);
  m_object = std::make_unique<SecretAgentObject>(
      *m_controller, m_systemConnection,
      [this] { return m_networkManagerOwner; });
}

ResidentSecretAgent::~ResidentSecretAgent() { stop(); }

ResidentStartStatus ResidentSecretAgent::start() {
  if (m_objectRegistered) {
    return ResidentStartStatus::Started;
  }
  if (!m_systemConnection.isConnected() ||
      m_systemConnection.interface() == nullptr) {
    return ResidentStartStatus::InvalidSystemConnection;
  }
  if (!m_presenceConnection.isConnected() ||
      m_presenceConnection.interface() == nullptr) {
    return ResidentStartStatus::InvalidPresenceConnection;
  }
  if (!m_systemConnection.registerObject(
          QString::fromLatin1(kSecretAgentPath), m_object.get(),
          QDBusConnection::ExportScriptableSlots)) {
    return ResidentStartStatus::ObjectRegistrationFailed;
  }
  m_objectRegistered = true;
  m_watcher = std::make_unique<QDBusServiceWatcher>(
      QString::fromLatin1(kNetworkManagerService), m_systemConnection,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &oldOwner,
                 const QString &newOwner) {
            handleOwnerChanged(oldOwner, newOwner);
          });
  const QDBusReply<QString> owner =
      m_systemConnection.interface()->serviceOwner(
          QString::fromLatin1(kNetworkManagerService));
  if (owner.isValid() && !owner.value().isEmpty()) {
    registerWithOwner(owner.value());
  }
  return ResidentStartStatus::Started;
}

void ResidentSecretAgent::stop() {
  m_watcher.reset();
  m_controller->cancelAll();
  releaseRegistration(true);
  m_networkManagerOwner.clear();
  if (m_objectRegistered) {
    m_systemConnection.unregisterObject(QString::fromLatin1(kSecretAgentPath));
    m_objectRegistered = false;
  }
}

bool ResidentSecretAgent::isRegistered() const noexcept {
  return m_agentRegistered && m_presenceRegistered;
}

const QString &ResidentSecretAgent::networkManagerOwner() const noexcept {
  return m_networkManagerOwner;
}

void ResidentSecretAgent::handleOwnerChanged(const QString &oldOwner,
                                             const QString &newOwner) {
  Q_UNUSED(oldOwner)
  // AGENT-GUARD: Cancel before accepting the replacement owner. Otherwise a
  // prompt created by owner A could dispatch its secret into owner B.
  m_controller->cancelAll();
  releaseRegistration(false);
  m_networkManagerOwner.clear();
  if (!newOwner.isEmpty()) {
    registerWithOwner(newOwner);
  }
}

void ResidentSecretAgent::registerWithOwner(const QString &owner) {
  if (owner.isEmpty() || !m_objectRegistered) {
    return;
  }
  m_networkManagerOwner = owner;
  if (!callAgentManager(owner, QStringLiteral("Register"),
                        {QString::fromLatin1(kAgentIdentifier)})) {
    m_networkManagerOwner.clear();
    return;
  }
  m_agentRegistered = true;
  m_presenceRegistered = m_presenceConnection.registerService(
      QString::fromLatin1(kPresenceService));
  if (!m_presenceRegistered) {
    (void)callAgentManager(owner, QStringLiteral("Unregister"));
    m_agentRegistered = false;
    m_networkManagerOwner.clear();
    return;
  }
  Q_EMIT registrationChanged(true);
}

void ResidentSecretAgent::releaseRegistration(const bool notifyManager) {
  const bool wasRegistered = isRegistered();
  if (m_presenceRegistered) {
    m_presenceConnection.unregisterService(
        QString::fromLatin1(kPresenceService));
    m_presenceRegistered = false;
  }
  if (m_agentRegistered && notifyManager && !m_networkManagerOwner.isEmpty()) {
    (void)callAgentManager(m_networkManagerOwner, QStringLiteral("Unregister"));
  }
  m_agentRegistered = false;
  if (wasRegistered) {
    Q_EMIT registrationChanged(false);
  }
}

bool ResidentSecretAgent::callAgentManager(const QString &owner,
                                           const QString &method,
                                           const QVariantList &arguments) {
  if (owner.isEmpty() || m_systemConnection.interface() == nullptr ||
      m_systemConnection.interface()
              ->serviceOwner(QString::fromLatin1(kNetworkManagerService))
              .value() != owner) {
    return false;
  }
  QDBusMessage request = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(kAgentManagerPath),
      QString::fromLatin1(kAgentManagerInterface), method);
  request.setArguments(arguments);
  const QDBusMessage reply =
      m_systemConnection.call(request, QDBus::Block, 2'000);
  return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace QindaQt::Network::SecretAgent
