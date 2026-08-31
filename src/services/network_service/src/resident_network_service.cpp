// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_service/resident_network_service.h>

#include "network_service_object_p.h"

#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtDBus/QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Network::Service {

ResidentNetworkService::ResidentNetworkService(
    std::unique_ptr<NetworkBackend> backend, const QDBusConnection &connection,
    QString serviceName, const int operationTimeoutMilliseconds,
    QObject *parent)
    : QObject(parent), m_backend(std::move(backend)), m_connection(connection),
      m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                          : std::move(serviceName)) {
  Q_ASSERT(m_backend != nullptr);
  m_coordinator = std::make_unique<NetworkServiceCoordinator>(
      m_backend.get(), operationTimeoutMilliseconds);
  m_serviceObject =
      std::make_unique<NetworkServiceObject>(m_coordinator.get(), m_connection);
  connect(m_coordinator.get(), &NetworkServiceCoordinator::restartRequired,
          this, &ResidentNetworkService::restartRequired);
}

ResidentNetworkService::~ResidentNetworkService() { stop(); }

NetworkServiceStartStatus ResidentNetworkService::start() {
  if (isRunning()) {
    return NetworkServiceStartStatus::Started;
  }
  if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
    return NetworkServiceStartStatus::InvalidConnection;
  }
  if (!m_connection.registerObject(
          QString::fromLatin1(kObjectPath), m_serviceObject.get(),
          QDBusConnection::ExportScriptableSlots |
              QDBusConnection::ExportScriptableSignals)) {
    return NetworkServiceStartStatus::ObjectRegistrationFailed;
  }
  m_objectRegistered = true;
  if (!m_connection.registerService(m_serviceName)) {
    const QString owner =
        m_connection.interface()->serviceOwner(m_serviceName).value();
    stop();
    return owner.isEmpty() ? NetworkServiceStartStatus::NameRegistrationFailed
                           : NetworkServiceStartStatus::NameAlreadyOwned;
  }
  m_nameRegistered = true;
  if (!m_coordinator->start(m_connection.baseService())) {
    stop();
    return NetworkServiceStartStatus::CoordinatorStartFailed;
  }
  return NetworkServiceStartStatus::Started;
}

void ResidentNetworkService::stop() {
  if (m_coordinator != nullptr) {
    m_coordinator->stop();
  }
  if (m_nameRegistered) {
    m_connection.unregisterService(m_serviceName);
    m_nameRegistered = false;
  }
  if (m_objectRegistered) {
    m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
    m_objectRegistered = false;
  }
}

bool ResidentNetworkService::isRunning() const noexcept {
  return m_nameRegistered && m_objectRegistered && m_coordinator->isRunning();
}

NetworkServiceCoordinator *ResidentNetworkService::coordinator() noexcept {
  return m_coordinator.get();
}

} // namespace QindaQt::Network::Service
