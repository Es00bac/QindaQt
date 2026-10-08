// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_controller.h"

namespace QindaQt::Apps::FileManager {
namespace {
QString noticeFor(const MutationOutputObservation &output) {
  switch (output.disposition) {
  case MutationOutputDisposition::None:
    return {};
  case MutationOutputDisposition::RetainedPartial:
    return (output.exclusiveCreation
        ? QStringLiteral("Partial copy retained at last observation: %1")
        : QStringLiteral("Partial folder output observed; creation ownership is unconfirmed: %1"))
        .arg(output.path);
  case MutationOutputDisposition::RetainedCopy:
    return QStringLiteral("Copy output retained after the operation failed: %1").arg(output.path);
  case MutationOutputDisposition::Replaced:
    return QStringLiteral("Destination changed; its current entry was preserved. The copied output may be elsewhere: %1").arg(output.path);
  case MutationOutputDisposition::Unconfirmed:
    return QStringLiteral("Copy output location could not be confirmed. No cleanup was attempted: %1").arg(output.path);
  }
  return {};
}
QVariantMap recoveryMap(const MutationRecoveryReceipt &value) {
  return {{QStringLiteral("operationId"), value.operationId},
          {QStringLiteral("phase"), value.phase},
          {QStringLiteral("disposition"), static_cast<int>(value.disposition)},
          {QStringLiteral("sourcePath"), value.sourcePath},
          {QStringLiteral("destinationPath"), value.destinationPath},
          {QStringLiteral("stageDirectory"), value.stageDirectory},
          {QStringLiteral("recoveryDirectory"), value.recoveryDirectory},
          {QStringLiteral("retainedBytes"), QString::number(value.retainedBytesEstimate)},
          {QStringLiteral("uncertain"), value.uncertain},
          {QStringLiteral("canRestore"), value.restoreAvailable}};
}
QString recoveryNotice(const MutationRecoveryReceipt &value) {
  if (value.disposition == MutationRecoveryDisposition::None) return {};
  return QStringLiteral("Recovery operation %1; phase %2; retained byte estimate %3.\nOriginal: %4\nDestination: %5\nStage: %6\nRecovery: %7")
      .arg(value.operationId, value.phase, QString::number(value.retainedBytesEstimate),
           value.sourcePath, value.destinationPath, value.stageDirectory, value.recoveryDirectory);
}
QVariantMap trashMap(const MutationTrashReceipt &value) {
  return {{QStringLiteral("root"), value.root},
          {QStringLiteral("topDirectory"), value.topDirectory},
          {QStringLiteral("payloadPath"), value.payloadPath},
          {QStringLiteral("metadataPath"), value.metadataPath},
          {QStringLiteral("originalPath"), value.originalPath},
          {QStringLiteral("payloadConfirmed"), value.payloadConfirmed},
          {QStringLiteral("metadataRetained"), value.metadataRetained},
          {QStringLiteral("restoredConfirmed"), value.restoredConfirmed}};
}
QString trashNotice(const MutationTrashReceipt &value) {
  if (value.root.isEmpty()) return {};
  const auto state = value.restoredConfirmed
      ? QStringLiteral("Payload restored; metadata-only record retained.")
      : value.payloadConfirmed ? QStringLiteral("Payload stored in Trash.")
      : QStringLiteral("Trash metadata was observed/reserved and retained. Payload placement is unconfirmed; inspect before repair.");
  return state + QStringLiteral("\nOriginal: %1\nPayload: %2\nMetadata: %3")
      .arg(value.originalPath, value.payloadPath, value.metadataPath);
}
QVariantMap itemMap(const MutationItemOutcome &item) {
  return {{QStringLiteral("attempted"), item.attempted},
          {QStringLiteral("sourcePath"), item.sourcePath},
          {QStringLiteral("destinationPath"), item.destinationPath},
          {QStringLiteral("error"), mutationErrorKey(item.error)},
          {QStringLiteral("outputDisposition"), mutationOutputDispositionKey(item.output.disposition)},
          {QStringLiteral("outputPath"), item.output.path},
          {QStringLiteral("exclusiveCreation"), item.output.exclusiveCreation},
          {QStringLiteral("recovery"), recoveryMap(item.recovery)},
          {QStringLiteral("trash"), trashMap(item.trash)}};
}
} // namespace

QString MutationController::outputNotice() const { return m_outputNotice; }
QVariantList MutationController::outputObservations() const { return m_outputObservations; }

void MutationController::finish(const MutationResult &result) {
  m_outputNotice.clear();
  m_outputObservations.clear();
  int completed = 0;
  bool observedEffects = result.trashReceipt.metadataRetained || result.recovery.observedEffects || result.outputObservation.disposition != MutationOutputDisposition::None;
  auto remember = [&](const MutationRecoveryReceipt &value) {
    if (value.operationId.isEmpty()) return;
    for (auto &old : m_recoveryRecords) {
      if (old.toMap().value(QStringLiteral("operationId")).toString() == value.operationId) {
        old = recoveryMap(value); return;
      }
    }
    if (m_recoveryRecords.size() < 128) m_recoveryRecords.append(recoveryMap(value));
  };
  for (const auto &item : result.itemOutcomes) {
    remember(item.recovery);
    m_outputObservations.append(itemMap(item));
    if (item.attempted && item.error == MutationError::None)
      ++completed;
    observedEffects = observedEffects ||
        (item.attempted && (item.error == MutationError::None ||
         item.output.disposition != MutationOutputDisposition::None || item.recovery.observedEffects ||
         item.trash.metadataRetained));
  }
  if (m_runningKind == MutationKind::InspectRecovery || !result.recoveryReceipts.isEmpty()) {
    if (result.ok() || !result.recoveryReceipts.isEmpty()) m_recoveryRecords.clear();
    else for (auto &old : m_recoveryRecords) {
      auto map = old.toMap(); map.insert(QStringLiteral("uncertain"), true);
      map.insert(QStringLiteral("canRestore"), false); old = map;
    }
    for (const auto &value : result.recoveryReceipts) m_recoveryRecords.append(recoveryMap(value));
  } else remember(result.recovery);
  m_busy = false;
  m_progressValue = result.ok() ? 100 : 0;
  m_progressText.clear();
  m_cancellation.reset();
  if (!result.ok()) {
    m_failure = result.error;
    m_failureMessage = boundedMutationDiagnostic(result.diagnostic);
    m_resultText.clear();
    m_isUndo = false;
    m_outputNotice = result.trashReceipt.root.isEmpty()
        ? noticeFor(result.outputObservation) : trashNotice(result.trashReceipt);
    // Preserve receipts for the successful prefix, even if the last failed
    // item did not reserve metadata. They do not grant replay authority.
    for (const auto &item : result.itemOutcomes) {
      if (item.trash.root.isEmpty() || item.trash.metadataPath == result.trashReceipt.metadataPath) continue;
      const auto text = trashNotice(item.trash);
      m_outputNotice += (m_outputNotice.isEmpty() ? QString{} : QChar('\n')) + text;
    }
    const auto recoveryText = recoveryNotice(result.recovery);
    if (!recoveryText.isEmpty())
      m_outputNotice += (m_outputNotice.isEmpty() ? QString{} : QChar('\n')) + recoveryText;
    if (completed > 0) {
      const QString prefix = QStringLiteral("%1 earlier items completed; remaining items did not complete.")
                                 .arg(completed);
      m_outputNotice = m_outputNotice.isEmpty() ? prefix : prefix + QChar('\n') + m_outputNotice;
    }
    emit stateChanged();
    if (observedEffects)
      emit mutationCommitted();
    return;
  }
  m_outputNotice = trashNotice(result.trashReceipt);
  m_failure = MutationError::None;
  m_failureMessage.clear();
  m_resultText = m_isUndo ? QStringLiteral("Operation undone")
                 : result.diagnostic.isEmpty()
                     ? QStringLiteral("File operation completed")
                     : boundedMutationDiagnostic(result.diagnostic);
  if (m_runningKind != MutationKind::InspectRecovery)
    m_undoRequest = m_isUndo ? nullptr : result.undoRequest;
  m_isUndo = false;
  if (!result.trashToken.isEmpty() && result.outputIdentity) {
    m_lastTrashToken = result.trashToken;
    m_lastTrashOriginalPath = result.originalPath;
    m_lastTrashPayloadPath = result.outputPath;
    m_lastTrashIdentity = result.outputIdentity;
  } else if (m_runningKind == MutationKind::Restore ||
             m_runningKind == MutationKind::EmptyTrash) {
    m_lastTrashToken.clear();
    m_lastTrashOriginalPath.clear();
    m_lastTrashPayloadPath.clear();
    m_lastTrashIdentity.reset();
  }
  emit stateChanged();
  emit mutationCommitted();
}

} // namespace QindaQt::Apps::FileManager
