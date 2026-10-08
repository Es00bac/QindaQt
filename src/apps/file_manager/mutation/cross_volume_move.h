// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_catalog.h"
#include "recovery_manifest.h"
namespace QindaQt::Apps::FileManager {
enum class CrossVolumeStep {
  Prepared, Copy, Verified, BeforePublish, AfterPublish, SyncDestination,
  BeforeRetirement, AfterRetirement, SyncRecovery, SyncSource, FinalVerification,
  BeforeRestore, AfterRestore, PublicationWindow, RetirementWindow, RestoreWindow,
  BeforeUnexpectedReturn, UnexpectedReturnWindow, AfterUnexpectedReturn
};
// Owning test hook may mutate fixtures or subtractively fail a real step.
// It never supplies device IDs, verification or successful syscall results.
// Same synchronous worker thread; captures must survive the complete call.
using CrossVolumeFault = std::function<MutationError(CrossVolumeStep)>;
class CrossVolumeMove final {
public:
  explicit CrossVolumeMove(QString catalogPath, CrossVolumeFault fault = {},
                           RecoveryStoreFault storeFault = {});
  [[nodiscard]] MutationResult move(const MutationRequest &request,
      const MutationCancellation &cancellation, const MutationProgressCallback &progress);
  [[nodiscard]] MutationResult inspect(const QString &operationId,
      const MutationCancellation &cancellation = {});
  [[nodiscard]] MutationResult restore(const QString &operationId,
      const MutationCancellation &cancellation = {});
private:
  [[nodiscard]] MutationResult step(CrossVolumeStep phase) const;
  QString m_catalogPath;
  CrossVolumeFault m_fault;
  RecoveryStoreFault m_storeFault;
};
} // namespace QindaQt::Apps::FileManager
