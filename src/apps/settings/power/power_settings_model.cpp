// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/power_settings_model.h>

#include "power_settings_projection.h"

#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsPower {
namespace {

using Power::Availability;
using Power::Capability;
using Power::OperationKind;
using Power::OperationResult;
using Power::OperationStatus;
using Power::PowerClientState;
using Power::Snapshot;

bool usableAvailability(const Availability availability) {
  return availability == Availability::Ready
      || availability == Availability::Degraded;
}

} // namespace

PowerSettingsModel::PowerSettingsModel(Power::PowerClient &client,
                                       QObject *sessionActions,
                                       QObject *parent)
    : QObject(parent), m_client(client), m_sessionActions(sessionActions) {
  m_debounceTimer.setSingleShot(true);
  m_debounceTimer.setInterval(120);
  m_convergenceTimer.setSingleShot(true);
  m_convergenceTimer.setInterval(5000);
  connect(&m_debounceTimer, &QTimer::timeout, this,
          &PowerSettingsModel::dispatchDebouncedBrightness);
  connect(&m_convergenceTimer, &QTimer::timeout, this, [this] {
    if (!m_convergence) return;
    m_convergence.reset();
    m_operationStatusText.clear();
    m_errorText = tr("The change was accepted but fresh power state did not arrive in time. It was not replayed.");
    Q_EMIT viewChanged();
  });
  connect(&m_client, &Power::PowerClient::stateChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &Power::PowerClient::snapshotChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &Power::PowerClient::operationCompleted, this,
          &PowerSettingsModel::handleOperationCompleted);
}

bool PowerSettingsModel::sessionActionsSupported() const noexcept {
  return m_sessionActions != nullptr;
}

QObject *PowerSettingsModel::sessionActions() const noexcept {
  return m_sessionActions;
}

bool PowerSettingsModel::hasDisplaySnapshot() const noexcept {
  if (m_client.owner().isEmpty() || !m_client.hasSnapshot()) return false;
  const Snapshot snapshot = m_client.snapshot();
  return snapshot.epoch != 0 && snapshot.revision != 0
      && Power::validateSnapshot(snapshot).accepted;
}

bool PowerSettingsModel::snapshotAdmitsBase() const noexcept {
  if (!hasDisplaySnapshot() || m_client.operationPending()) return false;
  const auto state = m_client.state();
  if (state != PowerClientState::Ready
      && state != PowerClientState::Degraded) return false;
  return usableAvailability(m_client.snapshot().availability);
}

bool PowerSettingsModel::loading() const noexcept {
  const auto state = m_client.state();
  return state == PowerClientState::Stopped || state == PowerClientState::Starting;
}

bool PowerSettingsModel::ready() const noexcept {
  return hasDisplaySnapshot() && m_client.state() == PowerClientState::Ready
      && m_client.snapshot().availability == Availability::Ready;
}

bool PowerSettingsModel::degraded() const noexcept {
  return hasDisplaySnapshot()
      && (m_client.state() == PowerClientState::Degraded
          || m_client.snapshot().availability == Availability::Degraded);
}

bool PowerSettingsModel::stale() const noexcept {
  return hasDisplaySnapshot()
      && m_client.state() == PowerClientState::Unavailable;
}

bool PowerSettingsModel::unavailable() const noexcept {
  return !loading() && !ready() && !degraded() && !stale();
}

bool PowerSettingsModel::busy() const noexcept {
  return m_debounce.has_value() || m_pending.has_value()
      || m_convergence.has_value() || m_client.operationPending();
}

bool PowerSettingsModel::retryAvailable() const noexcept { return !busy(); }

QString PowerSettingsModel::statusText() const {
  if (loading()) return tr("Connecting to the power service…");
  if (ready()) return tr("Choose a power mode and check battery and brightness settings.");
  if (degraded()) return tr("Some power features are unavailable on this computer. Available controls are shown below.");
  if (stale()) return tr("Power information is stale while the service recovers. Controls are unavailable.");
  return tr("The power service is unavailable.");
}

qulonglong PowerSettingsModel::serviceEpoch() const {
  return hasDisplaySnapshot() ? m_client.snapshot().epoch : 0;
}

