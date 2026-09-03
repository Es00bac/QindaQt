// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>

#include "bluetooth_settings_projection.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QVariantMap>

#include <algorithm>

namespace QindaQt::Apps::SettingsBluetooth {
namespace {

using namespace QindaQt::Bluetooth;

const Adapter *findSnapshotAdapter(const Snapshot &snapshot,
                                   const Handle &handle) {
  const auto found = std::ranges::find_if(
      snapshot.adapters, [&handle](const Adapter &item) {
        return item.handle == handle;
      });
  return found == snapshot.adapters.cend() ? nullptr : &*found;
}

const Device *findSnapshotDevice(const Snapshot &snapshot,
                                 const Handle &handle) {
  const auto found = std::ranges::find_if(
      snapshot.devices, [&handle](const Device &item) {
        return item.handle == handle;
      });
  return found == snapshot.devices.cend() ? nullptr : &*found;
}

} // namespace

BluetoothSettingsModel::BluetoothSettingsModel(BluetoothClient &client,
                                               QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client, &BluetoothClient::stateChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &BluetoothClient::snapshotChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &BluetoothClient::operationCompleted, this,
          &BluetoothSettingsModel::handleOperationCompleted);
}

BluetoothSettingsModel::~BluetoothSettingsModel() {
  setRouteActive(false);
}

bool BluetoothSettingsModel::loading() const noexcept {
  return m_client.state() == ClientState::Starting
      || m_client.state() == ClientState::Stopped;
}

bool BluetoothSettingsModel::ready() const noexcept {
  return exactSnapshotReady();
}

bool BluetoothSettingsModel::degraded() const noexcept {
  return m_client.state() == ClientState::Degraded;
}

bool BluetoothSettingsModel::unavailable() const noexcept {
  return m_client.state() == ClientState::Unavailable;
}

bool BluetoothSettingsModel::busy() const noexcept {
  return m_pending.has_value() || m_promptPending.has_value()
      || m_convergence.has_value()
      || m_client.operationPending();
}

bool BluetoothSettingsModel::pairingSupported() const noexcept {
  return exactSnapshotReady()
      && m_client.snapshot().capabilities.testFlag(Capability::Pair)
      && m_client.snapshot().capabilities.testFlag(Capability::PairingPrompt);
}

bool BluetoothSettingsModel::departureReleasePending() const noexcept {
  if (m_routeActive) return false;
  if (m_pending && (m_pending->request.kind == OperationKind::AcquireDiscovery
                    || m_pending->request.kind
                           == OperationKind::ReleaseDiscovery))
    return true;
  // A release that cannot be admitted is completed by Bluetooth1's
  // caller-disappearance contract when the process closes; do not trap an
  // unavailable window indefinitely waiting for an operation we cannot send.
  return m_releaseRequested && m_discoveryLease.has_value()
      && !m_automaticReleaseBlocked
      && exactSnapshotReady();
}

