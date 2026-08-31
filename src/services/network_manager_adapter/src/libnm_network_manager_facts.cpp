// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnm_network_manager_port_p.h"

#include <qindaqt/services/network_protocol/network_limits.h>

#include <algorithm>

namespace QindaQt::Network::NetworkManager {
namespace {

QByteArray bytesFromGBytes(GBytes *bytes) {
  if (bytes == nullptr) {
    return {};
  }
  gsize size = 0;
  const auto *data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
  if (data == nullptr || size > static_cast<gsize>(kMaxSsidRawBytes)) {
    return QByteArray(size > 0 ? kMaxSsidRawBytes + 1 : 0, '\0');
  }
  return QByteArray(data, static_cast<qsizetype>(size));
}

SecuritySuite securityFromKeyManagement(const char *keyManagement) {
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

SecuritySuite securityFromConnection(NMConnection *connection) {
  if (connection == nullptr) {
    return SecuritySuite::Open;
  }
  NMSettingWirelessSecurity *security =
      nm_connection_get_setting_wireless_security(connection);
  return security == nullptr
             ? SecuritySuite::Open
             : securityFromKeyManagement(
                   nm_setting_wireless_security_get_key_mgmt(security));
}

QByteArray ssidFromConnection(NMConnection *connection) {
  if (connection == nullptr) {
    return {};
  }
  NMSettingWireless *wireless = nm_connection_get_setting_wireless(connection);
  return wireless == nullptr
             ? QByteArray{}
             : bytesFromGBytes(nm_setting_wireless_get_ssid(wireless));
}

SecuritySuite securityFromAccessPoint(NMAccessPoint *point) {
  const auto flags = nm_access_point_get_flags(point);
  const auto wpa = nm_access_point_get_wpa_flags(point);
  const auto rsn = nm_access_point_get_rsn_flags(point);
  const guint32 combined =
      static_cast<guint32>(wpa) | static_cast<guint32>(rsn);
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_SAE) != 0U) {
    return SecuritySuite::Wpa3Personal;
  }
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_802_1X) != 0U) {
    return SecuritySuite::Wpa2Enterprise;
  }
  if ((combined & NM_802_11_AP_SEC_KEY_MGMT_PSK) != 0U) {
    return SecuritySuite::Wpa2Personal;
  }
  return (static_cast<guint32>(flags) & NM_802_11_AP_FLAGS_PRIVACY) != 0U
             ? SecuritySuite::Wep
             : SecuritySuite::Open;
}

std::optional<DeviceKind> deviceKind(const NMDeviceType type) {
  switch (type) {
  case NM_DEVICE_TYPE_ETHERNET:
    return DeviceKind::Ethernet;
  case NM_DEVICE_TYPE_WIFI:
    return DeviceKind::Wifi;
  case NM_DEVICE_TYPE_MODEM:
    return DeviceKind::Wwan;
  default:
    return std::nullopt;
  }
}

DeviceState deviceState(const NMDeviceState state) {
  switch (state) {
  case NM_DEVICE_STATE_UNMANAGED:
  case NM_DEVICE_STATE_UNAVAILABLE:
    return DeviceState::Unavailable;
  case NM_DEVICE_STATE_DISCONNECTED:
    return DeviceState::Disconnected;
  case NM_DEVICE_STATE_PREPARE:
  case NM_DEVICE_STATE_CONFIG:
  case NM_DEVICE_STATE_NEED_AUTH:
  case NM_DEVICE_STATE_IP_CONFIG:
  case NM_DEVICE_STATE_IP_CHECK:
  case NM_DEVICE_STATE_SECONDARIES:
    return DeviceState::Connecting;
  case NM_DEVICE_STATE_ACTIVATED:
    return DeviceState::Connected;
  case NM_DEVICE_STATE_DEACTIVATING:
    return DeviceState::Disconnecting;
  case NM_DEVICE_STATE_FAILED:
    return DeviceState::Failed;
  default:
    return DeviceState::Unknown;
  }
}

ConnectivityKind connectivity(const NMConnectivityState state) {
  switch (state) {
  case NM_CONNECTIVITY_NONE:
    return ConnectivityKind::Offline;
  case NM_CONNECTIVITY_PORTAL:
    return ConnectivityKind::Portal;
  case NM_CONNECTIVITY_LIMITED:
    return ConnectivityKind::Limited;
  case NM_CONNECTIVITY_FULL:
    return ConnectivityKind::Full;
  default:
    return ConnectivityKind::Unknown;
  }
}

bool permitted(NMClient *client, const NMClientPermission permission) {
  const NMClientPermissionResult result =
      nm_client_get_permission_result(client, permission);
  return result == NM_CLIENT_PERMISSION_RESULT_YES ||
         result == NM_CLIENT_PERMISSION_RESULT_AUTH;
}

qint64 monotonicMilliseconds() { return g_get_monotonic_time() / 1000; }

} // namespace

void ScanLeaseState::applyTo(Facts &facts,
                             const qint64 nowMilliseconds) noexcept {
  const qint64 remaining = deadlineMilliseconds - nowMilliseconds;
  if (remaining >= kMinimumScanDeadlineMilliseconds) {
    facts.scanPhase = inProgress ? ScanPhase::Scanning : ScanPhase::Leased;
    facts.scanLeaseRemainingMilliseconds =
        std::min<qint64>(remaining, kMaximumScanDeadlineMilliseconds);
    return;
  }
  *this = {};
}

