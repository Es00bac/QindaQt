// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_network/network_settings_model.h>
#include <qindaqt/services/network_model/network_intent_policy.h>

namespace QindaQt::Apps::SettingsNetwork {
using namespace QindaQt::Network;

QVariantList NetworkSettingsModel::hiddenNetworkDevices() const {
  QVariantList rows;
  const auto &snapshot = m_client.model().snapshot();
  if (!snapshot) {
    return rows;
  }
  for (const Device &device : snapshot->devices) {
    if (device.kind == DeviceKind::Wifi &&
        device.state != DeviceState::Unknown &&
        device.state != DeviceState::Unavailable) {
      rows.append(
          QVariantMap{{QStringLiteral("interfaceName"), device.interfaceName}});
    }
  }
  return rows;
}

bool NetworkSettingsModel::hiddenJoinAvailable(const QString &deviceInterface,
                                               const QString &ssid,
                                               const quint32 security) const {
  return !m_pendingRadio && secretAgentRegistered() &&
         m_client.operationAdmissionReady() &&
         m_client.model()
             .connectHidden(
                 {deviceInterface, ssid, static_cast<SecuritySuite>(security)})
             .allowed;
}

bool NetworkSettingsModel::connectHiddenNetwork(const QString &deviceInterface,
                                                const QString &ssid,
                                                const quint32 security) {
  // AGENT-GUARD: Presence permits attempting the existing agent path only;
  // it is never secret authority or evidence authentication will succeed.
  if (!secretAgentRegistered()) {
    rejectAction(QStringLiteral("secret-agent-unavailable"));
    return false;
  }
  if (m_pendingRadio) {
    rejectAction(QStringLiteral("operation-in-flight"));
    return false;
  }
  QString error;
  if (!m_client.connectHiddenNetwork(
          {deviceInterface, ssid, static_cast<SecuritySuite>(security)},
          &error)) {
    rejectAction(error);
    return false;
  }
  beginOperationMessage(OperationKind::ConnectKnownNetwork);
  m_operationStatusText =
      tr("Creating and connecting the hidden network profile…");
  Q_EMIT viewChanged();
  return true;
}
} // namespace QindaQt::Apps::SettingsNetwork
