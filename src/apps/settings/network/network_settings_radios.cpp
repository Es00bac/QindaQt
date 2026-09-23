// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_network/network_settings_model.h>

#include <algorithm>
#include <array>

namespace QindaQt::Apps::SettingsNetwork {
namespace {

using QindaQt::Network::Client::ClientState;
using QindaQt::Network::Model::ModelState;
using QindaQt::Network::Radio;
using QindaQt::Network::RadioKind;

constexpr int ReadbackRetryMilliseconds = 200;
constexpr int ReadbackDeadlineMilliseconds = 5'000;

QString radioName(const RadioKind kind) {
  return kind == RadioKind::Wifi
      ? QObject::tr("Wi-Fi") : QObject::tr("Mobile broadband");
}

const Radio *findRadio(const ModelState &state, const RadioKind kind) {
  const auto found = std::find_if(state.radios.cbegin(), state.radios.cend(),
                                  [kind](const Radio &radio) {
                                    return radio.kind == kind;
                                  });
  return found == state.radios.cend() ? nullptr : &*found;
}

} // namespace

QVariantList NetworkSettingsModel::radios() const {
  const ModelState state = m_client.projection();
  if (!state.hasSnapshot) return {};
  QVariantList rows;
  for (const RadioKind kind : std::array{RadioKind::Wifi, RadioKind::Wwan}) {
    const Radio *observed = findRadio(state, kind);
    const Radio radio = observed == nullptr
        ? Radio{kind, false, true, false} : *observed;
    const bool pending = m_pendingRadio && m_pendingRadio->kind == kind;
    const auto verdict = m_client.model().setRadio(
        QindaQt::Network::SetRadioIntent{kind, !radio.softwareEnabled});
    const bool controlAvailable = !m_pendingRadio
        && m_client.operationAdmissionReady() && verdict.allowed;
    QString status;
    if (!radio.present) {
      status = tr("Not present");
    } else if (!radio.hardwareEnabled) {
      status = radio.softwareEnabled
          ? tr("Software on; blocked by hardware")
          : tr("Software off; blocked by hardware");
    } else {
      status = radio.softwareEnabled ? tr("On") : tr("Off");
    }
    QString detail;
    if (pending) {
      detail = m_pendingRadio->awaitingReadback
          ? tr("Checking the saved radio state…")
          : (m_pendingRadio->enabled ? tr("Turning on…") : tr("Turning off…"));
    } else if (m_radioErrorKind && *m_radioErrorKind == kind
               && !m_radioError.isEmpty()) {
      detail = m_radioError;
    } else if (!controlAvailable) {
      detail = radioBlockedText(
          verdict.allowed ? QStringLiteral("client-not-ready")
                          : verdict.reasonCode);
    }
    rows.append(QVariantMap{
        {QStringLiteral("kind"), static_cast<quint32>(kind)},
        {QStringLiteral("name"), radioName(kind)},
        {QStringLiteral("present"), radio.present},
        {QStringLiteral("hardwareEnabled"), radio.hardwareEnabled},
        {QStringLiteral("softwareEnabled"), radio.softwareEnabled},
        {QStringLiteral("controlCapable"), state.radioControlCapable},
        {QStringLiteral("controlAvailable"), controlAvailable},
        {QStringLiteral("pending"), pending},
        {QStringLiteral("statusText"), status},
        {QStringLiteral("detailText"), detail},
    });
  }
  return rows;
}

bool NetworkSettingsModel::setRadio(const quint32 rawKind, const bool enabled) {
  if (rawKind > static_cast<quint32>(RadioKind::Wwan)) {
    rejectAction(QStringLiteral("radio-kind-invalid"));
    return false;
  }
  const RadioKind kind = static_cast<RadioKind>(rawKind);
  if (m_pendingRadio) {
    m_radioErrorKind = kind;
    m_radioError = tr("A radio change is still awaiting confirmation.");
    Q_EMIT actionRejected(QStringLiteral("operation-in-flight"));
    Q_EMIT viewChanged();
    return false;
  }
  QString error;
  if (!m_client.setRadio(kind, enabled, &error)) {
    m_radioErrorKind = kind;
    m_radioError = actionFailureText(error);
    m_operationStatusText.clear();
    Q_EMIT actionRejected(error);
    Q_EMIT viewChanged();
    return false;
  }
  const ModelState state = m_client.projection();
  m_pendingRadio = PendingRadio{kind, enabled, state.owner, state.epoch,
                                state.revision, false};
  m_radioErrorKind.reset();
  m_radioError.clear();
  m_localError.clear();
  beginOperationMessage(QindaQt::Network::OperationKind::SetRadio);
  return true;
}

void NetworkSettingsModel::clearRadioPending() {
  m_radioReadbackRetry.stop();
  m_radioReadbackDeadline.stop();
  m_pendingRadio.reset();
}

void NetworkSettingsModel::finishRadioOperation(
    const QindaQt::Network::OperationResult &result) {
  if (!m_pendingRadio) return;
  if (result.initiatingEpoch != m_pendingRadio->epoch
      || result.initiatingRevision != m_pendingRadio->revision) {
    finishRadioUncertain(tr("The radio response did not match the requested change."));
    return;
  }
  if (result.status == QindaQt::Network::OperationStatus::Succeeded) {
    m_pendingRadio->awaitingReadback = true;
    m_operationStatusText = tr("Checking the radio state reported by the service…");
    m_radioReadbackDeadline.start(ReadbackDeadlineMilliseconds);
    m_radioReadbackRetry.start(ReadbackRetryMilliseconds);
    Q_EMIT viewChanged();
    return;
  }
  const RadioKind kind = m_pendingRadio->kind;
  clearRadioPending();
  m_radioErrorKind = kind;
  m_radioError = actionFailureText(result.reasonCode);
  m_operationStatusText.clear();
  Q_EMIT viewChanged();
}

void NetworkSettingsModel::finishRadioUncertain(const QString &message) {
  if (!m_pendingRadio) return;
  const RadioKind kind = m_pendingRadio->kind;
  clearRadioPending();
  m_radioErrorKind = kind;
  m_radioError = message.isEmpty()
      ? tr("The radio change outcome is uncertain. Refresh to check its state.")
      : message;
  m_operationStatusText.clear();
  Q_EMIT viewChanged();
}

void NetworkSettingsModel::handleRadioSnapshot() {
  if (!m_pendingRadio || !m_pendingRadio->awaitingReadback) return;
  const ModelState state = m_client.projection();
  if (state.owner != m_pendingRadio->owner
      || state.epoch != m_pendingRadio->epoch) {
    finishRadioUncertain(tr("The network service changed before the radio state was confirmed."));
    return;
  }
  if (m_client.state() != ClientState::Ready) {
    finishRadioUncertain(tr("The radio state could not be confirmed from a ready service."));
    return;
  }
  if (state.revision <= m_pendingRadio->revision) {
    // AGENT-GUARD: NetworkClient can accept an unchanged post-operation
    // snapshot as Ready. An operation reply never proves a radio's new state.
    m_radioReadbackRetry.start(ReadbackRetryMilliseconds);
    return;
  }
  const Radio *radio = findRadio(state, m_pendingRadio->kind);
  const RadioKind kind = m_pendingRadio->kind;
  const bool requested = m_pendingRadio->enabled;
  clearRadioPending();
  if (radio != nullptr && radio->softwareEnabled == requested) {
    m_radioErrorKind.reset();
    m_radioError.clear();
    m_operationStatusText = requested
        ? tr("%1 is on.").arg(radioName(kind))
        : tr("%1 is off.").arg(radioName(kind));
  } else {
    m_radioErrorKind = kind;
    m_radioError = tr("The radio state differs from the requested change.");
    m_operationStatusText.clear();
  }
}

void NetworkSettingsModel::handleRadioClientState() {
  if (!m_pendingRadio) return;
  const ModelState state = m_client.projection();
  if (state.owner != m_pendingRadio->owner
      || m_client.state() == ClientState::Unavailable
      || m_client.state() == ClientState::Degraded) {
    finishRadioUncertain(tr("The network service changed before the radio state was confirmed."));
  }
}

QString NetworkSettingsModel::radioBlockedText(const QString &reason) const {
  if (reason == QStringLiteral("radio-absent")) return tr("This radio is not present.");
  if (reason == QStringLiteral("radio-hardware-disabled"))
    return tr("A hardware switch is blocking this radio.");
  if (reason == QStringLiteral("radio-control-unsupported"))
    return tr("The network service does not permit changing this radio.");
  if (reason == QStringLiteral("radio-already-in-state")) return {};
  return tr("Wait for current network information before changing this radio.");
}

} // namespace QindaQt::Apps::SettingsNetwork
