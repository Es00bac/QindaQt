// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_network/network_settings_model.h>

#include <algorithm>

namespace QindaQt::Apps::SettingsNetwork {
namespace {

using QindaQt::Network::OperationKind;
using QindaQt::Network::OperationStatus;

constexpr qint64 kScanDeadlineMilliseconds = 30'000;

} // namespace

bool NetworkSettingsModel::reload() {
  if (busy()) {
    rejectAction(QStringLiteral("operation-in-flight"));
    return false;
  }
  QString error;
  if (!m_client.start(&error)) {
    rejectAction(error.isEmpty() ? QStringLiteral("client-start-failed")
                                 : error);
    return false;
  }
  m_localError.clear();
  m_operationStatusText = tr("Refreshing authoritative network information…");
  m_client.refresh();
  Q_EMIT viewChanged();
  return true;
}

bool NetworkSettingsModel::requestScan() {
  QString error;
  if (!m_client.requestScan(kScanDeadlineMilliseconds, &error)) {
    rejectAction(error);
    return false;
  }
  beginOperationMessage(OperationKind::RequestScan);
  return true;
}

bool NetworkSettingsModel::connectKnownNetwork(
    const QString &knownNetworkId) {
  QString error;
  if (!m_client.connectKnownNetwork(knownNetworkId, &error)) {
    rejectAction(error);
    return false;
  }
  beginOperationMessage(OperationKind::ConnectKnownNetwork);
  return true;
}

bool NetworkSettingsModel::connectVisibleNetwork(
    const QString &accessPointId) {
  const QVariantList points = accessPoints();
  const auto projected = std::find_if(
      points.cbegin(), points.cend(), [&accessPointId](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("id")).toString()
               == accessPointId;
      });
  if (projected == points.cend()) {
    rejectAction(QStringLiteral("access-point-not-found"));
    return false;
  }
  const QVariantMap row = projected->toMap();
  if (!row.value(QStringLiteral("connectAvailable")).toBool()) {
    // AGENT-GUARD: The invokable must consume the exact availability projected
    // to QML. Re-evaluating only Network1 admission here bypasses route-owned
    // secret-agent presence truth for secured first-use connections.
    const QString reason =
        row.value(QStringLiteral("connectBlockedReason")).toString();
    rejectAction(reason.isEmpty() ? QStringLiteral("visible-network-unavailable")
                                  : reason);
    return false;
  }
  QString error;
  if (!m_client.connectVisibleNetwork(accessPointId, &error)) {
    rejectAction(error);
    return false;
  }
  beginOperationMessage(OperationKind::ConnectVisibleNetwork);
  return true;
}

bool NetworkSettingsModel::disconnectDevice(const QString &deviceInterface) {
  QString error;
  if (!m_client.disconnectDevice(deviceInterface, &error)) {
    rejectAction(error);
    return false;
  }
  beginOperationMessage(OperationKind::DisconnectActive);
  return true;
}

void NetworkSettingsModel::handleOperationFinished(
    const QindaQt::Network::OperationResult &result) {
  if (result.status == OperationStatus::Succeeded) {
    m_localError.clear();
    switch (result.kind) {
    case OperationKind::RequestScan:
      m_operationStatusText = tr("Scan requested; awaiting fresh results.");
      break;
    case OperationKind::ConnectKnownNetwork:
      m_operationStatusText =
          tr("Connecting…");
      break;
    case OperationKind::ConnectVisibleNetwork:
      m_operationStatusText =
          tr("New profile requested; awaiting authoritative state.");
      break;
    case OperationKind::DisconnectActive:
      m_operationStatusText =
          tr("Disconnecting…");
      break;
    case OperationKind::SetRadio:
      m_operationStatusText = tr("Network operation completed.");
      break;
    }
  } else {
    m_operationStatusText.clear();
    m_localError = actionFailureText(result.reasonCode);
  }
  Q_EMIT viewChanged();
}

void NetworkSettingsModel::handleOperationUncertain(const QString &message) {
  Q_UNUSED(message);
  m_operationStatusText.clear();
  m_localError = tr("The network operation outcome is uncertain. It was not "
                    "replayed; reload authoritative state before trying again.");
  Q_EMIT viewChanged();
}

void NetworkSettingsModel::beginOperationMessage(const OperationKind kind) {
  m_localError.clear();
  switch (kind) {
  case OperationKind::RequestScan:
    m_operationStatusText = tr("Requesting a network scan…");
    break;
  case OperationKind::ConnectKnownNetwork:
    m_operationStatusText = tr("Requesting connection to the saved network…");
    break;
  case OperationKind::ConnectVisibleNetwork:
    m_operationStatusText = tr("Creating and connecting a network profile…");
    break;
  case OperationKind::DisconnectActive:
    m_operationStatusText = tr("Requesting disconnection…");
    break;
  case OperationKind::SetRadio:
    m_operationStatusText = tr("Requesting network operation…");
    break;
  }
  Q_EMIT viewChanged();
}

void NetworkSettingsModel::rejectAction(const QString &reason) {
  m_operationStatusText.clear();
  m_localError = actionFailureText(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString NetworkSettingsModel::actionFailureText(const QString &reason) const {
  if (reason == QStringLiteral("scan-unsupported")) {
    return tr("Scanning is not permitted by the network service.");
  }
  if (reason == QStringLiteral("scan-busy")
      || reason == QStringLiteral("scan-lease-held")
      || reason == QStringLiteral("operation-in-flight")) {
    return tr("A network change or search is already in progress.");
  }
  if (reason == QStringLiteral("known-network-control-unsupported")) {
    return tr("Connecting saved networks is not permitted by the network service.");
  }
  if (reason == QStringLiteral("visible-network-control-unsupported")) {
    return tr("Creating network profiles is not permitted by the network service.");
  }
  if (reason == QStringLiteral("secret-agent-unavailable")) {
    return tr("No secret agent is available to prompt for this secured network.");
  }
  if (reason == QStringLiteral("hidden-network-unsupported")
      || reason == QStringLiteral("wep-network-unsupported")
      || reason == QStringLiteral("enterprise-network-unsupported")) {
    return tr("That network type is not supported for first-time connection.");
  }
  if (reason == QStringLiteral("active-connection-control-unsupported")) {
    return tr("Disconnecting is not permitted by the network service.");
  }
  if (reason == QStringLiteral("client-not-ready")
      || reason == QStringLiteral("service-not-ready")) {
    return tr("The network service is not ready.");
  }
  if (reason == QStringLiteral("network-already-active")) {
    return tr("That saved network is already active.");
  }
  if (reason == QStringLiteral("network-already-known")) {
    return tr("That network already has a saved profile.");
  }
  if (reason == QStringLiteral("device-not-connected")) {
    return tr("That network device is no longer connected.");
  }
  if (reason.isEmpty()) {
    return tr("The network request was rejected.");
  }
  if (reason == QStringLiteral("credentials-required")) {
    return tr("A password is required to connect to this network.");
  }
  return tr("The connection could not be changed. Refresh the network list and try again.");
}

} // namespace QindaQt::Apps::SettingsNetwork
