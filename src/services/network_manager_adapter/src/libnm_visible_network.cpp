// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnm_network_manager_port_p.h"

#include <qindaqt/services/network_protocol/network_identity.h>

#include <memory>

namespace QindaQt::Network::NetworkManager {
namespace {

using NmConnectionPtr =
    std::unique_ptr<NMConnection, decltype(&g_object_unref)>;

bool supportedSecurity(const SecuritySuite security) {
  return security == SecuritySuite::Open
         || security == SecuritySuite::Wpa2Personal
         || security == SecuritySuite::Wpa3Personal;
}

QByteArray accessPointSsid(NMAccessPoint *point) {
  GBytes *bytes = point == nullptr ? nullptr : nm_access_point_get_ssid(point);
  if (bytes == nullptr) {
    return {};
  }
  gsize size = 0;
  const auto *data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
  return data == nullptr ? QByteArray{}
                         : QByteArray(data, static_cast<qsizetype>(size));
}

SecuritySuite accessPointSecurity(NMAccessPoint *point) {
  const guint32 flags = static_cast<guint32>(nm_access_point_get_flags(point));
  const guint32 wpa =
      static_cast<guint32>(nm_access_point_get_wpa_flags(point));
  const guint32 rsn =
      static_cast<guint32>(nm_access_point_get_rsn_flags(point));
  const guint32 combined = wpa | rsn;
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_SAE) != 0U) {
    return SecuritySuite::Wpa3Personal;
  }
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_802_1X) != 0U) {
    return SecuritySuite::Wpa2Enterprise;
  }
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_PSK) != 0U) {
    return SecuritySuite::Wpa2Personal;
  }
  return (flags & NM_802_11_AP_FLAGS_PRIVACY) != 0U
             ? SecuritySuite::Wep
             : SecuritySuite::Open;
}

} // namespace

NMConnection *buildVisibleWifiProfile(const QByteArrayView rawSsid,
                                      const SecuritySuite security) {
  const SsidIdentity identity = normalizeSsid(rawSsid);
  if (!identity.valid || identity.hidden || identity.text.isEmpty()
      || !supportedSecurity(security)) {
    return nullptr;
  }

  NMConnection *profile = nm_simple_connection_new();
  auto *base = NM_SETTING_CONNECTION(nm_setting_connection_new());
  const QByteArray profileName = identity.text.toUtf8();
  g_object_set(base, NM_SETTING_CONNECTION_ID, profileName.constData(),
               NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
               NM_SETTING_CONNECTION_AUTOCONNECT, TRUE, nullptr);
  nm_connection_add_setting(profile, NM_SETTING(base));

  auto *wireless = NM_SETTING_WIRELESS(nm_setting_wireless_new());
  GBytes *ssid =
      g_bytes_new(rawSsid.data(), static_cast<gsize>(rawSsid.size()));
  g_object_set(wireless, NM_SETTING_WIRELESS_SSID, ssid,
               NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA,
               nullptr);
  g_bytes_unref(ssid);
  nm_connection_add_setting(profile, NM_SETTING(wireless));

  if (security != SecuritySuite::Open) {
    auto *wifiSecurity = NM_SETTING_WIRELESS_SECURITY(
        nm_setting_wireless_security_new());
    const char *keyManagement = security == SecuritySuite::Wpa3Personal
                                    ? "sae"
                                    : "wpa-psk";
    // AGENT-GUARD: The PSK property must remain absent. AGENT_OWNED tells
    // NetworkManager to obtain it later from the separately registered secret
    // agent; Network1 and this adapter never receive credential bytes.
    g_object_set(wifiSecurity, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                 keyManagement, NM_SETTING_WIRELESS_SECURITY_PSK_FLAGS,
                 static_cast<guint>(NM_SETTING_SECRET_FLAG_AGENT_OWNED),
                 nullptr);
    nm_connection_add_setting(profile, NM_SETTING(wifiSecurity));
  }
  return profile;
}

std::pair<NMDevice *, NMAccessPoint *>
LibnmNetworkManagerPort::accessPointForId(
    const QString &accessPointId) const {
  const GPtrArray *devices =
      m_client == nullptr ? nullptr : nm_client_get_devices(m_client);
  if (devices == nullptr) {
    return {};
  }
  for (guint deviceIndex = 0; deviceIndex < devices->len; ++deviceIndex) {
    auto *device = NM_DEVICE(g_ptr_array_index(devices, deviceIndex));
    if (nm_device_get_device_type(device) != NM_DEVICE_TYPE_WIFI) {
      continue;
    }
    const char *rawInterface = nm_device_get_iface(device);
    const GPtrArray *points =
        nm_device_wifi_get_access_points(NM_DEVICE_WIFI(device));
    if (rawInterface == nullptr || points == nullptr) {
      continue;
    }
    for (guint pointIndex = 0; pointIndex < points->len; ++pointIndex) {
      auto *point = NM_ACCESS_POINT(g_ptr_array_index(points, pointIndex));
      const char *rawBssid = nm_access_point_get_bssid(point);
      QString interfaceName;
      QString bssid;
      if (rawBssid != nullptr
          && normalizeInterfaceName(QString::fromUtf8(rawInterface),
                                    &interfaceName)
          && normalizeBssid(QString::fromUtf8(rawBssid), &bssid)
          && visibleAccessPointId(interfaceName, bssid) == accessPointId) {
        return {device, point};
      }
    }
  }
  return {};
}

void LibnmNetworkManagerPort::submitVisibleConnect(
    const quint64 operationId,
    const Service::BackendOperationRequest &request) {
  const auto [device, point] = accessPointForId(request.identifier);
  GCancellable *cancellable = beginAsync(operationId);
  if (device == nullptr || point == nullptr || cancellable == nullptr) {
    completeAsync(operationId, false,
                  QStringLiteral("visible-network-unavailable"));
    return;
  }
  const QByteArray ssid = accessPointSsid(point);
  const SecuritySuite security = accessPointSecurity(point);
  NmConnectionPtr profile(buildVisibleWifiProfile(ssid, security),
                          &g_object_unref);
  const char *specificObject = nm_object_get_path(NM_OBJECT(point));
  if (profile == nullptr || specificObject == nullptr) {
    completeAsync(operationId, false,
                  QStringLiteral("visible-network-unsupported"));
    return;
  }
  auto *state = new CallbackState{this, operationId};
  nm_client_add_and_activate_connection_async(
      m_client, profile.get(), device, specificObject, cancellable,
      &LibnmNetworkManagerPort::visibleActivationFinished, state);
}

void LibnmNetworkManagerPort::visibleActivationFinished(
    GObject *source, GAsyncResult *result, gpointer userData) {
  std::unique_ptr<CallbackState> state(
      static_cast<CallbackState *>(userData));
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
        QStringLiteral("visible-activation-dispatch-failed"));
    state->port->publishFacts();
  }
}

} // namespace QindaQt::Network::NetworkManager
