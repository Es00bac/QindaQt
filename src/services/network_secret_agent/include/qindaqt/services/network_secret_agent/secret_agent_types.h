// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>

namespace QindaQt::Network::SecretAgent {

inline constexpr auto kNetworkManagerService = "org.freedesktop.NetworkManager";
inline constexpr auto kAgentManagerPath =
    "/org/freedesktop/NetworkManager/AgentManager";
inline constexpr auto kAgentManagerInterface =
    "org.freedesktop.NetworkManager.AgentManager";
inline constexpr auto kSecretAgentPath =
    "/org/freedesktop/NetworkManager/SecretAgent";
inline constexpr auto kSecretAgentInterface =
    "org.freedesktop.NetworkManager.SecretAgent";
inline constexpr auto kPresenceService = "org.qindaqt.NetworkSecretAgent1";
inline constexpr auto kAgentIdentifier = "org.qindaqt.NetworkSecretAgent1";
inline constexpr auto kNoSecretsError =
    "org.freedesktop.NetworkManager.SecretAgent.Error.NoSecrets";
inline constexpr auto kUserCanceledError =
    "org.freedesktop.NetworkManager.SecretAgent.Error.UserCanceled";

using NmSettingsMap = QMap<QString, QVariantMap>;

enum class SecretAgentResult { Replied, NoSecrets, UserCanceled };
enum class StorageDisposition { NetworkManagerOwnsStorage };

struct PromptField final {
  QString key;
  QString label;
  bool concealed = true;
  qsizetype maximumLength = 256;
};

struct PromptRequest final {
  quint64 requestId = 0;
  QString connectionName;
  QString connectionPath;
  QString settingName;
  QList<PromptField> fields;
};

struct SecretValue final {
  QString key;
  QByteArray bytes;

  void wipe() noexcept;
};

struct PromptResult final {
  quint64 requestId = 0;
  bool accepted = false;
  bool remember = false;
  QList<SecretValue> values;

  void wipe() noexcept;
};

struct SecretReply final {
  QString settingName;
  bool remember = false;
  QList<SecretValue> values;

  void wipe() noexcept;
};

struct GetSecretsRequest final {
  QString caller;
  QString networkManagerOwner;
  NmSettingsMap connection;
  QString connectionPath;
  QString settingName;
  QStringList hints;
  quint32 flags = 0;
};

void registerSecretAgentDBusTypes();
// AGENT-CONTRACT: These consume mutable, dynamically owned Qt values and
// intentionally overwrite implicitly shared backing allocations. Every alias
// must be dead or another secret-bearing copy that must also be scrubbed; do
// not pass a value another live request/settings owner still needs.
// takeSecretUtf8 returns a separately owned byte buffer or empty on mismatch.
[[nodiscard]] QByteArray takeSecretUtf8(QVariant &value) noexcept;
void wipeSettingsMap(NmSettingsMap &settings) noexcept;

} // namespace QindaQt::Network::SecretAgent

Q_DECLARE_METATYPE(QindaQt::Network::SecretAgent::NmSettingsMap)
