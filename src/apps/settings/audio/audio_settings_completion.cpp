// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <utility>

namespace QindaQt::Apps::SettingsAudio {
using Audio::OperationResult;
using Audio::OperationStatus;

namespace {
QString translateAudio(const char *text) {
  return QCoreApplication::translate("AudioSettings", text);
}
} // namespace

void AudioSettingsModel::handleOperationCompleted(
    const quint64 requestId, const OperationResult &result) {
  // AGENT-GUARD: Only a tracked request's completion is presentation truth.
  // Late, foreign, or superseded request IDs are dropped; reacting to them
  // would let a retired lineage surface as fresh feedback. Lookup is by
  // requestId -> serial, then serial removes its own pending entry only, so
  // one target completing never touches another target's in-flight state.
  const auto serialIt = m_serialByRequestId.constFind(requestId);
  const bool consoleRequest = m_consoleRequestIds.remove(requestId);
  if (serialIt == m_serialByRequestId.constEnd() && !consoleRequest) {
    return;
  }
  std::optional<std::pair<quint64, PendingIntent>> completed;
  if (serialIt != m_serialByRequestId.constEnd()) {
    const quint64 serial = *serialIt;
    const auto pending = m_pendingBySerial.constFind(serial);
    if (pending != m_pendingBySerial.constEnd() && pending->requestId == requestId) {
      completed = std::pair{serial, *pending};
      m_pendingBySerial.remove(serial);
    }
    m_serialByRequestId.remove(requestId);
  }

  if (result.status == OperationStatus::Succeeded) {
    m_localError.clear();
    if (completed.has_value() && completed->second.intent == Intent::MoveStream) {
      const PendingIntent &pending = completed->second;
      if (m_client.owner() == pending.owner && m_client.hasSnapshot()
          && m_client.snapshot().epoch == pending.epoch
          && result.observedEpoch == pending.epoch) {
        const quint64 generation = m_nextMoveGeneration++;
        m_moveReadbacks.insert(completed->first,
            MoveReadback{pending.targetSerial, pending.owner, pending.epoch,
                         result.observedRevision,
                         pending.snapshotSequenceAtDispatch + 1, generation});
        m_operationStatusText = translateAudio(
            "Checking the application's selected device…");
        QTimer::singleShot(5'000, this, [this, serial = completed->first,
                                        generation] {
          const auto it = m_moveReadbacks.constFind(serial);
          if (it == m_moveReadbacks.constEnd() || it->generation != generation) {
            return;
          }
          m_moveReadbacks.remove(serial);
          m_operationStatusText.clear();
          m_localError = translateAudio(
              "The selected device could not be confirmed. Refresh the audio "
              "device list before trying again.");
          Q_EMIT viewChanged();
        });
        // AudioClient queues operationCompleted yet starts its readback
        // fetch synchronously. A fast accepted snapshot may already have
        // arrived before this callback; the dispatch sequence still admits
        // that proof without waiting for a nonexistent extra publication.
        reconcileMoveReadbacks();
      } else {
        m_operationStatusText.clear();
        m_localError = translateAudio(
            "The audio change could not be confirmed. Refreshing audio "
            "information before you try again.");
      }
    } else {
      m_operationStatusText = translateAudio(
          "Change applied; refreshing audio information.");
    }
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
  if (completed.has_value()) {
    completeVolumeRequest(completed->first, completed->second.intent, result);
  }
}

} // namespace QindaQt::Apps::SettingsAudio
