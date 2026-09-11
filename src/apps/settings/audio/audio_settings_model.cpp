// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QCoreApplication>

#include <cmath>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using QindaQt::Audio::AudioClient;
using QindaQt::Audio::Availability;
using QindaQt::Audio::Capability;
using QindaQt::Audio::ClientState;
using QindaQt::Audio::Device;
using QindaQt::Audio::DeviceKind;
using QindaQt::Audio::OperationResult;
using QindaQt::Audio::OperationStatus;
using QindaQt::Audio::Snapshot;
using QindaQt::Audio::Stream;

QString translateAudio(const char *text) {
  return QCoreApplication::translate("AudioSettings", text);
}

std::optional<double> clampedLevel(const double level) {
  if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
    return std::nullopt;
  }
  return level;
}

const Device *findDevice(const Snapshot &snapshot, const quint64 serial,
                         const DeviceKind kind) {
  const QList<Device> &devices =
      kind == DeviceKind::Output ? snapshot.outputs : snapshot.inputs;
  for (const Device &device : devices) {
    if (device.handle.serial == serial) {
      return &device;
    }
  }
  return nullptr;
}

const Stream *findStream(const Snapshot &snapshot, const quint64 serial) {
  for (const Stream &stream : snapshot.streams) {
    if (stream.handle.serial == serial) {
      return &stream;
    }
  }
  return nullptr;
}

} // namespace

// AGENT-GUARD: This predicate mirrors the public AudioClient's dispatch
// preflight exactly (retained snapshot presence, snapshot availability,
// capability bit) plus the client's serialized single-operation fence. The
// per-target can-set flags are combined by the callers. Widening it here
// silently enables controls the client refuses; narrowing it hides controls
// the client admits, breaking the availability/admission equality contract.
bool AudioSettingsModel::snapshotAdmitsOperation(
    const AudioClient &client, const Capability capability) {
  if (client.operationPending() || !client.hasSnapshot()
      || client.owner().isEmpty()) {
    return false;
  }
  const Snapshot snapshot = client.snapshot();
  return (snapshot.availability == Availability::Ready
          || snapshot.availability == Availability::Degraded)
         && snapshot.capabilities.testFlag(capability);
}

AudioSettingsModel::AudioSettingsModel(AudioClient &client, QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client, &AudioClient::stateChanged, this,
          &AudioSettingsModel::viewChanged);
  connect(&m_client, &AudioClient::snapshotChanged, this,
          &AudioSettingsModel::viewChanged);
  connect(&m_client, &AudioClient::operationCompleted, this,
          &AudioSettingsModel::handleOperationCompleted);
}

bool AudioSettingsModel::loading() const noexcept {
  const ClientState state = m_client.state();
  return state == ClientState::Stopped || state == ClientState::Starting;
}

bool AudioSettingsModel::ready() const noexcept {
  return m_client.state() == ClientState::Ready;
}

bool AudioSettingsModel::degraded() const noexcept {
  return m_client.state() == ClientState::Degraded;
}

bool AudioSettingsModel::unavailable() const noexcept {
  return !loading() && !ready() && !degraded() && !stale();
}

bool AudioSettingsModel::stale() const {
  // A retained snapshot while the client recovers is deliberately presented
  // as stale, matching the client's own publication truth.
  return m_client.state() == ClientState::Unavailable && m_client.hasSnapshot();
}

bool AudioSettingsModel::busy() const noexcept {
  return m_client.operationPending();
}

bool AudioSettingsModel::reloadAvailable() const noexcept { return !busy(); }

QString AudioSettingsModel::serviceOwner() const {
  return m_client.owner();
}

qulonglong AudioSettingsModel::serviceEpoch() const {
  return m_client.hasSnapshot() ? m_client.snapshot().epoch : 0;
}

qulonglong AudioSettingsModel::serviceRevision() const {
  return m_client.hasSnapshot() ? m_client.snapshot().revision : 0;
}

bool AudioSettingsModel::reload() {
  if (busy()) {
    rejectAction(QStringLiteral("operation-in-flight"));
    return false;
  }
  // AGENT-NOTE: The public client exposes no on-demand refetch; discovery and
  // invalidation are automatic. A user-visible retry is therefore one bounded
  // stop/start rediscovery, which resets lineage state exactly like the
  // client's own owner-replacement path and never replays a mutation.
  m_client.stop();
  m_client.start();
  m_localError.clear();
  m_operationStatusText = translateAudio("Reconnecting to the audio service…");
  Q_EMIT viewChanged();
  return true;
}

