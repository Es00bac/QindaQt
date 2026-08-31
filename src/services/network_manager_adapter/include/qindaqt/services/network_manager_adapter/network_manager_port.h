// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_manager_adapter/network_manager_facts.h>
#include <qindaqt/services/network_service/network_backend.h>

#include <QtCore/QObject>

namespace QindaQt::Network::NetworkManager {

// Injected platform seam owned and called by NetworkManagerBackend on one Qt
// thread. Implementations keep every libnm/GObject handle private. Facts and
// operation outcomes are secret-free immutable copies. authorityReplaced is
// emitted only after a previously observed nonempty NetworkManager unique
// owner disappears or changes; initial absence may later become available.
// start() reports whether an initial client exists, but false is recoverable
// and observation may continue. stop()/cancel() are idempotent.
class NetworkManagerPort : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~NetworkManagerPort() override = default;

  [[nodiscard]] virtual bool start() = 0;
  virtual void stop() = 0;
  virtual void
  submit(quint64 operationId,
         const QindaQt::Network::Service::BackendOperationRequest &request) = 0;
  virtual void cancel(quint64 operationId) = 0;

Q_SIGNALS:
  void factsReady(const QindaQt::Network::NetworkManager::Facts &facts);
  void operationFinished(
      quint64 operationId,
      const QindaQt::Network::Service::BackendOperationOutcome &outcome);
  void authorityReplaced();
};

} // namespace QindaQt::Network::NetworkManager
