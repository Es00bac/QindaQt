// SPDX-License-Identifier: GPL-3.0-or-later

#include "private_network_manager_bus.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusMetaType>

namespace QindaQt::Network::SecretAgent::Testing {

void FakeAgentManager::Register(const QString &identifier) {
  Q_EMIT registered(identifier, message().service());
}

void FakeAgentManager::Unregister() {
  Q_EMIT unregistered(message().service());
}

QList<QDBusObjectPath> FakeSettings::ListConnections() const {
  return {QDBusObjectPath(QString::fromLatin1(kKnownConnectionPath))};
}

bool FakePrompt::showPrompt(const PromptRequest &request,
                            Completion completion) {
  requests.append(request);
  completions.insert(request.requestId, std::move(completion));
  return true;
}

void FakePrompt::cancelPrompt(const quint64 requestId) {
  canceled.append(requestId);
  completions.remove(requestId);
}

void FakePrompt::submit(const bool remember) {
  const PromptRequest request = requests.last();
  auto completion = completions.take(request.requestId);
  completion(
      {request.requestId,
       true,
       remember,
       {{request.fields.first().key, QByteArray("private-bus-canary")}}});
}

NmSettingsMap connectionMap() {
  return {{QStringLiteral("connection"),
           {{QStringLiteral("id"), QStringLiteral("Private Bus Wi-Fi")},
            {QStringLiteral("uuid"),
             QStringLiteral("12345678-1234-4234-9234-123456789abc")}}},
          {QStringLiteral("802-11-wireless-security"),
           {{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")}}}};
}

QDBusMessage getSecretsMessage(const QString &destination, const QString &path,
                               const quint32 flags,
                               const NmSettingsMap &extra) {
  NmSettingsMap settings = connectionMap();
  for (auto section = extra.cbegin(); section != extra.cend(); ++section) {
    settings[section.key()].insert(section.value());
  }
  QDBusMessage message = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface), QStringLiteral("GetSecrets"));
  message.setArguments({QVariant::fromValue(settings), QDBusObjectPath(path),
                        QStringLiteral("802-11-wireless-security"),
                        QStringList{QStringLiteral("psk")}, flags});
  return message;
}

QDBusMessage cancelGetSecretsMessage(const QString &destination,
                                     const QString &path) {
  QDBusMessage cancel = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface),
      QStringLiteral("CancelGetSecrets"));
  cancel.setArguments(
      {QDBusObjectPath(path), QStringLiteral("802-11-wireless-security")});
  return cancel;
}

QDBusMessage storageMessage(const QString &destination, const QString &method) {
  NmSettingsMap connection = connectionMap();
  connection[QStringLiteral("802-11-wireless-security")]
            [QStringLiteral("psk")] =
                QString::fromUtf8("private-bus-storage-canary");
  QDBusMessage message = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface), method);
  message.setArguments(
      {QVariant::fromValue(connection),
       QDBusObjectPath(QString::fromLatin1(kKnownConnectionPath))});
  return message;
}

std::unique_ptr<QDBusPendingCallWatcher> watch(const QDBusConnection &caller,
                                               const QDBusMessage &message) {
  return std::make_unique<QDBusPendingCallWatcher>(caller.asyncCall(message));
}

PrivateNetworkManagerBus::~PrivateNetworkManagerBus() { (void)stop(); }

QString PrivateNetworkManagerBus::start() {
  registerSecretAgentDBusTypes();
  const QString suffix = QString::number(QCoreApplication::applicationPid());
  m_bus.setProgram(QStringLiteral("dbus-daemon"));
  m_bus.setArguments(
      {QStringLiteral("--session"), QStringLiteral("--nofork"),
       QStringLiteral("--nopidfile"),
       QStringLiteral("--address=unix:abstract=qindaqt-network-secret-agent-") +
           suffix,
       QStringLiteral("--print-address=1")});
  m_bus.start();
  if (!m_bus.waitForStarted(5'000) || !m_bus.waitForReadyRead(5'000)) {
    return QStringLiteral("private dbus-daemon did not start");
  }
  m_address = QString::fromUtf8(m_bus.readLine()).trimmed();
  if (m_address.isEmpty()) {
    return QStringLiteral("private dbus-daemon printed no address");
  }

  const auto connect = [this, &suffix](const QString &role) {
    return std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
        m_address, QStringLiteral("qindaqt-test-") + role + suffix));
  };
  m_manager = connect(QStringLiteral("nm-"));
  m_agent = connect(QStringLiteral("agent-"));
  m_presence = connect(QStringLiteral("presence-"));
  m_foreign = connect(QStringLiteral("foreign-"));
  if (!m_manager->isConnected() || !m_agent->isConnected() ||
      !m_presence->isConnected() || !m_foreign->isConnected()) {
    return QStringLiteral("private bus connection failed");
  }

  // AGENT-NOTE: The fakes answer from their own thread because an in-process
  // ResidentSecretAgent makes blocking calls to them during registration.
  m_agentManager = std::make_unique<FakeAgentManager>();
  m_settings = std::make_unique<FakeSettings>();
  m_agentManager->moveToThread(&m_managerThread);
  m_settings->moveToThread(&m_managerThread);
  m_managerThread.start();
  if (!m_manager->registerObject(QString::fromLatin1(kAgentManagerPath),
                                 m_agentManager.get(),
                                 QDBusConnection::ExportAllSlots) ||
      !m_manager->registerObject(
          QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
          m_settings.get(), QDBusConnection::ExportAllSlots) ||
      !m_manager->registerService(
          QString::fromLatin1(kNetworkManagerService))) {
    return QStringLiteral("fake NetworkManager owner registration failed");
  }
  return {};
}

QString PrivateNetworkManagerBus::stop() {
  QString failure;
  if (m_manager != nullptr) {
    m_manager->unregisterService(QString::fromLatin1(kNetworkManagerService));
    m_manager->unregisterObject(QString::fromLatin1(kAgentManagerPath));
    m_manager->unregisterObject(
        QStringLiteral("/org/freedesktop/NetworkManager/Settings"));
    const QStringList names{m_manager->name(), m_agent->name(),
                            m_presence->name(), m_foreign->name()};
    m_manager.reset();
    m_agent.reset();
    m_presence.reset();
    m_foreign.reset();
    for (const QString &name : names) {
      QDBusConnection::disconnectFromBus(name);
    }
  }
  if (m_managerThread.isRunning()) {
    m_managerThread.quit();
    if (!m_managerThread.wait(5'000)) {
      failure = QStringLiteral("fake NetworkManager thread did not stop");
    }
  }
  m_agentManager.reset();
  m_settings.reset();
  if (m_bus.state() != QProcess::NotRunning) {
    m_bus.terminate();
    if (!m_bus.waitForFinished(5'000)) {
      m_bus.kill();
      m_bus.waitForFinished(5'000);
    }
  }
  return failure;
}

} // namespace QindaQt::Network::SecretAgent::Testing