bool AudioSettingsModel::setDefaultDevice(const quint64 serial) {
  if (!snapshotAdmitsOperation(m_client, Capability::SetDefault)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *output =
      findDevice(snapshot, serial, DeviceKind::Output);
  const Device *input = findDevice(snapshot, serial, DeviceKind::Input);
  if (output == nullptr && input == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  const quint64 requestId =
      m_client.setDefault((output != nullptr ? output : input)->handle);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, serial, Intent::SetDefault);
  beginIntentMessage(Intent::SetDefault);
  return true;
}

bool AudioSettingsModel::dispatchDeviceIntent(const quint64 serial,
                                               const Intent intent,
                                               const double level,
                                               const bool muted) {
  const Capability capability = intent == Intent::DeviceVolume
                                    ? Capability::SetVolume
                                    : Capability::SetMute;
  if (!snapshotAdmitsOperation(m_client, capability)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (intent == Intent::DeviceVolume) {
    const std::optional<double> clamped = clampedLevel(level);
    if (!clamped.has_value()) {
      rejectAction(QStringLiteral("invalid-volume"));
      return false;
    }
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *output =
      findDevice(snapshot, serial, DeviceKind::Output);
  const Device *input = findDevice(snapshot, serial, DeviceKind::Input);
  const Device *device = output != nullptr ? output : input;
  if (device == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  if (intent == Intent::DeviceVolume && !device->canSetVolume) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (intent == Intent::DeviceMute && !device->canSetMute) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const quint64 requestId = intent == Intent::DeviceVolume
                                ? m_client.setVolume(device->handle, level)
                                : m_client.setMute(device->handle, muted);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, serial, intent);
  beginIntentMessage(intent);
  return true;
}

bool AudioSettingsModel::dispatchStreamIntent(const quint64 serial,
                                               const Intent intent,
                                               const double level,
                                               const bool muted) {
  const Capability capability = intent == Intent::StreamVolume
                                    ? Capability::SetVolume
                                    : Capability::SetMute;
  if (!snapshotAdmitsOperation(m_client, capability)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (intent == Intent::StreamVolume) {
    const std::optional<double> clamped = clampedLevel(level);
    if (!clamped.has_value()) {
      rejectAction(QStringLiteral("invalid-volume"));
      return false;
    }
  }
  const Snapshot snapshot = m_client.snapshot();
  const Stream *stream = findStream(snapshot, serial);
  if (stream == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  if (intent == Intent::StreamVolume && !stream->canSetVolume) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (intent == Intent::StreamMute && !stream->canSetMute) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const quint64 requestId = intent == Intent::StreamVolume
                                ? m_client.setVolume(stream->handle, level)
                                : m_client.setMute(stream->handle, muted);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, serial, intent);
  beginIntentMessage(intent);
  return true;
}

bool AudioSettingsModel::setDeviceVolume(const quint64 serial,
                                         const double level) {
  return dispatchDeviceIntent(serial, Intent::DeviceVolume, level, false);
}

bool AudioSettingsModel::setDeviceMuted(const quint64 serial,
                                        const bool muted) {
  return dispatchDeviceIntent(serial, Intent::DeviceMute, 0.0, muted);
}

bool AudioSettingsModel::setStreamVolume(const quint64 serial,
                                         const double level) {
  return dispatchStreamIntent(serial, Intent::StreamVolume, level, false);
}

bool AudioSettingsModel::setStreamMuted(const quint64 serial,
                                        const bool muted) {
  return dispatchStreamIntent(serial, Intent::StreamMute, 0.0, muted);
}

bool AudioSettingsModel::setDeviceChannelVolume(const quint64 serial,
                                                const int channelIndex,
                                                const double level) {
  if (!snapshotAdmitsOperation(m_client, Capability::SetChannelVolumes)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const std::optional<double> clamped = clampedLevel(level);
  if (!clamped.has_value()) {
    rejectAction(QStringLiteral("invalid-volume"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *output =
      findDevice(snapshot, serial, DeviceKind::Output);
  const Device *input = findDevice(snapshot, serial, DeviceKind::Input);
  const Device *device = output != nullptr ? output : input;
  if (device == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  // The client's preflight requires the full retained layout, so the route
  // rebuilds the vector from the same snapshot and replaces exactly one
  // channel. A single-channel device has no per-channel surface, and an
  // out-of-range index can never be dispatched.
  if (!device->canSetVolume || device->channelVolumes.size() < 2
      || channelIndex < 0
      || channelIndex >= device->channelVolumes.size()) {
    rejectAction(QStringLiteral("invalid-target"));
    return false;
  }
  QVector<double> volumes = device->channelVolumes;
  volumes[channelIndex] = *clamped;
  const quint64 requestId = m_client.setChannelVolumes(device->handle, volumes);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, serial, Intent::DeviceChannelVolume);
  beginIntentMessage(Intent::DeviceChannelVolume);
  return true;
}

bool AudioSettingsModel::createVirtualDevice(QString kindToken,
                                             QString displayName,
                                             const int channels) {
  if (!snapshotAdmitsOperation(m_client, Capability::ManageVirtualDevices)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const QString token = kindToken.trimmed();
  const QString name = displayName.trimmed();
  const bool outputKind =
      token.compare(QStringLiteral("output"), Qt::CaseInsensitive) == 0;
  const bool inputKind =
      token.compare(QStringLiteral("input"), Qt::CaseInsensitive) == 0;
  if (!outputKind && !inputKind) {
    rejectAction(QStringLiteral("invalid-target"));
    return false;
  }
  const DeviceKind kind = outputKind ? DeviceKind::Output : DeviceKind::Input;
  if (name.isEmpty()) {
    rejectAction(QStringLiteral("invalid-name"));
    return false;
  }
  if (channels != 2 && channels != 4 && channels != 6 && channels != 8) {
    rejectAction(QStringLiteral("invalid-channel-count"));
    return false;
  }
  const quint64 requestId = m_client.createVirtualDevice(
      kind, name, static_cast<quint32>(channels));
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, 0, Intent::CreateVirtual);
  beginIntentMessage(Intent::CreateVirtual);
  return true;
}

bool AudioSettingsModel::removeVirtualDevice(const quint64 serial) {
  if (!snapshotAdmitsOperation(m_client, Capability::ManageVirtualDevices)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *output =
      findDevice(snapshot, serial, DeviceKind::Output);
  const Device *input = findDevice(snapshot, serial, DeviceKind::Input);
  const Device *device = output != nullptr ? output : input;
  if (device == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  // AGENT-GUARD: the route never dispatches removal of a device the snapshot
  // does not flag as service-managed; the service and backend enforce the
  // same fence, so hardware can never be destroyed from Settings.
  if (!device->virtualDevice) {
    rejectAction(QStringLiteral("invalid-target"));
    return false;
  }
  const quint64 requestId = m_client.removeVirtualDevice(device->handle);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  trackPending(requestId, serial, Intent::RemoveVirtual);
  beginIntentMessage(Intent::RemoveVirtual);
  return true;
}

void AudioSettingsModel::trackPending(const quint64 requestId,
                                      const quint64 serial,
                                      const Intent intent) {
  m_pending = PendingIntent{requestId, serial, intent};
}

void AudioSettingsModel::beginIntentMessage(const Intent intent) {
  m_localError.clear();
  switch (intent) {
  case Intent::SetDefault:
    m_operationStatusText = translateAudio("Selecting the default device…");
    break;
  case Intent::DeviceVolume:
    m_operationStatusText = translateAudio("Applying the device volume…");
    break;
  case Intent::DeviceMute:
    m_operationStatusText = translateAudio("Applying the device mute state…");
    break;
  case Intent::StreamVolume:
    m_operationStatusText = translateAudio("Applying the application volume…");
    break;
  case Intent::StreamMute:
    m_operationStatusText = translateAudio("Applying the application mute state…");
    break;
  case Intent::DeviceChannelVolume:
    m_operationStatusText = translateAudio("Applying the channel volume…");
    break;
  case Intent::CreateVirtual:
    m_operationStatusText = translateAudio("Creating the virtual device…");
    break;
  case Intent::RemoveVirtual:
    m_operationStatusText = translateAudio("Removing the virtual device…");
    break;
  }
  Q_EMIT viewChanged();
}

void AudioSettingsModel::handleOperationCompleted(
    const quint64 requestId, const OperationResult &result) {
  // AGENT-GUARD: Only the tracked request's completion is presentation
  // truth. Late, foreign, or superseded request IDs are dropped; reacting to
  // them would let a retired lineage surface as fresh feedback.
  if (!m_pending.has_value() || m_pending->requestId != requestId) {
    return;
  }
  m_pending.reset();

  if (result.status == OperationStatus::Succeeded) {
    m_localError.clear();
    m_operationStatusText = translateAudio(
        "Change applied; refreshing audio information.");
  } else {
    m_operationStatusText.clear();
    if (result.status == OperationStatus::Uncertain) {
      m_localError = translateAudio(
          "The audio change could not be confirmed. Refreshing audio "
          "information before you try again.");
    } else {
      m_localError = actionFailureText(result.reasonCode);
    }
  }
  Q_EMIT viewChanged();
}

void AudioSettingsModel::rejectAction(const QString &reason) {
  m_operationStatusText.clear();
  m_localError = actionFailureText(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString AudioSettingsModel::actionFailureText(const QString &reason) const {
  if (reason == QStringLiteral("unsupported")) {
    return translateAudio("That change is not permitted by the audio service right now.");
  }
  if (reason == QStringLiteral("stale-handle")) {
    return translateAudio("That audio item changed; the list is being refreshed.");
  }
  if (reason == QStringLiteral("invalid-volume")) {
    return translateAudio("That volume value was not accepted.");
  }
  if (reason == QStringLiteral("invalid-target")) {
    return translateAudio(
        "That audio item cannot be changed that way.");
  }
  if (reason == QStringLiteral("invalid-name")) {
    return translateAudio("That virtual device name was not accepted.");
  }
  if (reason == QStringLiteral("invalid-channel-count")) {
    return translateAudio("That channel layout is not supported.");
  }
  if (reason == QStringLiteral("operation-in-flight")) {
    return translateAudio("Another audio change is still in progress.");
  }
  if (reason == QStringLiteral("unavailable")) {
    return translateAudio("The audio service is not available right now.");
  }
  if (reason.isEmpty()) {
    return translateAudio("The audio request was rejected.");
  }
  return translateAudio("The audio change failed. Refresh the device list and try again.");
}

} // namespace QindaQt::Apps::SettingsAudio