bool BluetoothSettingsModel::exactSnapshotReady() const noexcept {
  if (m_client.state() != ClientState::Ready || m_client.owner().isEmpty()
      || !m_client.hasSnapshot()) {
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  return snapshot.availability == Availability::Ready
      && snapshot.epoch != 0 && snapshot.revision != 0
      && validateSnapshot(snapshot).accepted;
}

QString BluetoothSettingsModel::statusText() const {
  if (loading()) return tr("Connecting to the Bluetooth service…");
  if (ready()) return tr("Bluetooth information is current.");
  if (degraded()) return tr("Bluetooth information could not be verified.");
  return tr("The Bluetooth service is unavailable.");
}

QString BluetoothSettingsModel::serviceOwner() const {
  return exactSnapshotReady() ? m_client.owner() : QString{};
}

qulonglong BluetoothSettingsModel::serviceEpoch() const {
  return exactSnapshotReady() ? m_client.snapshot().epoch : 0;
}

qulonglong BluetoothSettingsModel::serviceRevision() const {
  return exactSnapshotReady() ? m_client.snapshot().revision : 0;
}

std::optional<Adapter>
BluetoothSettingsModel::findAdapter(const QString &rowId) const {
  if (!exactSnapshotReady()) return std::nullopt;
  const Snapshot snapshot = m_client.snapshot();
  const auto found = std::ranges::find_if(
      snapshot.adapters, [&rowId](const Adapter &item) {
        return Projection::adapterRowId(item.handle) == rowId;
      });
  return found == snapshot.adapters.cend()
      ? std::nullopt : std::optional<Adapter>(*found);
}

std::optional<Device>
BluetoothSettingsModel::findDevice(const QString &rowId) const {
  if (!exactSnapshotReady()) return std::nullopt;
  const Snapshot snapshot = m_client.snapshot();
  const auto found = std::ranges::find_if(
      snapshot.devices, [&rowId](const Device &item) {
        return Projection::deviceRowId(item.handle) == rowId;
      });
  return found == snapshot.devices.cend()
      ? std::nullopt : std::optional<Device>(*found);
}

QString BluetoothSettingsModel::admissionReason(
    const OperationRequest &request) const {
  if (!m_routeActive && request.kind != OperationKind::ReleaseDiscovery)
    return QStringLiteral("route-inactive");
  if (busy()) return QStringLiteral("operation-busy");
  if (!exactSnapshotReady()) return QStringLiteral("unavailable");
  const Snapshot snapshot = m_client.snapshot();
  if (!validateOperationRequest(request).accepted
      || request.target.epoch != snapshot.epoch)
    return QStringLiteral("stale-handle");

  switch (request.kind) {
  case OperationKind::SetAdapterPower: {
    const Adapter *adapter = findSnapshotAdapter(snapshot, request.target);
    if (!snapshot.capabilities.testFlag(Capability::SetAdapterPower))
      return QStringLiteral("unsupported");
    if (adapter == nullptr) return QStringLiteral("stale-handle");
    return adapter->powered == request.powered
        ? QStringLiteral("already-in-state") : QString{};
  }
  case OperationKind::AcquireDiscovery: {
    const Adapter *adapter = findSnapshotAdapter(snapshot, request.target);
    if (!snapshot.capabilities.testFlag(Capability::DiscoveryLease))
      return QStringLiteral("unsupported");
    if (adapter == nullptr) return QStringLiteral("stale-handle");
    if (!adapter->powered) return QStringLiteral("adapter-off");
    return m_discoveryLease.has_value()
        ? QStringLiteral("lease-already-held") : QString{};
  }
  case OperationKind::ReleaseDiscovery:
    if (!snapshot.capabilities.testFlag(Capability::DiscoveryLease))
      return QStringLiteral("unsupported");
    if (findSnapshotAdapter(snapshot, request.target) == nullptr)
      return QStringLiteral("stale-handle");
    return m_discoveryLease == request.target
        ? QString{} : QStringLiteral("no-lease");
  case OperationKind::Connect: {
    const Device *device = findSnapshotDevice(snapshot, request.target);
    const Adapter *adapter = device == nullptr
        ? nullptr : findSnapshotAdapter(snapshot, device->adapterHandle);
    if (!snapshot.capabilities.testFlag(Capability::ConnectPaired))
      return QStringLiteral("unsupported");
    if (device == nullptr || adapter == nullptr)
      return QStringLiteral("stale-handle");
    if (!device->paired) return QStringLiteral("not-paired");
    if (device->connected) return QStringLiteral("already-connected");
    return adapter->powered ? QString{} : QStringLiteral("adapter-off");
  }
  case OperationKind::Disconnect: {
    const Device *device = findSnapshotDevice(snapshot, request.target);
    if (!snapshot.capabilities.testFlag(Capability::DisconnectPaired))
      return QStringLiteral("unsupported");
    if (device == nullptr) return QStringLiteral("stale-handle");
    return device->connected ? QString{} : QStringLiteral("not-connected");
  }
  case OperationKind::Pair: {
    const Device *device = findSnapshotDevice(snapshot, request.target);
    const Adapter *adapter = device == nullptr
        ? nullptr : findSnapshotAdapter(snapshot, device->adapterHandle);
    if (!snapshot.capabilities.testFlag(Capability::Pair))
      return QStringLiteral("unsupported");
    if (device == nullptr || adapter == nullptr)
      return QStringLiteral("stale-handle");
    if (device->paired) return QStringLiteral("already-paired");
    return adapter->powered ? QString{} : QStringLiteral("adapter-off");
  }
  case OperationKind::RemoveDevice: {
    const Device *device = findSnapshotDevice(snapshot, request.target);
    if (!snapshot.capabilities.testFlag(Capability::RemoveDevice))
      return QStringLiteral("unsupported");
    if (device == nullptr) return QStringLiteral("stale-handle");
    return device->paired ? QString{} : QStringLiteral("not-paired");
  }
  case OperationKind::SetTrusted: {
    const Device *device = findSnapshotDevice(snapshot, request.target);
    if (!snapshot.capabilities.testFlag(Capability::SetTrusted))
      return QStringLiteral("unsupported");
    if (device == nullptr) return QStringLiteral("stale-handle");
    if (!device->paired) return QStringLiteral("not-paired");
    return device->trusted == request.trusted
        ? QStringLiteral("already-set") : QString{};
  }
  case OperationKind::CancelPairing:
  case OperationKind::ReplyConfirmation:
  case OperationKind::ReplyPasskey:
  case OperationKind::ReplyPin:
  case OperationKind::CancelPrompt:
    return QStringLiteral("malformed-request");
  }
  return QStringLiteral("malformed-request");
}

QVariantList BluetoothSettingsModel::adapters() const {
  QVariantList rows;
  if (!exactSnapshotReady()) return rows;
  const Snapshot snapshot = m_client.snapshot();
  rows.reserve(snapshot.adapters.size());
  qsizetype ordinal = 1;
  for (const Adapter &adapter : snapshot.adapters) {
    const OperationRequest power{OperationKind::SetAdapterPower,
                                 adapter.handle, !adapter.powered};
    const OperationRequest acquire{OperationKind::AcquireDiscovery,
                                   adapter.handle, false};
    const OperationRequest release{OperationKind::ReleaseDiscovery,
                                   adapter.handle, false};
    const bool ownedLease = m_discoveryLease == adapter.handle;
    const QString label = Projection::boundedNonAddressName(
        adapter.name, tr("Bluetooth adapter %1").arg(ordinal++));
    rows.append(QVariantMap{
        {QStringLiteral("id"), Projection::adapterRowId(adapter.handle)},
        {QStringLiteral("label"), label},
        {QStringLiteral("powered"), adapter.powered},
        {QStringLiteral("discovering"), adapter.discovering},
        {QStringLiteral("discoveryLeaseOwned"), ownedLease},
        {QStringLiteral("powerAvailable"), admissionReason(power).isEmpty()},
        {QStringLiteral("startDiscoveryAvailable"),
         admissionReason(acquire).isEmpty()},
        {QStringLiteral("stopDiscoveryAvailable"),
         admissionReason(release).isEmpty()},
        {QStringLiteral("accessibleDescription"),
         tr("%1, %2%3").arg(adapter.powered ? tr("powered on")
                                            : tr("powered off"),
                              adapter.discovering ? tr("discovering")
                                                  : tr("not discovering"),
                              ownedLease ? tr(", discovery requested by this page")
                                         : QString{})},
    });
  }
  return rows;
}

bool BluetoothSettingsModel::dispatch(const OperationRequest &request) {
  const QString reason = admissionReason(request);
  if (!reason.isEmpty()) {
    reject(reason);
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  quint64 requestId = 0;
  switch (request.kind) {
  case OperationKind::SetAdapterPower:
    requestId = m_client.setAdapterPower(request.target, request.powered); break;
  case OperationKind::AcquireDiscovery:
    requestId = m_client.acquireDiscovery(request.target); break;
  case OperationKind::ReleaseDiscovery:
    requestId = m_client.releaseDiscovery(request.target); break;
  case OperationKind::Connect:
    requestId = m_client.connectDevice(request.target); break;
  case OperationKind::Disconnect:
    requestId = m_client.disconnectDevice(request.target); break;
  case OperationKind::Pair:
    requestId = m_client.pairDevice(request.target); break;
  case OperationKind::RemoveDevice:
    requestId = m_client.removeDevice(request.target); break;
  case OperationKind::SetTrusted:
    requestId = m_client.setTrusted(request.target, request.trusted); break;
  case OperationKind::CancelPairing:
  case OperationKind::ReplyConfirmation:
  case OperationKind::ReplyPasskey:
  case OperationKind::ReplyPin:
  case OperationKind::CancelPrompt:
    break;
  }
  if (requestId == 0) {
    reject(QStringLiteral("request-id-exhausted"));
    return false;
  }
  m_pending = PendingOperation{requestId, m_client.owner(), request,
                               snapshot.epoch, snapshot.revision};
  m_errorText.clear();
  m_operationStatusText = tr("Bluetooth operation in progress…");
  Q_EMIT viewChanged();
  return true;
}

bool BluetoothSettingsModel::requestAdapterPower(const QString &id,
                                                  const bool powered) {
  const auto adapter = findAdapter(id);
  if (!adapter) { reject(QStringLiteral("stale-handle")); return false; }
  return dispatch({OperationKind::SetAdapterPower, adapter->handle, powered});
}

bool BluetoothSettingsModel::requestDiscovery(const QString &id,
                                               const bool enabled) {
  const auto adapter = findAdapter(id);
  if (!adapter) { reject(QStringLiteral("stale-handle")); return false; }
  if (!enabled) m_automaticReleaseBlocked = false;
  return dispatch({enabled ? OperationKind::AcquireDiscovery
                           : OperationKind::ReleaseDiscovery,
                   adapter->handle, false});
}

bool BluetoothSettingsModel::requestDeviceConnection(const QString &id,
                                                      const bool connected) {
  const auto device = findDevice(id);
  if (!device) { reject(QStringLiteral("stale-handle")); return false; }
  return dispatch({connected ? OperationKind::Connect
                             : OperationKind::Disconnect,
                   device->handle, false});
}

void BluetoothSettingsModel::setRouteActive(const bool active) {
  if (m_routeActive == active) return;
  m_routeActive = active;
  if (active) {
    m_releaseRequested = false;
    m_automaticReleaseBlocked = false;
  } else if (m_discoveryLease.has_value()
             || (m_pending && m_pending->request.kind
                 == OperationKind::AcquireDiscovery)) {
    m_releaseRequested = true;
  }
  Q_EMIT viewChanged();
  if (!active) tryAutomaticRelease();
}

void BluetoothSettingsModel::handleOperationCompleted(
    const quint64 requestId, const OperationResult &result) {
  if (m_promptPending && m_promptPending->requestId == requestId) {
    const PendingOperation pending = *m_promptPending;
    m_promptPending.reset();
    const bool exact = m_client.owner() == pending.owner && result.wireValid
        && validateOperationResult(result).accepted
        && result.kind == pending.request.kind
        && result.initiatingEpoch == pending.epoch
        && result.initiatingRevision == pending.revision;
    if (!exact || result.status == OperationStatus::Uncertain) {
      m_errorText = tr("The pairing response is uncertain. Check the current prompt before trying again.");
    } else if (result.status != OperationStatus::Succeeded) {
      m_errorText = failureText(result);
    } else {
      m_errorText.clear();
      m_operationStatusText = tr("Pairing response sent.");
    }
    synchronizeAuthority();
    return;
  }
  if (!m_pending || m_pending->requestId != requestId) return;
  const PendingOperation pending = *m_pending;
  m_pending.reset();
  const bool exact = m_client.owner() == pending.owner && result.wireValid
      && validateOperationResult(result).accepted
      && result.kind == pending.request.kind
      && result.initiatingEpoch == pending.epoch
      && result.initiatingRevision == pending.revision;
  if (!exact || result.status == OperationStatus::Uncertain) {
    m_convergence.reset();
    m_operationStatusText.clear();
    m_errorText = tr("The Bluetooth result is uncertain. It was not replayed; check current state before trying again.");
  } else if (result.status != OperationStatus::Succeeded) {
    m_convergence.reset();
    m_operationStatusText.clear();
    m_errorText = failureText(result);
  } else {
    m_convergence = SuccessConvergence{pending.owner, result.observedEpoch,
                                       result.observedRevision};
    m_errorText.clear();
    m_operationStatusText = tr("Waiting for authoritative Bluetooth state…");
    if (pending.request.kind == OperationKind::AcquireDiscovery) {
      m_discoveryLease = pending.request.target;
      m_discoveryLeaseOwner = pending.owner;
      m_discoveryLeaseMinimumRevision = result.observedRevision;
    } else if (pending.request.kind == OperationKind::ReleaseDiscovery) {
      m_discoveryLease.reset();
      m_discoveryLeaseOwner.clear();
      m_discoveryLeaseMinimumRevision = 0;
      m_releaseRequested = false;
      m_automaticReleaseBlocked = false;
    }
  }
  if (pending.request.kind == OperationKind::ReleaseDiscovery
      && (!exact || result.status != OperationStatus::Succeeded)) {
    m_automaticReleaseBlocked = true;
    m_releaseRequested = false;
  }
  // AGENT-GUARD: Departure may wait for an admitted acquire, but a terminal
  // non-success cannot leave a release request behind when no lease exists;
  // Main.qml otherwise rejects every later window close.
  if (pending.request.kind == OperationKind::AcquireDiscovery
      && !m_discoveryLease.has_value()) {
    m_releaseRequested = false;
    m_automaticReleaseBlocked = false;
  }
  synchronizeAuthority();
}

void BluetoothSettingsModel::retireLeaseFromCurrentTruth() {
  if (!m_discoveryLease) return;
  if (m_client.owner().isEmpty() || m_client.owner() != m_discoveryLeaseOwner) {
    m_discoveryLease.reset();
  } else if (m_client.hasSnapshot()) {
    const Snapshot snapshot = m_client.snapshot();
    const Adapter *adapter = findSnapshotAdapter(snapshot, *m_discoveryLease);
    if (snapshot.epoch != m_discoveryLease->epoch || adapter == nullptr
        || !adapter->powered
        || (snapshot.revision >= m_discoveryLeaseMinimumRevision
            && !adapter->discovering)) {
      m_discoveryLease.reset();
    }
  }
  if (!m_discoveryLease) {
    m_discoveryLeaseOwner.clear();
    m_discoveryLeaseMinimumRevision = 0;
    m_releaseRequested = false;
    m_automaticReleaseBlocked = false;
  }
}

void BluetoothSettingsModel::synchronizeAuthority() {
  if (m_promptPending
      && (m_client.owner() != m_promptPending->owner
          || (m_client.hasSnapshot()
              && m_client.snapshot().epoch != m_promptPending->epoch))) {
    m_promptPending.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Bluetooth authority changed. The pairing response was not replayed.");
  }
  if (m_pending && (m_client.owner() != m_pending->owner
                    || (m_client.hasSnapshot()
                        && m_client.snapshot().epoch != m_pending->epoch))) {
    const bool retiredAcquire =
        m_pending->request.kind == OperationKind::AcquireDiscovery;
    m_pending.reset();
    m_convergence.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Bluetooth authority changed. The operation was not replayed.");
    if (retiredAcquire && !m_discoveryLease.has_value()) {
      m_releaseRequested = false;
      m_automaticReleaseBlocked = false;
    }
  }
  retireLeaseFromCurrentTruth();
  if (m_convergence) {
    if (!exactSnapshotReady() || m_client.owner() != m_convergence->owner
        || m_client.snapshot().epoch != m_convergence->epoch) {
      m_convergence.reset();
      m_operationStatusText.clear();
      m_errorText = tr("Bluetooth authority changed before the result could be confirmed.");
    } else if (m_client.snapshot().revision
               >= m_convergence->minimumRevision) {
      m_convergence.reset();
      m_operationStatusText = tr("Bluetooth state updated.");
    }
  }
  Q_EMIT viewChanged();
  tryAutomaticRelease();
}

void BluetoothSettingsModel::tryAutomaticRelease() {
  if (!m_releaseRequested || m_automaticReleaseBlocked || busy()
      || !m_discoveryLease) return;
  if (!exactSnapshotReady()) return;
  if (m_client.owner() != m_discoveryLeaseOwner) {
    retireLeaseFromCurrentTruth();
    Q_EMIT viewChanged();
    return;
  }
  const OperationRequest release{OperationKind::ReleaseDiscovery,
                                 *m_discoveryLease, false};
  if (!dispatch(release)) {
    m_automaticReleaseBlocked = true;
    m_releaseRequested = false;
  }
}

void BluetoothSettingsModel::reject(const QString &reason) {
  m_operationStatusText.clear();
  m_errorText = tr("The Bluetooth request was not admitted (%1).").arg(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString BluetoothSettingsModel::failureText(const OperationResult &result) const {
  switch (result.status) {
  case OperationStatus::Rejected:
    return tr("The Bluetooth request was rejected (%1).").arg(result.reasonCode);
  case OperationStatus::Unsupported:
    return tr("That Bluetooth operation is not supported.");
  case OperationStatus::Busy:
    return tr("Bluetooth is busy; wait for the current operation to finish.");
  case OperationStatus::Failed:
    return tr("The Bluetooth operation failed (%1).").arg(result.reasonCode);
  case OperationStatus::Uncertain:
    return tr("The Bluetooth operation result is uncertain.");
  case OperationStatus::Succeeded:
    return {};
  }
  return tr("The Bluetooth result could not be understood.");
}

} // namespace QindaQt::Apps::SettingsBluetooth
