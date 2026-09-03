// SPDX-License-Identifier: GPL-3.0-or-later

#include "secret_agent_object_p.h"

#include "secret_request_policy_p.h"

#include <QtCore/QScopeGuard>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Network::SecretAgent {

SecretAgentObject::SecretAgentObject(SecretAgentController &controller,
                                     const QDBusConnection &connection,
                                     OwnerProvider ownerProvider,
                                     QObject *parent)
    : QObject(parent), m_controller(controller), m_connection(connection),
      m_ownerProvider(std::move(ownerProvider)) {
  registerSecretAgentDBusTypes();
}

void SecretAgentObject::GetSecrets(NmSettingsMap connection,
                                   const QDBusObjectPath &connectionPath,
                                   const QString &settingName,
                                   const QStringList &hints,
                                   const quint32 flags) {
  const auto wipeInput = qScopeGuard([&connection] {
    wipeSettingsMap(connection);
  });
  if (!calledFromDBus()) {
    return;
  }
  const QDBusMessage call = message();
  setDelayedReply(true);
  const QString owner = m_ownerProvider();
  GetSecretsRequest request{
      call.service(), owner, connection, connectionPath.path(),
      settingName,    hints, flags};
  (void)m_controller.requestSecrets(
      request,
      [this, call](const SecretAgentResult result, SecretReply reply) mutable {
        sendCompletion(call, result, std::move(reply));
      });
}

void SecretAgentObject::CancelGetSecrets(const QDBusObjectPath &connectionPath,
                                         const QString &settingName) {
  if (!authenticated()) {
    sendErrorReply(QString::fromLatin1(kNoSecretsError),
                   QStringLiteral("request authority was not accepted"));
    return;
  }
  m_controller.cancel(connectionPath.path(), settingName);
}

void SecretAgentObject::SaveSecrets(NmSettingsMap connection,
                                    const QDBusObjectPath &connectionPath) {
  const auto wipeInput = qScopeGuard([&connection] {
    wipeSettingsMap(connection);
  });
  Q_UNUSED(connectionPath)
  acknowledgeStorageNoOp();
}

void SecretAgentObject::DeleteSecrets(NmSettingsMap connection,
                                      const QDBusObjectPath &connectionPath) {
  const auto wipeInput = qScopeGuard([&connection] {
    wipeSettingsMap(connection);
  });
  Q_UNUSED(connectionPath)
  acknowledgeStorageNoOp();
}

bool SecretAgentObject::authenticated() const {
  return calledFromDBus() && !m_ownerProvider().isEmpty() &&
         message().service() == m_ownerProvider();
}

void SecretAgentObject::sendCompletion(const QDBusMessage &call,
                                       const SecretAgentResult result,
                                       SecretReply reply) {
  if (result == SecretAgentResult::NoSecrets) {
    m_connection.send(call.createErrorReply(
        QString::fromLatin1(kNoSecretsError),
        QStringLiteral("no permitted secret is available")));
    return;
  }
  if (result == SecretAgentResult::UserCanceled) {
    m_connection.send(call.createErrorReply(
        QString::fromLatin1(kUserCanceledError),
        QStringLiteral("credential request was canceled")));
    return;
  }

  QVariantMap section;
  section.insert(QStringLiteral("name"), reply.settingName);
  for (const SecretValue &value : std::as_const(reply.values)) {
    section.insert(value.key, QString::fromUtf8(value.bytes));
    const QString flagsKey = Private::flagsKey(value.key);
    if (!flagsKey.isEmpty()) {
      section.insert(flagsKey, reply.remember ? Private::kSecretFlagNone
                                              : Private::kSecretFlagNotSaved);
    }
  }
  NmSettingsMap settings{{reply.settingName, section}};
  // QDBusConnection::send serializes the reply before returning. Clear both
  // the UTF-16 map and original byte buffers immediately after dispatch.
  m_connection.send(call.createReply(QVariant::fromValue(settings)));
  wipeSettingsMap(settings);
  reply.wipe();
}

void SecretAgentObject::acknowledgeStorageNoOp() {
  if (!authenticated()) {
    sendErrorReply(QString::fromLatin1(kNoSecretsError),
                   QStringLiteral("request authority was not accepted"));
    return;
  }
  // Success is the standard method's typed void acknowledgement. This process
  // has no backing store; NetworkManager owns values whose agent-owned bit is
  // clear and ignores these notifications safely.
}

} // namespace QindaQt::Network::SecretAgent
