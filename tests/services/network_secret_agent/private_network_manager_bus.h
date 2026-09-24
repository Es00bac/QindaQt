// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared private-bus fixture for the network secret-agent D-Bus tests. It
// starts a disposable dbus-daemon, owns fake NetworkManager AgentManager and
// Settings objects on a worker thread, and opens the agent, presence, and
// foreign caller connections. Test suites own one instance for their lifetime.

#include <qindaqt/services/network_secret_agent/prompt_port.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtCore/QHash>
#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>

#include <memory>

namespace QindaQt::Network::SecretAgent::Testing {

class FakeAgentManager final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.AgentManager")

public Q_SLOTS:
  void Register(const QString &identifier);
  void Unregister();

Q_SIGNALS:
  void registered(const QString &identifier, const QString &caller);
  void unregistered(const QString &caller);
};

class FakeSettings final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Settings")

public Q_SLOTS:
  QList<QDBusObjectPath> ListConnections() const;
};

class FakePrompt final : public PromptPort {
public:
  QList<PromptRequest> requests;
  QHash<quint64, Completion> completions;
  QList<quint64> canceled;

  bool showPrompt(const PromptRequest &request, Completion completion) override;
  void cancelPrompt(quint64 requestId) override;
  // Completes the newest prompt with the fixed "private-bus-canary" value.
  void submit(bool remember);
};

inline constexpr auto kKnownConnectionPath =
    "/org/freedesktop/NetworkManager/Settings/7";

// Minimal WPA-PSK profile: connection id/uuid and 802-11-wireless-security.
[[nodiscard]] NmSettingsMap connectionMap();

// GetSecrets for 802-11-wireless-security/psk. `extra` properties are merged
// into the base profile section by section before marshalling.
[[nodiscard]] QDBusMessage getSecretsMessage(const QString &destination,
                                             const QString &path,
                                             quint32 flags = 0x1U,
                                             const NmSettingsMap &extra = {});
[[nodiscard]] QDBusMessage cancelGetSecretsMessage(const QString &destination,
                                                   const QString &path);
[[nodiscard]] QDBusMessage storageMessage(const QString &destination,
                                          const QString &method);
[[nodiscard]] std::unique_ptr<QDBusPendingCallWatcher>
watch(const QDBusConnection &caller, const QDBusMessage &message);

class PrivateNetworkManagerBus final {
public:
  PrivateNetworkManagerBus() = default;
  ~PrivateNetworkManagerBus();
  PrivateNetworkManagerBus(const PrivateNetworkManagerBus &) = delete;
  PrivateNetworkManagerBus &
  operator=(const PrivateNetworkManagerBus &) = delete;

  // Returns an empty string on success or a diagnostic on failure.
  [[nodiscard]] QString start();
  // Idempotent. Retires the fake owner and terminates the private broker.
  [[nodiscard]] QString stop();

  [[nodiscard]] QString address() const { return m_address; }
  [[nodiscard]] QDBusConnection &manager() { return *m_manager; }
  [[nodiscard]] QDBusConnection &agent() { return *m_agent; }
  [[nodiscard]] QDBusConnection &presence() { return *m_presence; }
  [[nodiscard]] QDBusConnection &foreign() { return *m_foreign; }
  [[nodiscard]] FakeAgentManager *agentManager() {
    return m_agentManager.get();
  }
  [[nodiscard]] FakeSettings *settings() { return m_settings.get(); }

private:
  QProcess m_bus;
  QString m_address;
  QThread m_managerThread;
  std::unique_ptr<FakeAgentManager> m_agentManager;
  std::unique_ptr<FakeSettings> m_settings;
  std::unique_ptr<QDBusConnection> m_manager;
  std::unique_ptr<QDBusConnection> m_agent;
  std::unique_ptr<QDBusConnection> m_presence;
  std::unique_ptr<QDBusConnection> m_foreign;
};

} // namespace QindaQt::Network::SecretAgent::Testing