qulonglong PowerSettingsModel::serviceRevision() const {
  return hasDisplaySnapshot() ? m_client.snapshot().revision : 0;
}

QVariantList PowerSettingsModel::supplyRows() const {
  return hasDisplaySnapshot() ? Projection::supplies(m_client.snapshot())
                              : QVariantList{};
}

QString PowerSettingsModel::profileAdmission(const QString &profileId) const {
  if (!snapshotAdmitsBase() || m_debounce || m_pending || m_convergence)
    return QStringLiteral("operation-busy-or-unavailable");
  const Snapshot snapshot = m_client.snapshot();
  if (!snapshot.capabilities.testFlag(Capability::Profiles))
    return QStringLiteral("unsupported");
  if (snapshot.profiles.activeProfileId == profileId)
    return QStringLiteral("already-active");
  for (const Power::Profile &profile : snapshot.profiles.supported)
    if (profile.id == profileId) return {};
  return QStringLiteral("unknown-profile");
}

QVariantList PowerSettingsModel::profileRows() const {
  QVariantList rows;
  if (!hasDisplaySnapshot()) return rows;
  const Snapshot snapshot = m_client.snapshot();
  for (const Power::Profile &profile : snapshot.profiles.supported) {
    const bool active = profile.id == snapshot.profiles.activeProfileId;
    rows.append(QVariantMap{
        {QStringLiteral("id"), profile.id},
        {QStringLiteral("label"), profile.label},
        {QStringLiteral("active"), active},
        {QStringLiteral("available"), profileAdmission(profile.id).isEmpty()},
        {QStringLiteral("accessibleDescription"), active
             ? tr("Current power profile")
             : tr("Select the %1 power profile").arg(profile.label)},
    });
  }
  return rows;
}

QVariantList PowerSettingsModel::profileHoldRows() const {
  return hasDisplaySnapshot()
      ? Projection::profileHolds(m_client.snapshot()) : QVariantList{};
}

QVariantList PowerSettingsModel::internalBrightnessRows() const {
  return hasDisplaySnapshot()
      ? Projection::internalBrightness(m_client.snapshot()) : QVariantList{};
}

QString PowerSettingsModel::brightnessAdmission(
    const QString &rowId, const bool replacingDebounce) const {
  if (m_pending || m_convergence || m_client.operationPending())
    return QStringLiteral("operation-busy");
  if (m_debounce && (!replacingDebounce || m_debounce->rowId != rowId))
    return QStringLiteral("operation-busy");
  if (!snapshotAdmitsBase()) return QStringLiteral("unavailable");
  const Snapshot snapshot = m_client.snapshot();
  if (!snapshot.capabilities.testFlag(Capability::KeyboardBacklight))
    return QStringLiteral("unsupported");
  const Power::KeyboardBacklight *device = findKeyboard(snapshot, rowId);
  if (device == nullptr || device->handle.epoch != snapshot.epoch)
    return QStringLiteral("stale-handle");
  if (!device->canSet || !device->valueKnown || device->maximum == 0)
    return QStringLiteral("unsupported");
  return {};
}

QVariantList PowerSettingsModel::keyboardBrightnessRows() const {
  if (!hasDisplaySnapshot()) return {};
  QVariantList rows = Projection::keyboardBrightness(m_client.snapshot());
  for (QVariant &value : rows) {
    QVariantMap row = value.toMap();
    const QString id = row.value(QStringLiteral("id")).toString();
    row.insert(QStringLiteral("available"),
               brightnessAdmission(id, true).isEmpty());
    value = row;
  }
  return rows;
}

const Power::KeyboardBacklight *PowerSettingsModel::findKeyboard(
    const Snapshot &snapshot, const QString &rowId) const {
  for (const Power::KeyboardBacklight &device : snapshot.keyboardBacklights)
    if (Projection::keyboardRowId(snapshot, device.handle) == rowId)
      return &device;
  return nullptr;
}

bool PowerSettingsModel::retry() {
  if (!retryAvailable()) {
    reject(QStringLiteral("operation-busy"));
    return false;
  }
  m_retrying = true;
  m_client.stop();
  m_client.start();
  m_errorText.clear();
  m_operationStatusText = tr("Reconnecting to the power service…");
  Q_EMIT viewChanged();
  return true;
}

