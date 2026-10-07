// SPDX-License-Identifier: GPL-3.0-or-later
#include "libnm_network_manager_port_p.h"
#include <memory>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>

namespace QindaQt::Network::NetworkManager {
bool hiddenDeviceSecuritySupported(const quint32 capabilities) noexcept {
  return (capabilities & NM_WIFI_DEVICE_CAP_RSN) != 0U &&
         (capabilities & NM_WIFI_DEVICE_CAP_CIPHER_CCMP) != 0U;
}

void LibnmNetworkManagerPort::submitHiddenConnect(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  const ConnectHiddenIntent &intent = *request.hiddenJoin;
  const GPtrArray *devices = nm_client_get_devices(m_client);
  NMDevice *device = nullptr;
  for (guint index = 0; devices != nullptr && index < devices->len; ++index) {
    auto *candidate = NM_DEVICE(g_ptr_array_index(devices, index));
    const char *iface = nm_device_get_iface(candidate);
    if (nm_device_get_device_type(candidate) == NM_DEVICE_TYPE_WIFI &&
        iface != nullptr &&
        intent.deviceInterface == QString::fromUtf8(iface)) {
      device = candidate;
      break;
    }
  }
  GCancellable *cancellable = beginAsync(operationId);
  if (device == nullptr || cancellable == nullptr ||
      !nm_client_wireless_get_enabled(m_client) ||
      !nm_client_wireless_hardware_get_enabled(m_client)) {
    completeAsync(operationId, false,
                  QStringLiteral("hidden-network-device-unavailable"));
    return;
  }
  if (!hiddenDeviceSecuritySupported(
          nm_device_wifi_get_capabilities(NM_DEVICE_WIFI(device)))) {
    completeAsync(operationId, false,
                  QStringLiteral("hidden-network-security-unsupported"));
    return;
  }
  if (intent.ssid.isEmpty() || intent.ssid.size() > kMaxSsidRawBytes ||
      !isPresentationSafeText(intent.ssid)) {
    completeAsync(operationId, false,
                  QStringLiteral("hidden-network-ssid-invalid"));
    return;
  }
  const QByteArray ssid = intent.ssid.toUtf8();
  if (connectionForKnownId(knownNetworkId(ssid, intent.security)) != nullptr) {
    completeAsync(operationId, false, QStringLiteral("network-already-known"));
    return;
  }
  std::unique_ptr<NMConnection, decltype(&g_object_unref)> profile(
      buildWifiProfile(ssid, intent.security, true), &g_object_unref);
  if (!profile) {
    completeAsync(operationId, false,
                  QStringLiteral("hidden-network-security-unsupported"));
    return;
  }
  // AGENT-CONTRACT: null AP + exact SSID/infra/hidden profile is NM's hidden
  // first-use path. RSN device bits do NOT prove SAE support; NM/supplicant
  // owns that refusal. No credential or supplicant authority crosses here.
  auto *state = new CallbackState{this, operationId};
  nm_client_add_and_activate_connection_async(
      m_client, profile.get(), device, nullptr, cancellable,
      &LibnmNetworkManagerPort::hiddenActivationFinished, state);
}

void LibnmNetworkManagerPort::hiddenActivationFinished(GObject *source,
                                                       GAsyncResult *result,
                                                       gpointer userData) {
  std::unique_ptr<CallbackState> state(static_cast<CallbackState *>(userData));
  GError *error = nullptr;
  NMActiveConnection *active = nm_client_add_and_activate_connection_finish(
      NM_CLIENT(source), result, &error);
  const bool succeeded = active != nullptr;
  if (active != nullptr) {
    g_object_unref(active);
  }
  if (error != nullptr) {
    g_error_free(error);
  }
  if (state->port != nullptr) {
    state->port->completeAsync(
        state->operationId, succeeded,
        QStringLiteral("hidden-activation-dispatch-failed"));
    state->port->publishFacts();
  }
}
} // namespace QindaQt::Network::NetworkManager
