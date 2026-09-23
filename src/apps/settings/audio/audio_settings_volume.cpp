// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QTimer>

#include <cmath>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using Audio::AudioClient;
using Audio::Availability;
using Audio::Capability;
using Audio::ClientState;
using Audio::Device;
using Audio::OperationStatus;
using Audio::Snapshot;
using Audio::Stream;

bool volumeTargetAvailable(const Snapshot &snapshot, const quint64 serial,
                           const bool isStream) {
  if (isStream) {
    for (const Stream &stream : snapshot.streams) {
      if (stream.handle.serial == serial) return stream.canSetVolume;
    }
    return false;
  }
  for (const Device &device : snapshot.outputs) {
    if (device.handle.serial == serial) return device.canSetVolume;
  }
  for (const Device &device : snapshot.inputs) {
    if (device.handle.serial == serial) return device.canSetVolume;
  }
  return false;
}

} // namespace

bool AudioSettingsModel::setDeviceVolume(const quint64 serial,
                                         const double level) {
  return requestVolume(serial, false, level);
}

bool AudioSettingsModel::setStreamVolume(const quint64 serial,
                                         const double level) {
  return requestVolume(serial, true, level);
}

bool AudioSettingsModel::setDeviceMuted(const quint64 serial,
                                        const bool muted) {
  return dispatchDeviceIntent(serial, Intent::DeviceMute, 0.0, muted);
}

bool AudioSettingsModel::setStreamMuted(const quint64 serial,
                                        const bool muted) {
  return dispatchStreamIntent(serial, Intent::StreamMute, 0.0, muted);
}

bool AudioSettingsModel::requestVolume(const quint64 serial,
                                       const bool isStream,
                                       const double level) {
  if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
    rejectAction(QStringLiteral("invalid-volume"));
    return false;
  }
  if (!snapshotAdmitsOperation(m_client, Capability::SetVolume)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  if (!volumeTargetAvailable(snapshot, serial, isStream)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }

  const Intent intent = isStream ? Intent::StreamVolume : Intent::DeviceVolume;
  const auto pending = m_pendingBySerial.constFind(serial);
  if (pending != m_pendingBySerial.constEnd()) {
    if (pending->intent != intent) {
      rejectAction(QStringLiteral("operation-in-flight"));
      return false;
    }
  }
  auto outstanding = m_volumeBySerial.find(serial);
  if (pending != m_pendingBySerial.constEnd()
      || (outstanding != m_volumeBySerial.end()
          && outstanding->awaitingSnapshot)) {
    if (outstanding == m_volumeBySerial.end()
        || outstanding->owner != m_client.owner()
        || outstanding->epoch != snapshot.epoch
        || outstanding->isStream != isStream) {
      rejectAction(QStringLiteral("operation-in-flight"));
      return false;
    }
    // AGENT-GUARD: keep only the most recent target. The first write must
    // complete and be confirmed by a newer snapshot before this value leaves
    // the process; a rejected or uncertain first write cannot cause replay.
    outstanding->displayLevel = level;
    outstanding->queuedLevel = level;
    Q_EMIT viewChanged();
    return true;
  }

  const bool dispatched = isStream
      ? dispatchStreamIntent(serial, intent, level, false)
      : dispatchDeviceIntent(serial, intent, level, false);
  if (!dispatched) {
    m_volumeBySerial.remove(serial);
    return false;
  }
  m_volumeBySerial.insert(serial,
      VolumeIntent{.displayLevel = level,
                   .queuedLevel = std::nullopt,
                   .owner = m_client.owner(),
                   .epoch = snapshot.epoch,
                   .requestRevision = snapshot.revision,
                   .generation = m_nextVolumeGeneration++,
                   .isStream = isStream,
                   .awaitingSnapshot = false});
  Q_EMIT viewChanged();
  return true;
}

