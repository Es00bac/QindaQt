// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_manager_adapter/network_manager_port.h>
#include <qindaqt/services/network_service/network_backend.h>

#include <memory>

namespace QindaQt::Network::NetworkManager {

// Production NetworkBackend implementation. The default constructor owns the
// confined libnm port; tests inject a deterministic port. It normalizes and
// bounds raw public facts before they cross into Network1, derives known IDs
// from raw SSID/security, drops broken references atomically, and never reads
// or transports credentials. All lifetime and calls are Qt-thread-confined.
class NetworkManagerBackend final : public Service::NetworkBackend {
  Q_OBJECT

public:
  explicit NetworkManagerBackend(QObject *parent = nullptr);
  explicit NetworkManagerBackend(std::unique_ptr<NetworkManagerPort> port,
                                 QObject *parent = nullptr);
  ~NetworkManagerBackend() override;

  [[nodiscard]] quint64 start() override;
  void stop() override;
  void submit(quint64 operationId,
              const Service::BackendOperationRequest &request) override;
  void cancel(quint64 operationId) override;

private:
  void handleFacts(const Facts &facts);
  [[nodiscard]] quint64 advanceGeneration();

  std::unique_ptr<NetworkManagerPort> m_port;
  quint64 m_generation = 0;
  bool m_running = false;
  bool m_starting = false;
};

} // namespace QindaQt::Network::NetworkManager
