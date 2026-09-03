// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_network/network_settings_model.h>
#include <qindaqt/apps/settings_network/network_secret_agent_presence.h>

#include <qindaqt/services/network_model/network_intent_policy.h>
#include <qindaqt/services/network_model/network_model_state.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

#include <algorithm>

namespace QindaQt::Apps::SettingsNetwork {
namespace {

using QindaQt::Network::ActiveConnection;
using QindaQt::Network::Client::ClientState;
using QindaQt::Network::Device;
using QindaQt::Network::DeviceKind;
using QindaQt::Network::DeviceState;
using QindaQt::Network::KnownNetwork;
using QindaQt::Network::Model::ModelState;
using QindaQt::Network::OperationKind;
using QindaQt::Network::OperationStatus;
using QindaQt::Network::RadioKind;
using QindaQt::Network::ScanPhase;
using QindaQt::Network::SecuritySuite;

constexpr qint64 kScanDeadlineMilliseconds = 30'000;

QString tr(const char *text) {
  return QCoreApplication::translate("NetworkSettings", text);
}

QString connectivityLabel(const QindaQt::Network::ConnectivityKind kind) {
  using QindaQt::Network::ConnectivityKind;
  switch (kind) {
  case ConnectivityKind::Unknown:
    return tr("Connectivity unknown");
  case ConnectivityKind::Offline:
    return tr("Offline");
  case ConnectivityKind::Portal:
    return tr("Sign-in portal detected");
  case ConnectivityKind::Limited:
    return tr("Limited connectivity");
  case ConnectivityKind::Full:
    return tr("Connected to the internet");
  }
  return tr("Connectivity unknown");
}

QString deviceKindText(const DeviceKind kind) {
  switch (kind) {
  case DeviceKind::Ethernet:
    return tr("Ethernet");
  case DeviceKind::Wifi:
    return tr("Wi-Fi");
  case DeviceKind::Wwan:
    return tr("Mobile broadband");
  }
  return tr("Network device");
}

QString deviceStateText(const DeviceState state) {
  switch (state) {
  case DeviceState::Unknown:
    return tr("Unknown");
  case DeviceState::Unavailable:
    return tr("Unavailable");
  case DeviceState::Disconnected:
    return tr("Disconnected");
  case DeviceState::Connecting:
    return tr("Connecting");
  case DeviceState::Connected:
    return tr("Connected");
  case DeviceState::Disconnecting:
    return tr("Disconnecting");
  case DeviceState::Failed:
    return tr("Failed");
  }
  return tr("Unknown");
}

QString securityText(const SecuritySuite suite) {
  switch (suite) {
  case SecuritySuite::Open:
    return tr("Open");
  case SecuritySuite::Wep:
    return tr("WEP");
  case SecuritySuite::Wpa2Personal:
    return tr("WPA2 Personal");
  case SecuritySuite::Wpa2Enterprise:
    return tr("WPA2 Enterprise");
  case SecuritySuite::Wpa3Personal:
    return tr("WPA3 Personal");
  case SecuritySuite::Wpa3Enterprise:
    return tr("WPA3 Enterprise");
  }
  return tr("Unknown security");
}

QString radioKindText(const RadioKind kind) {
  return kind == RadioKind::Wifi ? tr("Wi-Fi") : tr("Mobile broadband");
}

const KnownNetwork *findKnownNetwork(const ModelState &state,
                                     const QString &id) {
  const auto found = std::find_if(
      state.knownNetworks.cbegin(), state.knownNetworks.cend(),
      [&id](const KnownNetwork &network) { return network.id == id; });
  return found == state.knownNetworks.cend() ? nullptr : &*found;
}

const ActiveConnection *findActiveForDevice(const ModelState &state,
                                             const QString &interfaceName) {
  const auto found = std::find_if(
      state.activeConnections.cbegin(), state.activeConnections.cend(),
      [&interfaceName](const ActiveConnection &active) {
        return active.deviceInterface == interfaceName;
      });
  return found == state.activeConnections.cend() ? nullptr : &*found;
}

const ActiveConnection *findActiveForNetwork(const ModelState &state,
                                              const QString &networkId) {
  const auto found = std::find_if(
      state.activeConnections.cbegin(), state.activeConnections.cend(),
      [&networkId](const ActiveConnection &active) {
        return active.knownNetworkId == networkId;
      });
  return found == state.activeConnections.cend() ? nullptr : &*found;
}

QString displayName(const KnownNetwork &network) {
  return network.hidden || network.ssid.isEmpty()
             ? tr("Hidden saved network")
             : network.ssid;
}

} // namespace

NetworkSettingsModel::NetworkSettingsModel(
    QindaQt::Network::Client::NetworkClient &client, QObject *parent)
    : NetworkSettingsModel(client, QDBusConnection::sessionBus(), parent) {}

NetworkSettingsModel::NetworkSettingsModel(
    QindaQt::Network::Client::NetworkClient &client,
    const QDBusConnection &presenceConnection, QObject *parent)
    : QObject(parent), m_client(client),
      m_secretAgentPresence(
          std::make_unique<NetworkSecretAgentPresence>(presenceConnection)) {
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::stateChanged, this,
          &NetworkSettingsModel::viewChanged);
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::snapshotChanged, this,
          &NetworkSettingsModel::viewChanged);
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::operationInFlightChanged,
          this, &NetworkSettingsModel::viewChanged);
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::operationAdmissionChanged,
          this, &NetworkSettingsModel::viewChanged);
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::operationFinished, this,
          &NetworkSettingsModel::handleOperationFinished);
  connect(&m_client,
          &QindaQt::Network::Client::NetworkClient::operationUncertain, this,
          &NetworkSettingsModel::handleOperationUncertain);
  connect(m_secretAgentPresence.get(),
          &NetworkSecretAgentPresence::registeredChanged, this,
          &NetworkSettingsModel::viewChanged);
}