bool PowerSettingsModel::requestProfile(const QString &profileId) {
  const QString reason = profileAdmission(profileId);
  if (!reason.isEmpty()) {
    reject(reason);
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const quint64 requestId = m_client.setProfile(profileId);
  if (requestId == 0) {
    reject(QStringLiteral("request-id-exhausted"));
    return false;
  }
  m_pending = PendingOperation{requestId, Intent::Profile, profileId,
                               m_client.owner(), snapshot.epoch,
                               snapshot.revision, 0};
  m_errorText.clear();
  m_operationStatusText = tr("Applying the power profile…");
  Q_EMIT viewChanged();
  return true;
}

bool PowerSettingsModel::requestKeyboardBrightness(const QString &rowId,
                                                    const int normalized) {
  const QString reason = brightnessAdmission(rowId, true);
  if (!reason.isEmpty() || normalized < 0
      || normalized > static_cast<int>(Power::kNormalizedBrightnessMaximum)) {
    reject(reason.isEmpty() ? QStringLiteral("invalid-brightness") : reason);
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const Power::KeyboardBacklight *device = findKeyboard(snapshot, rowId);
  if (device == nullptr) {
    reject(QStringLiteral("stale-handle"));
    return false;
  }
  const auto raw = Brightness::denormalizeRaw(
      0, device->maximum, static_cast<quint32>(normalized));
  // AGENT-GUARD: Returning a gesture to admitted observed truth must cancel
  // its queued predecessor. Checking both representations avoids an
  // idempotent raw write when multiple normalized positions round alike.
  if (normalized == static_cast<int>(device->normalized)
      || (raw.succeeded() && raw.value == device->value)) {
    m_debounceTimer.stop();
    m_debounce.reset();
    m_errorText.clear();
    m_operationStatusText.clear();
    Q_EMIT viewChanged();
    return true;
  }
  m_debounce = DebouncedBrightness{rowId, normalized, m_client.owner(),
                                   snapshot.epoch, snapshot.revision};
  m_debounceTimer.start();
  m_errorText.clear();
  m_operationStatusText = tr("Brightness change queued…");
  Q_EMIT viewChanged();
  return true;
}

void PowerSettingsModel::dispatchDebouncedBrightness() {
  if (!m_debounce) return;
  const DebouncedBrightness debounce = *m_debounce;
  m_debounce.reset();
  if (!hasDisplaySnapshot() || m_client.owner() != debounce.owner) {
    reject(QStringLiteral("authority-changed"));
    return;
  }
  const Snapshot snapshot = m_client.snapshot();
  if (snapshot.epoch != debounce.epoch || snapshot.revision != debounce.revision) {
    reject(QStringLiteral("stale-handle"));
    return;
  }
  const Power::KeyboardBacklight *device = findKeyboard(snapshot, debounce.rowId);
  if (device == nullptr || !device->canSet) {
    reject(QStringLiteral("stale-handle"));
    return;
  }
  const auto raw = Brightness::denormalizeRaw(
      0, device->maximum, static_cast<quint32>(debounce.normalized));
  if (!raw.succeeded()) {
    reject(QStringLiteral("invalid-brightness"));
    return;
  }
  if (device->valueKnown && raw.value == device->value) {
    m_operationStatusText.clear();
    Q_EMIT viewChanged();
    return;
  }
  const quint64 requestId = m_client.setKeyboardBrightness(device->handle,
                                                            raw.value);
  if (requestId == 0) {
    reject(QStringLiteral("request-id-exhausted"));
    return;
  }
  m_pending = PendingOperation{requestId, Intent::KeyboardBrightness,
                               debounce.rowId, debounce.owner, debounce.epoch,
                               debounce.revision, raw.value};
  m_operationStatusText = tr("Applying keyboard brightness…");
  Q_EMIT viewChanged();
}

void PowerSettingsModel::handleOperationCompleted(
    const quint64 requestId, const OperationResult &result) {
  if (!m_pending || m_pending->requestId != requestId) return;
  const PendingOperation pending = *m_pending;
  m_pending.reset();
  const bool exact = m_client.owner() == pending.owner && result.wireValid
      && Power::validateOperationResult(result).accepted
      && result.initiatingEpoch == pending.epoch
      && result.initiatingRevision == pending.revision
      && ((pending.intent == Intent::Profile
           && result.kind == OperationKind::SetProfile)
          || (pending.intent == Intent::KeyboardBrightness
              && result.kind == OperationKind::SetKeyboardBrightness));
  if (!exact || result.status == OperationStatus::Uncertain) {
    m_operationStatusText.clear();
    m_errorText = tr("The power change could not be confirmed. It was not replayed.");
  } else if (result.status != OperationStatus::Succeeded) {
    m_operationStatusText.clear();
    m_errorText = failureText(result.reasonCode);
  } else {
    m_convergence = Convergence{pending.intent, pending.target, pending.owner,
                                result.observedEpoch, result.observedRevision,
                                pending.expectedRaw};
    m_errorText.clear();
    m_operationStatusText = tr("Waiting for authoritative power state…");
    m_convergenceTimer.start();
  }
  synchronizeAuthority();
}

void PowerSettingsModel::synchronizeAuthority() {
  const auto state = m_client.state();
  if (m_retrying && hasDisplaySnapshot()
      && (state == PowerClientState::Ready
          || state == PowerClientState::Degraded)) {
    m_retrying = false;
    m_operationStatusText.clear();
  }
  if (m_debounce && (!hasDisplaySnapshot()
                     || m_client.owner() != m_debounce->owner
                     || m_client.snapshot().epoch != m_debounce->epoch)) {
    m_debounceTimer.stop();
    m_debounce.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Power authority changed before the queued brightness request; no change was sent.");
  }
  if (m_pending && (m_client.owner().isEmpty()
                    || m_client.owner() != m_pending->owner)) {
    m_pending.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Power authority changed during the operation. It was not replayed.");
  }
  if (m_convergence) {
    if (!hasDisplaySnapshot() || m_client.owner() != m_convergence->owner
        || m_client.snapshot().epoch != m_convergence->epoch) {
      m_convergence.reset();
      m_convergenceTimer.stop();
      m_operationStatusText.clear();
      m_errorText = tr("Power authority changed before the result converged. It was not replayed.");
    } else {
      const Snapshot snapshot = m_client.snapshot();
      bool converged = false;
      if (snapshot.revision >= m_convergence->minimumRevision) {
        if (m_convergence->intent == Intent::Profile) {
          converged = snapshot.profiles.activeProfileId == m_convergence->target;
        } else if (const Power::KeyboardBacklight *device =
                       findKeyboard(snapshot, m_convergence->target)) {
          converged = device->valueKnown
              && device->value == m_convergence->expectedRaw;
        }
      }
      if (converged) {
        m_convergence.reset();
        m_convergenceTimer.stop();
        m_operationStatusText.clear();
      }
    }
  }
  Q_EMIT viewChanged();
}

void PowerSettingsModel::reject(const QString &reason) {
  m_operationStatusText.clear();
  m_errorText = failureText(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString PowerSettingsModel::failureText(const QString &reason) const {
  if (reason == QStringLiteral("operation-busy")
      || reason == QStringLiteral("operation-busy-or-unavailable"))
    return tr("Another power change is still in progress.");
  if (reason == QStringLiteral("unsupported"))
    return tr("That power change is not admitted by the current service state.");
  if (reason == QStringLiteral("stale-handle")
      || reason == QStringLiteral("authority-changed"))
    return tr("Power state changed before the request could be sent.");
  if (reason == QStringLiteral("invalid-brightness"))
    return tr("That brightness value was not accepted.");
  if (reason == QStringLiteral("already-active"))
    return tr("That power profile is already active.");
  if (reason == QStringLiteral("unknown-profile"))
    return tr("That power profile is no longer available.");
  return reason.isEmpty() ? tr("The power request was rejected.")
                          : tr("The power request failed (%1).").arg(reason);
}

} // namespace QindaQt::Apps::SettingsPower
