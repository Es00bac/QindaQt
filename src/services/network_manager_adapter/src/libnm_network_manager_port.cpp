// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnm_network_manager_port_p.h"

#include <qindaqt/services/network_protocol/network_identity.h>

#include <memory>

namespace QindaQt::Network::NetworkManager {
namespace {

QByteArray connectionSsid(NMConnection *connection) {
  NMSettingWireless *wireless =
      connection == nullptr ? nullptr
                            : nm_connection_get_setting_wireless(connection);
  GBytes *bytes =
      wireless == nullptr ? nullptr : nm_setting_wireless_get_ssid(wireless);
  if (bytes == nullptr) {
    return {};
  }
  gsize size = 0;
  const auto *data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
  return data == nullptr ? QByteArray{}
                         : QByteArray(data, static_cast<qsizetype>(size));
}

SecuritySuite connectionSecurity(NMConnection *connection) {
  NMSettingWirelessSecurity *security =
      connection == nullptr
          ? nullptr
          : nm_connection_get_setting_wireless_security(connection);
  const char *keyManagement =
      security == nullptr ? nullptr
                          : nm_setting_wireless_security_get_key_mgmt(security);
  if (keyManagement == nullptr || keyManagement[0] == '\0') {
    return SecuritySuite::Open;
  }
  const QByteArrayView value(keyManagement);
  if (value == "none") {
    return SecuritySuite::Wep;
  }
  if (value == "sae") {
    return SecuritySuite::Wpa3Personal;
  }
  if (value == "wpa-eap-suite-b-192") {
    return SecuritySuite::Wpa3Enterprise;
  }
  if (value == "wpa-eap" || value == "ieee8021x") {
    return SecuritySuite::Wpa2Enterprise;
  }
  return SecuritySuite::Wpa2Personal;
}

qint64 monotonicMilliseconds() { return g_get_monotonic_time() / 1000; }

} // namespace

struct LibnmNetworkManagerPort::CallbackState final {
  QPointer<LibnmNetworkManagerPort> port;
  quint64 operationId = 0;
  RadioKind radioKind = RadioKind::Wifi;
  bool enable = false;
};

LibnmNetworkManagerPort::LibnmNetworkManagerPort(QObject *parent)
    : NetworkManagerPort(parent) {
  m_pollTimer.setInterval(1'000);
  connect(&m_pollTimer, &QTimer::timeout, this, &LibnmNetworkManagerPort::poll);
}

LibnmNetworkManagerPort::~LibnmNetworkManagerPort() { stop(); }

bool LibnmNetworkManagerPort::start() {
  if (m_running) {
    return true;
  }
  m_running = true;
  m_authorityRetired = false;
  m_observedOwner.clear();
  m_pollTimer.start();
  const bool available = ensureClient();
  QTimer::singleShot(0, this, &LibnmNetworkManagerPort::poll);
  return available;
}

void LibnmNetworkManagerPort::stop() {
  if (!m_running && m_client == nullptr && m_pending.isEmpty()) {
    return;
  }
  m_running = false;
  m_pollTimer.stop();
  for (GCancellable *cancellable : std::as_const(m_pending)) {
    g_cancellable_cancel(cancellable);
    g_object_unref(cancellable);
  }
  m_pending.clear();
  if (m_client != nullptr) {
    g_object_unref(m_client);
    m_client = nullptr;
  }
  m_observedOwner.clear();
  m_scanInProgress = false;
  m_scanLeaseDeadline = 0;
}

bool LibnmNetworkManagerPort::ensureClient() {
  if (m_client != nullptr) {
    return true;
  }
  GError *error = nullptr;
  m_client = nm_client_new(nullptr, &error);
  if (error != nullptr) {
    g_error_free(error);
  }
  return m_client != nullptr;
}

void LibnmNetworkManagerPort::poll() {
  if (!m_running || m_authorityRetired) {
    return;
  }
  if (!ensureClient()) {
    Facts unavailable;
    Q_EMIT factsReady(unavailable);
    return;
  }
  const char *rawOwner = nm_client_get_dbus_name_owner(m_client);
  const QString owner =
      rawOwner == nullptr ? QString{} : QString::fromUtf8(rawOwner);
  if (!m_observedOwner.isEmpty() &&
      (owner.isEmpty() || owner != m_observedOwner)) {
    // AGENT-GUARD: N0 rejects same-Network1-owner epoch replacement. Once
    // an upstream unique owner was observed, loss/replacement retires this
    // whole process lineage rather than reusing public ownership.
    m_authorityRetired = true;
    Q_EMIT authorityReplaced();
    return;
  }
  if (m_observedOwner.isEmpty() && !owner.isEmpty()) {
    m_observedOwner = owner;
  }
  publishFacts();
}

void LibnmNetworkManagerPort::publishFacts() {
  if (m_running && !m_authorityRetired) {
    Q_EMIT factsReady(collectFacts());
  }
}

GCancellable *LibnmNetworkManagerPort::beginAsync(const quint64 operationId) {
  if (m_pending.contains(operationId)) {
    return nullptr;
  }
  GCancellable *cancellable = g_cancellable_new();
  m_pending.insert(operationId, cancellable);
  return cancellable;
}

void LibnmNetworkManagerPort::submit(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  if (!m_running || m_client == nullptr || m_authorityRetired ||
      !nm_client_get_nm_running(m_client)) {
    Service::BackendOperationOutcome unavailable;
    unavailable.status = Service::BackendOperationStatus::Failed;
    unavailable.reasonCode = QStringLiteral("networkmanager-unavailable");
    Q_EMIT operationFinished(operationId, unavailable);
    return;
  }
  switch (request.kind) {
  case OperationKind::RequestScan:
    submitScan(operationId, request);
    break;
  case OperationKind::ConnectKnownNetwork:
    submitConnect(operationId, request);
    break;
  case OperationKind::DisconnectActive:
    submitDisconnect(operationId, request);
    break;
  case OperationKind::SetRadio:
    submitRadio(operationId, request);
    break;
  }
}

void LibnmNetworkManagerPort::submitScan(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  NMDevice *device = firstWifiDevice();
  GCancellable *cancellable = beginAsync(operationId);
  if (device == nullptr || cancellable == nullptr) {
    completeAsync(operationId, false, QStringLiteral("scan-unavailable"));
    return;
  }
  m_scanInProgress = true;
  m_scanLeaseDeadline =
      monotonicMilliseconds() + request.scanDeadlineMilliseconds;
  publishFacts();
  auto *state = new CallbackState{this, operationId};
  nm_device_wifi_request_scan_async(NM_DEVICE_WIFI(device), cancellable,
                                    &LibnmNetworkManagerPort::scanFinished,
                                    state);
}

void LibnmNetworkManagerPort::submitConnect(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  NMRemoteConnection *connection = connectionForKnownId(request.identifier);
  GCancellable *cancellable = beginAsync(operationId);
  if (connection == nullptr || cancellable == nullptr) {
    completeAsync(operationId, false,
                  QStringLiteral("known-network-unavailable"));
    return;
  }
  auto *state = new CallbackState{this, operationId};
  // AGENT-CONTRACT: Only an already-stored connection object is activated.
  // NetworkManager may consult an external registered secret agent; this
  // process never requests, receives, caches, or transports its credentials.
  nm_client_activate_connection_async(
      m_client, NM_CONNECTION(connection), nullptr, nullptr, cancellable,
      &LibnmNetworkManagerPort::activationFinished, state);
}

void LibnmNetworkManagerPort::submitDisconnect(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  NMActiveConnection *connection = activeForInterface(request.identifier);
  GCancellable *cancellable = beginAsync(operationId);
  if (connection == nullptr || cancellable == nullptr) {
    completeAsync(operationId, false,
                  QStringLiteral("active-connection-unavailable"));
    return;
  }
  auto *state = new CallbackState{this, operationId};
  nm_client_deactivate_connection_async(
      m_client, connection, cancellable,
      &LibnmNetworkManagerPort::deactivationFinished, state);
}

void LibnmNetworkManagerPort::submitRadio(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  GCancellable *cancellable = beginAsync(operationId);
  if (cancellable == nullptr) {
    completeAsync(operationId, false, QStringLiteral("radio-busy"));
    return;
  }
  auto *state =
      new CallbackState{this, operationId, request.radioKind, request.enable};
  const char *property =
      request.radioKind == RadioKind::Wifi ? "WirelessEnabled" : "WwanEnabled";
  nm_client_dbus_set_property(m_client, NM_DBUS_PATH, NM_DBUS_INTERFACE,
                              property, g_variant_new_boolean(request.enable),
                              4'000, cancellable,
                              &LibnmNetworkManagerPort::radioFinished, state);
}

void LibnmNetworkManagerPort::cancel(const quint64 operationId) {
  const auto it = m_pending.find(operationId);
  if (it == m_pending.end()) {
    return;
  }
  g_cancellable_cancel(it.value());
  g_object_unref(it.value());
  m_pending.erase(it);
}

void LibnmNetworkManagerPort::completeAsync(const quint64 operationId,
                                            const bool succeeded,
                                            const QString &reason) {
  const auto it = m_pending.find(operationId);
  if (it == m_pending.end()) {
    return;
  }
  g_object_unref(it.value());
  m_pending.erase(it);
  if (!m_running || m_authorityRetired) {
    return;
  }
  Q_EMIT operationFinished(
      operationId,
      {.status = succeeded ? Service::BackendOperationStatus::Succeeded
                           : Service::BackendOperationStatus::Failed,
       .reasonCode = succeeded ? QString{} : reason,
       .diagnostic = {}});
}

void LibnmNetworkManagerPort::scanFinished(GObject *source,
                                           GAsyncResult *result,
                                           gpointer userData) {
  std::unique_ptr<CallbackState> state(static_cast<CallbackState *>(userData));
  GError *error = nullptr;
  const bool succeeded = nm_device_wifi_request_scan_finish(
                             NM_DEVICE_WIFI(source), result, &error) != FALSE;
  if (error != nullptr) {
    g_error_free(error);
  }
  if (state->port != nullptr) {
    state->port->m_scanInProgress = false;
    state->port->completeAsync(state->operationId, succeeded,
                               QStringLiteral("scan-dispatch-failed"));
    state->port->publishFacts();
  }
}

void LibnmNetworkManagerPort::activationFinished(GObject *source,
                                                 GAsyncResult *result,
                                                 gpointer userData) {
  std::unique_ptr<CallbackState> state(static_cast<CallbackState *>(userData));
  GError *error = nullptr;
  NMActiveConnection *active =
      nm_client_activate_connection_finish(NM_CLIENT(source), result, &error);
  const bool succeeded = active != nullptr;
  if (active != nullptr) {
    g_object_unref(active);
  }
  if (error != nullptr) {
    g_error_free(error);
  }
  if (state->port != nullptr) {
    state->port->completeAsync(state->operationId, succeeded,
                               QStringLiteral("activation-dispatch-failed"));
    state->port->publishFacts();
  }
}

void LibnmNetworkManagerPort::deactivationFinished(GObject *source,
                                                   GAsyncResult *result,
                                                   gpointer userData) {
  std::unique_ptr<CallbackState> state(static_cast<CallbackState *>(userData));
  GError *error = nullptr;
  const bool succeeded = nm_client_deactivate_connection_finish(
                             NM_CLIENT(source), result, &error) != FALSE;
  if (error != nullptr) {
    g_error_free(error);
  }
  if (state->port != nullptr) {
    state->port->completeAsync(state->operationId, succeeded,
                               QStringLiteral("deactivation-dispatch-failed"));
    state->port->publishFacts();
  }
}

void LibnmNetworkManagerPort::radioFinished(GObject *source,
                                            GAsyncResult *result,
                                            gpointer userData) {
  std::unique_ptr<CallbackState> state(static_cast<CallbackState *>(userData));
  GError *error = nullptr;
  bool succeeded = nm_client_dbus_set_property_finish(NM_CLIENT(source), result,
                                                      &error) != FALSE;
  if (error != nullptr) {
    g_error_free(error);
  }
  if (state->port != nullptr && succeeded) {
    const bool observed =
        state->radioKind == RadioKind::Wifi
            ? nm_client_wireless_get_enabled(NM_CLIENT(source)) != FALSE
            : nm_client_wwan_get_enabled(NM_CLIENT(source)) != FALSE;
    succeeded = observed == state->enable;
  }
  if (state->port != nullptr) {
    state->port->completeAsync(state->operationId, succeeded,
                               QStringLiteral("radio-state-unconfirmed"));
    state->port->publishFacts();
  }
}

NMRemoteConnection *LibnmNetworkManagerPort::connectionForKnownId(
    const QString &knownNetworkIdValue) const {
  const GPtrArray *connections =
      m_client == nullptr ? nullptr : nm_client_get_connections(m_client);
  if (connections == nullptr) {
    return nullptr;
  }
  for (guint index = 0; index < connections->len; ++index) {
    auto *remote = NM_REMOTE_CONNECTION(g_ptr_array_index(connections, index));
    NMConnection *connection = NM_CONNECTION(remote);
    const QByteArray ssid = connectionSsid(connection);
    if (!ssid.isEmpty() &&
        knownNetworkId(ssid, connectionSecurity(connection)) ==
            knownNetworkIdValue) {
      return remote;
    }
  }
  return nullptr;
}

NMDevice *LibnmNetworkManagerPort::firstWifiDevice() const {
  const GPtrArray *devices =
      m_client == nullptr ? nullptr : nm_client_get_devices(m_client);
  if (devices == nullptr) {
    return nullptr;
  }
  for (guint index = 0; index < devices->len; ++index) {
    auto *device = NM_DEVICE(g_ptr_array_index(devices, index));
    if (nm_device_get_device_type(device) == NM_DEVICE_TYPE_WIFI) {
      return device;
    }
  }
  return nullptr;
}

NMActiveConnection *LibnmNetworkManagerPort::activeForInterface(
    const QString &interfaceName) const {
  const GPtrArray *active = m_client == nullptr
                                ? nullptr
                                : nm_client_get_active_connections(m_client);
  if (active == nullptr) {
    return nullptr;
  }
  for (guint index = 0; index < active->len; ++index) {
    auto *connection = NM_ACTIVE_CONNECTION(g_ptr_array_index(active, index));
    const GPtrArray *devices = nm_active_connection_get_devices(connection);
    if (devices == nullptr) {
      continue;
    }
    for (guint deviceIndex = 0; deviceIndex < devices->len; ++deviceIndex) {
      auto *device = NM_DEVICE(g_ptr_array_index(devices, deviceIndex));
      const char *rawInterface = nm_device_get_iface(device);
      if (rawInterface != nullptr &&
          QString::fromUtf8(rawInterface) == interfaceName) {
        return connection;
      }
    }
  }
  return nullptr;
}

} // namespace QindaQt::Network::NetworkManager