NetworkSettingsModel::~NetworkSettingsModel() = default;

bool NetworkSettingsModel::loading() const noexcept {
  return m_client.state() == ClientState::Connecting;
}

bool NetworkSettingsModel::ready() const noexcept {
  return m_client.state() == ClientState::Ready;
}

bool NetworkSettingsModel::degraded() const noexcept {
  return m_client.state() == ClientState::Degraded;
}

bool NetworkSettingsModel::unavailable() const noexcept {
  return m_client.state() == ClientState::Unavailable;
}

bool NetworkSettingsModel::stale() const {
  return m_client.projection().hasSnapshot && !ready();
}

bool NetworkSettingsModel::busy() const noexcept {
  return m_client.operationInFlight();
}

bool NetworkSettingsModel::reloadAvailable() const noexcept { return !busy(); }

bool NetworkSettingsModel::scanAvailable() const {
  if (!m_client.operationAdmissionReady()) {
    return false;
  }
  return m_client.model()
      .requestScan(QindaQt::Network::RequestScanIntent{
          kScanDeadlineMilliseconds})
      .allowed;
}

bool NetworkSettingsModel::secretAgentRegistered() const noexcept {
  return m_secretAgentPresence->registered();
}

QString NetworkSettingsModel::secretAgentStatusText() const {
  return secretAgentRegistered()
             ? tr("The QindaQt credential prompt is registered with "
                  "NetworkManager. Credentials still bypass this page and "
                  "Network1.")
             : tr("No QindaQt credential prompt is registered. Secured saved "
                  "networks that need a credential may fail to connect.");
}

QString NetworkSettingsModel::statusText() const {
  const ModelState state = m_client.projection();
  if (loading()) {
    return tr("Connecting to the network service…");
  }
  if (ready()) {
    return connectivityLabel(state.connectivity);
  }
  if (stale()) {
    return tr("Network information is stale while the service recovers.");
  }
  if (degraded()) {
    return tr("Network information could not be verified.");
  }
  return tr("The network service is unavailable.");
}

QString NetworkSettingsModel::errorText() const {
  if (!m_localError.isEmpty()) {
    return m_localError;
  }
  return m_client.lastError();
}

QString NetworkSettingsModel::serviceOwner() const {
  return m_client.projection().owner;
}

qulonglong NetworkSettingsModel::serviceEpoch() const {
  return m_client.projection().epoch;
}

qulonglong NetworkSettingsModel::serviceRevision() const {
  return m_client.projection().revision;
}

QString NetworkSettingsModel::connectivityText() const {
  return connectivityLabel(m_client.projection().connectivity);
}

QString NetworkSettingsModel::scanStatusText() const {
  const ModelState state = m_client.projection();
  if (state.scanPhase == ScanPhase::Scanning) {
    return tr("Scanning for networks…");
  }
  if (state.scanPhase == ScanPhase::Leased && !state.scanLeaseExpired) {
    const qint64 seconds = (state.scanLeaseRemainingMs + 999) / 1'000;
    return QCoreApplication::translate(
               "NetworkSettings", "Scan results are current for %n second(s).",
               nullptr, static_cast<int>(seconds));
  }
  return tr("Scan results can be refreshed.");
}

QVariantList NetworkSettingsModel::radios() const {
  QVariantList rows;
  const ModelState state = m_client.projection();
  rows.reserve(state.radios.size());
  for (const auto &radio : state.radios) {
    QString status;
    if (!radio.present) {
      status = tr("Not present");
    } else if (!radio.hardwareEnabled) {
      status = tr("Disabled by hardware");
    } else if (!radio.softwareEnabled) {
      status = tr("Off");
    } else {
      status = tr("On");
    }
    rows.append(QVariantMap{
        {QStringLiteral("kind"), static_cast<quint32>(radio.kind)},
        {QStringLiteral("name"), radioKindText(radio.kind)},
        {QStringLiteral("present"), radio.present},
        {QStringLiteral("hardwareEnabled"), radio.hardwareEnabled},
        {QStringLiteral("softwareEnabled"), radio.softwareEnabled},
        {QStringLiteral("statusText"), status},
    });
  }
  return rows;
}

