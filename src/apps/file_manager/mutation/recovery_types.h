// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"
#include <QByteArray>

namespace QindaQt::Apps::FileManager {

// Owning worker-thread values; serializable observations, never restart grants.
// Schema1 is private to File Manager. Unknown versions/fields refuse inspection
// as a valid record; callers retain the original bytes for manual inspection.
enum class RecoveryPhase {
  Prepared, Copying, Verified, Published, Retiring, Retained, Required, Restored
};

struct RecoveryMountObservation final {
  quint64 mountId = 0;
  quint64 device = 0;
  quint64 inode = 0;
  quint32 mode = 0;
  quint32 owner = 0;
  bool operator==(const RecoveryMountObservation &) const = default;
};

struct RecoveryRecord final {
  QString operationId;
  quint32 sequence = 0;
  RecoveryPhase phase = RecoveryPhase::Prepared;
  QString sourcePath;
  QString destinationPath;
  QString stageDirectory;
  QString recoveryDirectory;
  FileIdentity sourceIdentity;
  RecoveryMountObservation sourceParent;
  RecoveryMountObservation destinationParent;
  RecoveryMountObservation recoveryStorage;
  QByteArray manifestDigest;
  quint64 retainedBytesEstimate = 0;
  QString diagnostic;
  bool uncertain = false;
  bool operator==(const RecoveryRecord &) const = default;
};

inline constexpr qsizetype maximumRecoveryRecordBytes = 64 * 1024;
inline constexpr quint32 maximumRecoveryRecords = 16;
inline constexpr quint32 maximumRecoveryOperations = 128;

[[nodiscard]] QString recoveryPhaseKey(RecoveryPhase phase);
[[nodiscard]] std::optional<RecoveryPhase> recoveryPhaseFromKey(const QString &key);
[[nodiscard]] bool recoveryTransitionAllowed(RecoveryPhase from, RecoveryPhase to);
// Compares immutable admitted facts. Phase/sequence/diagnostic/uncertainty are
// observations that may advance; retainedBytesEstimate remains an estimate.
[[nodiscard]] bool sameRecoveryOperation(const RecoveryRecord &left,
                                          const RecoveryRecord &right);

} // namespace QindaQt::Apps::FileManager
