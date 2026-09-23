// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QCoreApplication>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using Audio::Availability;
using Audio::Capability;
using Audio::Device;
using Audio::Snapshot;
using Audio::Stream;
using Audio::StreamDirection;

const Stream *findRouteStream(const Snapshot &snapshot, quint64 serial) {
  for (const Stream &stream : snapshot.streams) {
    if (stream.handle.serial == serial) return &stream;
  }
  return nullptr;
}

const Device *findRouteDevice(const Snapshot &snapshot, quint64 serial,
                              StreamDirection direction) {
  const auto &devices = direction == StreamDirection::Playback
                            ? snapshot.outputs : snapshot.inputs;
  for (const Device &device : devices) {
    if (device.handle.serial == serial) return &device;
  }
  return nullptr;
}

QString routeText(const char *text) {
  return QCoreApplication::translate("AudioSettings", text);
}

} // namespace

bool AudioSettingsModel::moveStream(quint64 streamSerial, quint64 deviceSerial) {
  // AGENT-CONTRACT: Audio Settings resolves BOTH serials from one retained
  // AudioClient snapshot; the client then repeats exact-handle preflight.
  // A QML index or old owner/epoch is never used as a graph handle.
  if (!snapshotAdmitsOperation(m_client, Capability::MoveStream)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (serialPending(streamSerial) || m_volumeBySerial.contains(streamSerial)
      || m_moveReadbacks.contains(streamSerial)) {
    rejectAction(QStringLiteral("operation-in-flight"));
    return false;
  }
  const Snapshot snapshot = m_client.snapshot();
  const Stream *stream = findRouteStream(snapshot, streamSerial);
  if (stream == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  if (!stream->canMove) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  const Device *device =
      findRouteDevice(snapshot, deviceSerial, stream->direction);
  if (device == nullptr) {
    const StreamDirection opposite =
        stream->direction == StreamDirection::Playback
            ? StreamDirection::Capture : StreamDirection::Playback;
    rejectAction(findRouteDevice(snapshot, deviceSerial, opposite) != nullptr
                     ? QStringLiteral("invalid-target")
                     : QStringLiteral("stale-handle"));
    return false;
  }
  if (stream->targetKnown && stream->target == device->handle) {
    rejectAction(QStringLiteral("invalid-target"));
    return false;
  }
  const quint64 requestId =
      m_client.moveStream(stream->handle, device->handle);
  if (requestId == 0) {
    rejectAction(QString());
    return false;
  }
  // A deliberate retry retires only this stream's earlier readback failure.
  m_moveFailures.remove(streamSerial);
  trackPending(requestId, streamSerial, Intent::MoveStream, deviceSerial);
  beginIntentMessage(Intent::MoveStream);
  return true;
}

void AudioSettingsModel::reconcileMoveReadbacks() {
  if (m_moveReadbacks.isEmpty()) return;
  if (m_client.owner().isEmpty() || !m_client.hasSnapshot()) {
    m_moveReadbacks.clear();
    m_operationStatusText.clear();
    m_localError = routeText(
        "The audio change could not be confirmed. Refreshing audio "
        "information before you try again.");
    return;
  }
  const Snapshot snapshot = m_client.snapshot();
  bool confirmedAny = false;
  bool resolvedAny = false;
  for (auto it = m_moveReadbacks.begin(); it != m_moveReadbacks.end();) {
    const MoveReadback expected = it.value();
    const quint64 streamSerial = it.key();
    if (m_client.owner() != expected.owner || snapshot.epoch != expected.epoch) {
      it = m_moveReadbacks.erase(it);
      m_operationStatusText.clear();
      m_localError = routeText(
          "The audio change could not be confirmed. Refreshing audio "
          "information before you try again.");
      continue;
    }
    if (m_moveSnapshotSequence < expected.requiredSnapshotSequence
        || snapshot.revision < expected.revisionFloor
        || (snapshot.availability != Availability::Ready
            && snapshot.availability != Availability::Degraded)) {
      ++it;
      continue;
    }
    const Stream *stream = findRouteStream(snapshot, streamSerial);
    const bool confirmed = stream != nullptr && stream->targetKnown
                           && stream->target.epoch == expected.epoch
                           && stream->target.serial == expected.targetSerial;
    it = m_moveReadbacks.erase(it);
    resolvedAny = true;
    if (confirmed) {
      m_moveFailures.remove(streamSerial);
      confirmedAny = true;
    } else {
      m_moveFailureOwner = expected.owner;
      m_moveFailures.insert(streamSerial,
          stream == nullptr
              ? routeText("That application stream is no longer available.")
              : routeText("The selected device did not take effect. Refresh the audio "
                          "device list before trying again."));
    }
  }
  // AGENT-GUARD: one accepted snapshot may resolve several stream moves.
  // Apply feedback after the entire batch, and keep earlier failed streams
  // visible when a different stream confirms in a later snapshot.
  if (resolvedAny) {
    if (!m_moveFailures.isEmpty()) {
      m_operationStatusText.clear();
    } else if (confirmedAny) {
      m_operationStatusText = routeText("Application device changed.");
    }
  }
}

} // namespace QindaQt::Apps::SettingsAudio
