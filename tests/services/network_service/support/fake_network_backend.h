// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_service/network_backend.h>

#include <QtCore/QList>

#include <optional>

namespace QindaQt::Network::Tests {

class FakeNetworkBackend final : public Service::NetworkBackend {
  Q_OBJECT

public:
  struct Call final {
    quint64 operationId = 0;
    Service::BackendOperationRequest request;
  };

  quint64 start() override {
    ++startCalls;
    running = true;
    if (failStart) {
      return 0;
    }
    ++generation;
    if (generation == 0) {
      generation = 1;
    }
    return generation;
  }

  void stop() override {
    ++stopCalls;
    running = false;
  }

  void submit(const quint64 operationId,
              const Service::BackendOperationRequest &request) override {
    calls.append({operationId, request});
    if (synchronousOutcome.has_value()) {
      Q_EMIT operationFinished(generation, operationId, *synchronousOutcome);
    }
  }

  void cancel(const quint64 operationId) override {
    cancelled.append(operationId);
  }

  void publish(const Service::BackendObservation &observation) {
    Q_EMIT observationReady(generation, observation);
  }

  void publishForGeneration(const quint64 selectedGeneration,
                            const Service::BackendObservation &observation) {
    Q_EMIT observationReady(selectedGeneration, observation);
  }

  void finish(const quint64 operationId,
              const Service::BackendOperationOutcome &outcome) {
    Q_EMIT operationFinished(generation, operationId, outcome);
  }

  void finishForGeneration(const quint64 selectedGeneration,
                           const quint64 operationId,
                           const Service::BackendOperationOutcome &outcome) {
    Q_EMIT operationFinished(selectedGeneration, operationId, outcome);
  }

  void replaceAuthority() { Q_EMIT authorityReplaced(generation); }

  QList<Call> calls;
  QList<quint64> cancelled;
  quint64 generation = 0;
  int startCalls = 0;
  int stopCalls = 0;
  bool failStart = false;
  bool running = false;
  std::optional<Service::BackendOperationOutcome> synchronousOutcome;
};

inline Service::BackendObservation readyNetworkObservation() {
  const QByteArray ssid("Cafe");
  const QString id = knownNetworkId(ssid, SecuritySuite::Wpa2Personal);
  Service::BackendObservation observation;
  observation.availability = Availability::Ready;
  observation.capabilities = Capability::Connectivity | Capability::Scan |
                             Capability::KnownNetworkControl |
                             Capability::RadioControl |
                             Capability::ActiveConnectionControl;
  observation.connectivity = ConnectivityKind::Full;
  observation.radios = {{RadioKind::Wifi, true, true, true}};
  observation.devices = {
      {QStringLiteral("enp3s0"), DeviceKind::Ethernet, DeviceState::Connected},
      {QStringLiteral("wlan0"), DeviceKind::Wifi, DeviceState::Connected}};
  observation.accessPoints = {{QStringLiteral("wlan0"), QStringLiteral("Cafe"),
                               false, QStringLiteral("02:11:22:33:44:55"),
                               SecuritySuite::Wpa2Personal, 5'180, 72}};
  observation.knownNetworks = {
      {id, QStringLiteral("Cafe"), false, SecuritySuite::Wpa2Personal, true}};
  observation.activeConnections = {{QStringLiteral("wlan0"), id}};
  return observation;
}

} // namespace QindaQt::Network::Tests
