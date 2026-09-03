// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/secret_agent_controller.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusObjectPath>

#include <functional>

namespace QindaQt::Network::SecretAgent {

class SecretAgentObject final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.SecretAgent")
  Q_CLASSINFO(
      "D-Bus Introspection",
      "<interface name=\"org.freedesktop.NetworkManager.SecretAgent\">"
      "<method name=\"GetSecrets\"><arg name=\"connection\" "
      "type=\"a{sa{sv}}\" direction=\"in\"/><arg name=\"connection_path\" "
      "type=\"o\" direction=\"in\"/><arg name=\"setting_name\" type=\"s\" "
      "direction=\"in\"/><arg name=\"hints\" type=\"as\" direction=\"in\"/>"
      "<arg name=\"flags\" type=\"u\" direction=\"in\"/><arg "
      "name=\"secrets\" type=\"a{sa{sv}}\" direction=\"out\"/></method>"
      "<method name=\"CancelGetSecrets\"><arg name=\"connection_path\" "
      "type=\"o\" direction=\"in\"/><arg name=\"setting_name\" type=\"s\" "
      "direction=\"in\"/></method><method name=\"SaveSecrets\"><arg "
      "name=\"connection\" type=\"a{sa{sv}}\" direction=\"in\"/><arg "
      "name=\"connection_path\" type=\"o\" direction=\"in\"/></method>"
      "<method name=\"DeleteSecrets\"><arg name=\"connection\" "
      "type=\"a{sa{sv}}\" direction=\"in\"/><arg name=\"connection_path\" "
      "type=\"o\" direction=\"in\"/></method></interface>")

public:
  using OwnerProvider = std::function<QString()>;

  SecretAgentObject(SecretAgentController &controller,
                    const QDBusConnection &connection,
                    OwnerProvider ownerProvider, QObject *parent = nullptr);

public Q_SLOTS:
  Q_SCRIPTABLE void GetSecrets(const NmSettingsMap &connection,
                               const QDBusObjectPath &connectionPath,
                               const QString &settingName,
                               const QStringList &hints, quint32 flags);
  Q_SCRIPTABLE void CancelGetSecrets(const QDBusObjectPath &connectionPath,
                                     const QString &settingName);
  Q_SCRIPTABLE void SaveSecrets(const NmSettingsMap &connection,
                                const QDBusObjectPath &connectionPath);
  Q_SCRIPTABLE void DeleteSecrets(const NmSettingsMap &connection,
                                  const QDBusObjectPath &connectionPath);

private:
  [[nodiscard]] bool authenticated() const;
  void sendCompletion(const QDBusMessage &call, SecretAgentResult result,
                      SecretReply reply);
  void acknowledgeStorageNoOp();

  SecretAgentController &m_controller;
  QDBusConnection m_connection;
  OwnerProvider m_ownerProvider;
};

} // namespace QindaQt::Network::SecretAgent
