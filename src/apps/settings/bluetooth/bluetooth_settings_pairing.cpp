// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>

#include "bluetooth_settings_projection.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <algorithm>

namespace QindaQt::Apps::SettingsBluetooth {
namespace {

using namespace QindaQt::Bluetooth;

QString promptKindToken(const PairingPromptKind kind) {
  switch (kind) {
  case PairingPromptKind::ConfirmPasskey: return QStringLiteral("confirm-passkey");
  case PairingPromptKind::EnterPasskey: return QStringLiteral("enter-passkey");
  case PairingPromptKind::EnterPin: return QStringLiteral("enter-pin");
  case PairingPromptKind::DisplayPasskey: return QStringLiteral("display-passkey");
  case PairingPromptKind::DisplayPin: return QStringLiteral("display-pin");
  case PairingPromptKind::AuthorizeService: return QStringLiteral("authorize-service");
  case PairingPromptKind::None: return {};
  }
  return {};
}

} // namespace

QVariantList BluetoothSettingsModel::devices() const {
  QVariantList rows;
  if (!exactSnapshotReady()) return rows;
  const Snapshot snapshot = m_client.snapshot();
  rows.reserve(snapshot.devices.size());
  for (const Device &device : snapshot.devices) {
    const QString kind = Projection::deviceClassLabel(device.deviceClass);
    const QString label = Projection::boundedNonAddressName(device.name, kind);
    const OperationRequest connect{OperationKind::Connect, device.handle, false};
    const OperationRequest disconnect{OperationKind::Disconnect,
                                      device.handle, false};
    const OperationRequest pair{.kind = OperationKind::Pair,
                                .target = device.handle};
    const OperationRequest remove{.kind = OperationKind::RemoveDevice,
                                  .target = device.handle};
    const OperationRequest trust{.kind = OperationKind::SetTrusted,
                                 .target = device.handle,
                                 .trusted = !device.trusted};
    QString state = device.paired ? tr("paired") : tr("not paired");
    state += device.connected ? tr(", connected") : tr(", disconnected");
    if (device.rssiKnown) state += tr(", signal %1 dBm").arg(device.rssi);
    rows.append(QVariantMap{
        {QStringLiteral("id"), Projection::deviceRowId(device.handle)},
        {QStringLiteral("adapterId"), Projection::adapterRowId(device.adapterHandle)},
        {QStringLiteral("label"), label},
        {QStringLiteral("classLabel"), kind},
        {QStringLiteral("iconName"), Projection::deviceIconName(device.deviceClass)},
        {QStringLiteral("iconText"), Projection::deviceIconText(device.deviceClass)},
        {QStringLiteral("paired"), device.paired},
        {QStringLiteral("connected"), device.connected},
        {QStringLiteral("trusted"), device.trusted},
        {QStringLiteral("rssiKnown"), device.rssiKnown},
        {QStringLiteral("rssi"), device.rssi},
        {QStringLiteral("connectAvailable"), admissionReason(connect).isEmpty()},
        {QStringLiteral("disconnectAvailable"), admissionReason(disconnect).isEmpty()},
        {QStringLiteral("pairAvailable"), admissionReason(pair).isEmpty()},
        {QStringLiteral("forgetAvailable"), admissionReason(remove).isEmpty()},
        {QStringLiteral("trustAvailable"), admissionReason(trust).isEmpty()},
        {QStringLiteral("accessibleDescription"), state},
    });
  }
  return rows;
}

QVariantMap BluetoothSettingsModel::pairingPrompt() const {
  if (!exactSnapshotReady()) return {};
  const Snapshot snapshot = m_client.snapshot();
  const PairingPrompt &prompt = snapshot.pairingPrompt;
  if (!prompt.active()) return {};
  const auto device = std::ranges::find_if(
      snapshot.devices, [&prompt](const Device &candidate) {
        return candidate.handle == prompt.device;
      });
  if (device == snapshot.devices.cend()) return {};
  const QString label = Projection::boundedNonAddressName(
      device->name, Projection::deviceClassLabel(device->deviceClass));
  return {
      {QStringLiteral("active"), true},
      {QStringLiteral("kind"), promptKindToken(prompt.kind)},
      {QStringLiteral("deviceLabel"), label},
      {QStringLiteral("detail"), prompt.detail},
      {QStringLiteral("serviceUuid"), prompt.serviceUuid},
      {QStringLiteral("entered"), prompt.entered},
      {QStringLiteral("confirmationAvailable"),
       prompt.kind == PairingPromptKind::ConfirmPasskey
           || prompt.kind == PairingPromptKind::AuthorizeService},
      {QStringLiteral("passkeyInput"),
       prompt.kind == PairingPromptKind::EnterPasskey},
      {QStringLiteral("pinInput"), prompt.kind == PairingPromptKind::EnterPin},
  };
}

bool BluetoothSettingsModel::requestPairing(const QString &id) {
  const auto device = findDevice(id);
  if (!device) { reject(QStringLiteral("stale-handle")); return false; }
  return dispatch({.kind = OperationKind::Pair, .target = device->handle});
}

bool BluetoothSettingsModel::requestForget(const QString &id) {
  const auto device = findDevice(id);
  if (!device) { reject(QStringLiteral("stale-handle")); return false; }
  return dispatch({.kind = OperationKind::RemoveDevice, .target = device->handle});
}

bool BluetoothSettingsModel::requestTrust(const QString &id, const bool trusted) {
  const auto device = findDevice(id);
  if (!device) { reject(QStringLiteral("stale-handle")); return false; }
  return dispatch({.kind = OperationKind::SetTrusted,
                   .target = device->handle,
                   .trusted = trusted});
}

bool BluetoothSettingsModel::dispatchPrompt(
    const OperationRequest &request, const std::function<quint64()> &sender) {
  if (!m_routeActive) { reject(QStringLiteral("route-inactive")); return false; }
  if (m_promptPending) { reject(QStringLiteral("operation-busy")); return false; }
  if (!exactSnapshotReady() || !validateOperationRequest(request).accepted) {
    reject(QStringLiteral("malformed-request"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  if (!snapshot.pairingPrompt.active()
      || snapshot.pairingPrompt.device != request.target) {
    reject(QStringLiteral("no-prompt"));
    return false;
  }
  const quint64 requestId = sender();
  if (requestId == 0) { reject(QStringLiteral("request-id-exhausted")); return false; }
  m_promptPending = PendingOperation{requestId, m_client.owner(), request,
                                     snapshot.epoch, snapshot.revision};
  m_errorText.clear();
  m_operationStatusText = tr("Sending pairing response…");
  Q_EMIT viewChanged();
  return true;
}

bool BluetoothSettingsModel::replyConfirmation(const bool accepted) {
  if (!exactSnapshotReady()) { reject(QStringLiteral("unavailable")); return false; }
  const PairingPrompt prompt = m_client.snapshot().pairingPrompt;
  OperationRequest request{.kind = OperationKind::ReplyConfirmation,
                           .target = prompt.device,
                           .accepted = accepted};
  return dispatchPrompt(request, [this, accepted] {
    return m_client.replyConfirmation(accepted);
  });
}

bool BluetoothSettingsModel::replyPasskey(const QString &passkey) {
  if (!exactSnapshotReady()) { reject(QStringLiteral("unavailable")); return false; }
  OperationRequest request{.kind = OperationKind::ReplyPasskey,
                           .target = m_client.snapshot().pairingPrompt.device};
  if (!setPairingInput(request.input, request.inputSize, passkey)) {
    reject(QStringLiteral("malformed-request"));
    return false;
  }
  return dispatchPrompt(request, [this, passkey] {
    return m_client.replyPasskey(passkey);
  });
}

bool BluetoothSettingsModel::replyPin(const QString &pin) {
  if (!exactSnapshotReady()) { reject(QStringLiteral("unavailable")); return false; }
  OperationRequest request{.kind = OperationKind::ReplyPin,
                           .target = m_client.snapshot().pairingPrompt.device};
  if (!setPairingInput(request.input, request.inputSize, pin)) {
    reject(QStringLiteral("malformed-request"));
    return false;
  }
  return dispatchPrompt(request, [this, pin] { return m_client.replyPin(pin); });
}

bool BluetoothSettingsModel::cancelPrompt() {
  if (!exactSnapshotReady()) { reject(QStringLiteral("unavailable")); return false; }
  const OperationRequest request{
      .kind = OperationKind::CancelPrompt,
      .target = m_client.snapshot().pairingPrompt.device,
  };
  return dispatchPrompt(request, [this] { return m_client.cancelPrompt(); });
}

} // namespace QindaQt::Apps::SettingsBluetooth
