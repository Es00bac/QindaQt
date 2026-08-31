// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_manager_adapter/network_manager_backend.h>

#include "libnm_network_manager_port_p.h"

#include <QtCore/QMetaObject>

#include <limits>

namespace QindaQt::Network::NetworkManager {

Service::BackendObservation mapNetworkManagerFacts(const Facts &facts);

NetworkManagerBackend::NetworkManagerBackend(QObject *parent)
    : NetworkManagerBackend(std::make_unique<LibnmNetworkManagerPort>(),
                            parent) {}

NetworkManagerBackend::NetworkManagerBackend(
    std::unique_ptr<NetworkManagerPort> port, QObject *parent)
    : Service::NetworkBackend(parent), m_port(std::move(port)) {
  Q_ASSERT(m_port != nullptr);
  connect(m_port.get(), &NetworkManagerPort::factsReady, this,
          &NetworkManagerBackend::handleFacts);
  connect(m_port.get(), &NetworkManagerPort::operationFinished, this,
          [this](const quint64 operationId,
                 const Service::BackendOperationOutcome &outcome) {
            if (m_running) {
              Q_EMIT operationFinished(m_generation, operationId, outcome);
            }
          });
  connect(m_port.get(), &NetworkManagerPort::authorityReplaced, this, [this] {
    if (m_running) {
      Q_EMIT authorityReplaced(m_generation);
    }
  });
}

NetworkManagerBackend::~NetworkManagerBackend() { stop(); }

quint64 NetworkManagerBackend::start() {
  if (m_running) {
    return m_generation;
  }
  m_generation = advanceGeneration();
  m_running = true;
  m_starting = true;
  const bool initiallyAvailable = m_port->start();
  m_starting = false;
  if (!initiallyAvailable) {
    QMetaObject::invokeMethod(
        this,
        [this, generation = m_generation] {
          if (m_running && generation == m_generation) {
            Facts unavailable;
            Q_EMIT observationReady(generation,
                                    mapNetworkManagerFacts(unavailable));
          }
        },
        Qt::QueuedConnection);
  }
  return m_generation;
}

void NetworkManagerBackend::stop() {
  if (!m_running) {
    return;
  }
  m_running = false;
  (void)advanceGeneration();
  m_port->stop();
}

void NetworkManagerBackend::submit(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  if (!m_running) {
    return;
  }
  m_port->submit(operationId, request);
}

void NetworkManagerBackend::cancel(const quint64 operationId) {
  m_port->cancel(operationId);
}

void NetworkManagerBackend::handleFacts(const Facts &facts) {
  if (!m_running) {
    return;
  }
  if (m_starting) {
    const quint64 generation = m_generation;
    // AGENT-GUARD: NetworkBackend promises start() returns before any
    // publication. An injected or future port may report synchronously;
    // defer that copy and fence it against stop/restart generation changes.
    QMetaObject::invokeMethod(
        this,
        [this, generation, facts] {
          if (m_running && generation == m_generation) {
            Q_EMIT observationReady(generation, mapNetworkManagerFacts(facts));
          }
        },
        Qt::QueuedConnection);
    return;
  }
  Q_EMIT observationReady(m_generation, mapNetworkManagerFacts(facts));
}

quint64 NetworkManagerBackend::advanceGeneration() {
  if (m_generation == std::numeric_limits<quint64>::max()) {
    m_generation = 1;
  } else {
    ++m_generation;
  }
  return m_generation;
}

} // namespace QindaQt::Network::NetworkManager