QVariantList NetworkSettingsModel::devices() const {
  QVariantList rows;
  const ModelState state = m_client.projection();
  rows.reserve(state.devices.size());
  for (const Device &device : state.devices) {
    const ActiveConnection *active =
        findActiveForDevice(state, device.interfaceName);
    const KnownNetwork *network =
        active == nullptr ? nullptr
                          : findKnownNetwork(state, active->knownNetworkId);
    const auto verdict = m_client.model().disconnectDevice(
        QindaQt::Network::DisconnectIntent{device.interfaceName});
    rows.append(QVariantMap{
        {QStringLiteral("interfaceName"), device.interfaceName},
        {QStringLiteral("kind"), static_cast<quint32>(device.kind)},
        {QStringLiteral("kindText"), deviceKindText(device.kind)},
        {QStringLiteral("state"), static_cast<quint32>(device.state)},
        {QStringLiteral("stateText"), deviceStateText(device.state)},
        {QStringLiteral("active"), active != nullptr},
        {QStringLiteral("activeNetworkName"),
         network == nullptr ? QString() : displayName(*network)},
        {QStringLiteral("disconnectAvailable"),
         m_client.operationAdmissionReady() && verdict.allowed},
        {QStringLiteral("disconnectBlockedReason"), verdict.reasonCode},
    });
  }
  return rows;
}

QVariantList NetworkSettingsModel::accessPoints() const {
  QVariantList rows;
  const ModelState state = m_client.projection();
  rows.reserve(state.accessPoints.size());
  for (const auto &point : state.accessPoints) {
    const bool saved = !point.hidden && std::any_of(
        state.knownNetworks.cbegin(), state.knownNetworks.cend(),
        [&point](const KnownNetwork &network) {
          return !network.hidden && network.ssid == point.ssid
                 && network.security == point.security;
        });
    rows.append(QVariantMap{
        {QStringLiteral("deviceInterface"), point.deviceInterface},
        {QStringLiteral("displayName"),
         QindaQt::Network::Model::accessPointDisplayName(point)},
        {QStringLiteral("hidden"), point.hidden},
        {QStringLiteral("security"), static_cast<quint32>(point.security)},
        {QStringLiteral("securityText"), securityText(point.security)},
        {QStringLiteral("frequencyMHz"), point.frequencyMHz},
        {QStringLiteral("signalStrength"), point.signalStrength},
        {QStringLiteral("saved"), saved},
    });
  }
  return rows;
}

QVariantList NetworkSettingsModel::knownNetworks() const {
  QVariantList rows;
  const ModelState state = m_client.projection();
  rows.reserve(state.knownNetworks.size());
  for (const KnownNetwork &network : state.knownNetworks) {
    const ActiveConnection *active = findActiveForNetwork(state, network.id);
    const auto verdict = m_client.model().connectKnown(
        QindaQt::Network::ConnectIntent{network.id});
    rows.append(QVariantMap{
        {QStringLiteral("id"), network.id},
        {QStringLiteral("displayName"), displayName(network)},
        {QStringLiteral("hidden"), network.hidden},
        {QStringLiteral("security"), static_cast<quint32>(network.security)},
        {QStringLiteral("securityText"), securityText(network.security)},
        {QStringLiteral("autoConnect"), network.autoConnect},
        {QStringLiteral("active"), active != nullptr},
        {QStringLiteral("activeDeviceInterface"),
         active == nullptr ? QString() : active->deviceInterface},
        {QStringLiteral("connectAvailable"),
         m_client.operationAdmissionReady() && verdict.allowed},
        {QStringLiteral("connectBlockedReason"), verdict.reasonCode},
        {QStringLiteral("mayRequireExternalCredentials"),
         network.security != SecuritySuite::Open},
    });
  }
  return rows;
}

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
          tr("Connection requested; awaiting authoritative state.");
      break;
    case OperationKind::DisconnectActive:
      m_operationStatusText =
          tr("Disconnection requested; awaiting authoritative state.");
      break;
    case OperationKind::SetRadio:
      m_operationStatusText = tr("Network operation completed.");
      break;
    }
  } else {
    m_operationStatusText.clear();
    m_localError = actionFailureText(result.reasonCode);
    if (result.kind == OperationKind::ConnectKnownNetwork) {
      m_localError += tr(" This page cannot request credentials; a registered "
                         "external NetworkManager secret agent must provide "
                         "them when required.");
    }
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
    return tr("Another network operation or current scan lease is active.");
  }
  if (reason == QStringLiteral("known-network-control-unsupported")) {
    return tr("Connecting saved networks is not permitted by the network service.");
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
  if (reason == QStringLiteral("device-not-connected")) {
    return tr("That network device is no longer connected.");
  }
  if (reason.isEmpty()) {
    return tr("The network request was rejected.");
  }
  return tr("The network request failed (%1).").arg(reason);
}

} // namespace QindaQt::Apps::SettingsNetwork