Facts LibnmNetworkManagerPort::collectFacts() {
  Facts facts;
  if (m_client == nullptr || !nm_client_get_nm_running(m_client) ||
      m_observedOwner.isEmpty()) {
    return facts;
  }
  facts.available = true;
  facts.connectivity =
      NetworkManager::connectivity(nm_client_get_connectivity(m_client));

  bool wifiPresent = false;
  bool wwanPresent = false;
  const GPtrArray *devices = nm_client_get_devices(m_client);
  if (devices != nullptr) {
    for (guint index = 0; index < devices->len; ++index) {
      auto *device = NM_DEVICE(g_ptr_array_index(devices, index));
      const std::optional<DeviceKind> kind =
          deviceKind(nm_device_get_device_type(device));
      const char *rawInterface = nm_device_get_iface(device);
      if (!kind.has_value() || rawInterface == nullptr) {
        continue;
      }
      const QString interfaceName = QString::fromUtf8(rawInterface);
      facts.devices.append(
          {interfaceName, *kind,
           NetworkManager::deviceState(nm_device_get_state(device))});
      wifiPresent = wifiPresent || *kind == DeviceKind::Wifi;
      wwanPresent = wwanPresent || *kind == DeviceKind::Wwan;

      if (*kind != DeviceKind::Wifi) {
        continue;
      }
      const GPtrArray *points =
          nm_device_wifi_get_access_points(NM_DEVICE_WIFI(device));
      if (points == nullptr) {
        continue;
      }
      for (guint pointIndex = 0; pointIndex < points->len; ++pointIndex) {
        auto *point = NM_ACCESS_POINT(g_ptr_array_index(points, pointIndex));
        const char *bssid = nm_access_point_get_bssid(point);
        facts.accessPoints.append(
            {interfaceName, bytesFromGBytes(nm_access_point_get_ssid(point)),
             bssid == nullptr ? QString{} : QString::fromUtf8(bssid),
             securityFromAccessPoint(point),
             nm_access_point_get_frequency(point),
             nm_access_point_get_strength(point)});
      }
    }
  }

  if (wifiPresent) {
    facts.radios.append(
        {RadioKind::Wifi, true,
         nm_client_wireless_hardware_get_enabled(m_client) != FALSE,
         nm_client_wireless_get_enabled(m_client) != FALSE});
  }
  if (wwanPresent) {
    facts.radios.append({RadioKind::Wwan, true,
                         nm_client_wwan_hardware_get_enabled(m_client) != FALSE,
                         nm_client_wwan_get_enabled(m_client) != FALSE});
  }

  const GPtrArray *connections = nm_client_get_connections(m_client);
  if (connections != nullptr) {
    for (guint index = 0; index < connections->len; ++index) {
      auto *remote =
          NM_REMOTE_CONNECTION(g_ptr_array_index(connections, index));
      NMConnection *connection = NM_CONNECTION(remote);
      NMSettingWireless *wireless =
          nm_connection_get_setting_wireless(connection);
      if (wireless == nullptr) {
        continue;
      }
      NMSettingConnection *base =
          nm_connection_get_setting_connection(connection);
      facts.knownNetworks.append(
          {bytesFromGBytes(nm_setting_wireless_get_ssid(wireless)),
           securityFromConnection(connection),
           base == nullptr ||
               nm_setting_connection_get_autoconnect(base) != FALSE});
    }
  }

  const GPtrArray *active = nm_client_get_active_connections(m_client);
  if (active != nullptr) {
    for (guint index = 0; index < active->len; ++index) {
      auto *connection = NM_ACTIVE_CONNECTION(g_ptr_array_index(active, index));
      if (nm_active_connection_get_state(connection) !=
          NM_ACTIVE_CONNECTION_STATE_ACTIVATED) {
        continue;
      }
      NMRemoteConnection *remote =
          nm_active_connection_get_connection(connection);
      NMConnection *settings =
          remote == nullptr ? nullptr : NM_CONNECTION(remote);
      if (settings == nullptr ||
          nm_connection_get_setting_wireless(settings) == nullptr) {
        continue;
      }
      const GPtrArray *activeDevices =
          nm_active_connection_get_devices(connection);
      if (activeDevices == nullptr) {
        continue;
      }
      for (guint deviceIndex = 0; deviceIndex < activeDevices->len;
           ++deviceIndex) {
        auto *device = NM_DEVICE(g_ptr_array_index(activeDevices, deviceIndex));
        const char *rawInterface = nm_device_get_iface(device);
        if (rawInterface != nullptr) {
          facts.activeConnections.append({QString::fromUtf8(rawInterface),
                                          ssidFromConnection(settings),
                                          securityFromConnection(settings)});
        }
      }
    }
  }

  facts.scanSupported =
      wifiPresent && permitted(m_client, NM_CLIENT_PERMISSION_WIFI_SCAN);
  const bool canControl =
      permitted(m_client, NM_CLIENT_PERMISSION_NETWORK_CONTROL);
  facts.knownNetworkControlSupported =
      canControl && !facts.knownNetworks.isEmpty();
  facts.disconnectSupported = canControl;
  facts.radioControlSupported =
      (wifiPresent &&
       permitted(m_client, NM_CLIENT_PERMISSION_ENABLE_DISABLE_WIFI)) ||
      (wwanPresent &&
       permitted(m_client, NM_CLIENT_PERMISSION_ENABLE_DISABLE_WWAN));

  m_scanLease.applyTo(facts, monotonicMilliseconds());
  return facts;
}

} // namespace QindaQt::Network::NetworkManager
