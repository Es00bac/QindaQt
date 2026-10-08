// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_types.h"

namespace QindaQt::Apps::FileManager {

QString recoveryPhaseKey(RecoveryPhase phase) {
  switch (phase) {
  case RecoveryPhase::Prepared: return QStringLiteral("prepared");
  case RecoveryPhase::Copying: return QStringLiteral("copying");
  case RecoveryPhase::Verified: return QStringLiteral("verified");
  case RecoveryPhase::Published: return QStringLiteral("published");
  case RecoveryPhase::Retiring: return QStringLiteral("retiring");
  case RecoveryPhase::Retained: return QStringLiteral("retained");
  case RecoveryPhase::Required: return QStringLiteral("recovery-required");
  case RecoveryPhase::Restored: return QStringLiteral("restored");
  }
  return {};
}

std::optional<RecoveryPhase> recoveryPhaseFromKey(const QString &key) {
  for (const auto phase : {RecoveryPhase::Prepared, RecoveryPhase::Copying,
                           RecoveryPhase::Verified, RecoveryPhase::Published,
                           RecoveryPhase::Retiring, RecoveryPhase::Retained,
                           RecoveryPhase::Required, RecoveryPhase::Restored}) {
    if (key == recoveryPhaseKey(phase)) return phase;
  }
  return std::nullopt;
}

bool recoveryTransitionAllowed(RecoveryPhase from, RecoveryPhase to) {
  if (to == RecoveryPhase::Required)
    return from != RecoveryPhase::Restored && from != RecoveryPhase::Required;
  switch (from) {
  case RecoveryPhase::Prepared: return to == RecoveryPhase::Copying;
  case RecoveryPhase::Copying: return to == RecoveryPhase::Verified;
  case RecoveryPhase::Verified: return to == RecoveryPhase::Published;
  case RecoveryPhase::Published: return to == RecoveryPhase::Retiring;
  case RecoveryPhase::Retiring: return to == RecoveryPhase::Retained;
  case RecoveryPhase::Retained:
  case RecoveryPhase::Required: return to == RecoveryPhase::Restored;
  case RecoveryPhase::Restored: return false;
  }
  return false;
}

bool sameRecoveryOperation(const RecoveryRecord &left,
                            const RecoveryRecord &right) {
  return left.operationId == right.operationId &&
      left.sourcePath == right.sourcePath &&
      left.destinationPath == right.destinationPath &&
      left.stageDirectory == right.stageDirectory &&
      left.recoveryDirectory == right.recoveryDirectory &&
      left.sourceIdentity == right.sourceIdentity &&
      left.sourceParent == right.sourceParent &&
      left.destinationParent == right.destinationParent &&
      left.recoveryStorage == right.recoveryStorage &&
      left.manifestDigest == right.manifestDigest &&
      left.retainedBytesEstimate == right.retainedBytesEstimate;
}

} // namespace QindaQt::Apps::FileManager
