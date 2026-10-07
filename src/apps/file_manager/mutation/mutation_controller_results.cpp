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
QVariantMap itemMap(const MutationItemOutcome &item) {
  return {{QStringLiteral("attempted"), item.attempted},
          {QStringLiteral("sourcePath"), item.sourcePath},
          {QStringLiteral("destinationPath"), item.destinationPath},
          {QStringLiteral("error"), mutationErrorKey(item.error)},
          {QStringLiteral("outputDisposition"), mutationOutputDispositionKey(item.output.disposition)},
          {QStringLiteral("outputPath"), item.output.path},
          {QStringLiteral("exclusiveCreation"), item.output.exclusiveCreation}};
}
} // namespace

QString MutationController::outputNotice() const { return m_outputNotice; }
QVariantList MutationController::outputObservations() const { return m_outputObservations; }

void MutationController::finish(const MutationResult &result) {
  m_outputNotice.clear();
  m_outputObservations.clear();
  int completed = 0;
  bool observedEffects = result.outputObservation.disposition != MutationOutputDisposition::None;
  for (const auto &item : result.itemOutcomes) {
    m_outputObservations.append(itemMap(item));
    if (item.attempted && item.error == MutationError::None)
      ++completed;
    observedEffects = observedEffects ||
        (item.attempted && (item.error == MutationError::None ||
         item.output.disposition != MutationOutputDisposition::None));
  }
  m_busy = false;
  m_progressValue = result.ok() ? 100 : 0;
  m_progressText.clear();
  m_cancellation.reset();
  if (!result.ok()) {
    m_failure = result.error;
    m_failureMessage = boundedMutationDiagnostic(result.diagnostic);
    m_resultText.clear();
    m_isUndo = false;
    m_outputNotice = noticeFor(result.outputObservation);
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
  m_failure = MutationError::None;
  m_failureMessage.clear();
  m_resultText = m_isUndo ? QStringLiteral("Operation undone")
                 : result.diagnostic.isEmpty()
                     ? QStringLiteral("File operation completed")
                     : boundedMutationDiagnostic(result.diagnostic);
  m_undoRequest = m_isUndo ? nullptr : result.undoRequest;
  m_isUndo = false;
  if (!result.trashToken.isEmpty() && result.outputIdentity) {
    m_lastTrashToken = result.trashToken;
    m_lastTrashOriginalPath = result.originalPath;
    m_lastTrashIdentity = result.outputIdentity;
  } else if (m_runningKind == MutationKind::Restore ||
             m_runningKind == MutationKind::EmptyTrash) {
    m_lastTrashToken.clear();
    m_lastTrashOriginalPath.clear();
    m_lastTrashIdentity.reset();
  }
  emit stateChanged();
  emit mutationCommitted();
}

} // namespace QindaQt::Apps::FileManager
