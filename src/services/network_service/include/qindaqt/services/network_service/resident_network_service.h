// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_service/network_service_coordinator.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Network::Service {

class NetworkServiceObject;

enum class NetworkServiceStartStatus {
  Started,
  InvalidConnection,
  ObjectRegistrationFailed,
  NameAlreadyOwned,
  NameRegistrationFailed,
  CoordinatorStartFailed,
};

// Owns the backend, coordinator, service object, and well-known name on the
// constructing Qt thread. The named connection must remain registered for the
// object's lifetime. start()/stop() are idempotent; stop resolves pending work
// before releasing name and object ownership. A failed start leaves no public
// registration behind.
class ResidentNetworkService final : public QObject {
  Q_OBJECT

public:
  explicit ResidentNetworkService(std::unique_ptr<NetworkBackend> backend,
                                  const QDBusConnection &connection,
                                  QString serviceName = {},
                                  int operationTimeoutMilliseconds = 5'000,
                                  QObject *parent = nullptr);
  ~ResidentNetworkService() override;

  [[nodiscard]] NetworkServiceStartStatus start();
  void stop();
  [[nodiscard]] bool isRunning() const noexcept;
  [[nodiscard]] NetworkServiceCoordinator *coordinator() noexcept;

Q_SIGNALS:
  void restartRequired();

private:
  std::unique_ptr<NetworkBackend> m_backend;
  std::unique_ptr<NetworkServiceCoordinator> m_coordinator;
  std::unique_ptr<NetworkServiceObject> m_serviceObject;
  QDBusConnection m_connection;
  QString m_serviceName;
  bool m_objectRegistered = false;
  bool m_nameRegistered = false;
};

} // namespace QindaQt::Network::Service
