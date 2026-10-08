// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_controller.h"
namespace QindaQt::Apps::FileManager {
QVariantList MutationController::recoveryRecords() const { return m_recoveryRecords; }
bool MutationController::inspectRecovery() {
  MutationRequest request; request.kind = MutationKind::InspectRecovery;
  return submit(std::move(request));
}
bool MutationController::restoreRecovery(const QString &operationId) {
  if (operationId.isEmpty()) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose a recovery operation first."));
    return false;
  }
  MutationRequest request; request.kind = MutationKind::RestoreRecovery;
  request.recoveryOperationId = operationId;
  return submit(std::move(request));
}
} // namespace QindaQt::Apps::FileManager