void AudioSettingsModel::completeVolumeRequest(
    const quint64 serial, const Intent intent,
    const Audio::OperationResult &result) {
  if (intent != Intent::DeviceVolume && intent != Intent::StreamVolume) return;
  auto outstanding = m_volumeBySerial.find(serial);
  if (outstanding == m_volumeBySerial.end()) return;
  if (result.status != OperationStatus::Succeeded) {
    m_volumeBySerial.erase(outstanding);
    Q_EMIT viewChanged();
    return;
  }
  outstanding->awaitingSnapshot = true;
  outstanding->generation = m_nextVolumeGeneration++;
  const quint64 generation = outstanding->generation;
  // A successful reply is not a snapshot. A missing/duplicate readback must
  // not leave the handle showing unverified intent indefinitely.
  QTimer::singleShot(2'500, this, [this, serial, generation] {
    expireVolumeIntent(serial, generation);
  });
  reconcileVolumeIntents();
  Q_EMIT viewChanged();
}

void AudioSettingsModel::expireVolumeIntent(const quint64 serial,
                                            const quint64 generation) {
  const auto outstanding = m_volumeBySerial.constFind(serial);
  if (outstanding == m_volumeBySerial.constEnd()
      || outstanding->generation != generation
      || !outstanding->awaitingSnapshot || serialPending(serial)) return;
  m_volumeBySerial.remove(serial);
  Q_EMIT viewChanged();
}

void AudioSettingsModel::reconcileVolumeIntents() {
  if (m_volumeBySerial.isEmpty()) return;
  const QString owner = m_client.owner();
  const bool hasSnapshot = m_client.hasSnapshot();
  const Snapshot snapshot = hasSnapshot ? m_client.snapshot() : Snapshot{};
  const bool available = hasSnapshot && !owner.isEmpty()
      && m_client.state() != ClientState::Stopped
      && m_client.state() != ClientState::Unavailable
      && (snapshot.availability == Availability::Ready
          || snapshot.availability == Availability::Degraded)
      && snapshot.capabilities.testFlag(Capability::SetVolume);

  // Copy keys because dispatch emits viewChanged and can add a new pending
  // request. Every queued successor is checked against its exact authority.
  const QList<quint64> serials = m_volumeBySerial.keys();
  for (const quint64 serial : serials) {
    auto outstanding = m_volumeBySerial.find(serial);
    if (outstanding == m_volumeBySerial.end()) continue;
    if (!available || outstanding->owner != owner
        || outstanding->epoch != snapshot.epoch
        || !volumeTargetAvailable(snapshot, serial, outstanding->isStream)) {
      m_volumeBySerial.erase(outstanding);
      continue;
    }
    if (!outstanding->awaitingSnapshot || serialPending(serial)
        || snapshot.revision <= outstanding->requestRevision) continue;
    if (!outstanding->queuedLevel) {
      m_volumeBySerial.erase(outstanding);
      continue;
    }
    const double latest = *outstanding->queuedLevel;
    const bool isStream = outstanding->isStream;
    outstanding->queuedLevel.reset();
    outstanding->awaitingSnapshot = false;
    outstanding->requestRevision = snapshot.revision;
    const bool dispatched = isStream
        ? dispatchStreamIntent(serial, Intent::StreamVolume, latest, false)
        : dispatchDeviceIntent(serial, Intent::DeviceVolume, latest, false);
    if (!dispatched) m_volumeBySerial.remove(serial);
  }
}

std::optional<double> AudioSettingsModel::displayVolumeLevel(
    const quint64 serial, const bool isStream) const {
  const auto outstanding = m_volumeBySerial.constFind(serial);
  if (outstanding == m_volumeBySerial.constEnd() || !m_client.hasSnapshot()
      || outstanding->isStream != isStream
      || outstanding->owner != m_client.owner()
      || outstanding->epoch != m_client.snapshot().epoch) return std::nullopt;
  return outstanding->displayLevel;
}

} // namespace QindaQt::Apps::SettingsAudio
