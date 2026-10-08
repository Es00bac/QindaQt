// SPDX-License-Identifier: GPL-3.0-or-later
#include "cross_volume_move_p.h"
namespace QindaQt::Apps::FileManager {
using namespace RecoveryMovePrivate;
namespace {
RecoveryRecordRead currentRecord(const RecoveryRecord &locator,
    std::optional<RecoveryDirectoryAdmission> &storage) {
  MutationResult result;
  storage = RecoveryDirectoryAdmission::open(locator.recoveryDirectory, result);
  if (!storage) return {result, {}};
  result = storage->privateCurrent();
  if (!result.ok()) return {result, {}};
  if (storage->observation() != locator.recoveryStorage)
    return {failure(MutationError::Changed, QStringLiteral("Recovery storage was replaced or its mount changed; inspect without restoring.")), {}};
  RecoveryRecordStore store(*storage);
  auto read = store.inspect();
  if (read.record && !sameRecoveryOperation(locator, *read.record))
    return {failure(MutationError::Changed, QStringLiteral("Recovery record no longer matches its locator.")), {}};
  return read;
}
MutationRecoveryReceipt inspectPlacement(const RecoveryRecord &record,
    const std::optional<RecoveryDirectoryAdmission> &storage) {
  auto observed = receipt(record);
  observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
  if (!storage || !storage->privateCurrent().ok()) { observed.uncertain = true; return observed; }
  struct stat payload {};
  if (::fstatat(storage->descriptor(), "payload", &payload, AT_SYMLINK_NOFOLLOW) == 0) {
    observed.disposition = sameRoot(record.sourceIdentity, identity(payload))
        ? MutationRecoveryDisposition::SourceRetained : MutationRecoveryDisposition::UnexpectedEntryRetained;
    return observed;
  }
  MutationResult result;
  auto original = RecoveryDirectoryAdmission::open(QFileInfo(record.sourcePath).absolutePath(), result);
  if (original && original->observation() == record.sourceParent &&
      ::fstatat(original->descriptor(), leaf(record.sourcePath).constData(), &payload, AT_SYMLINK_NOFOLLOW) == 0 &&
      sameRoot(record.sourceIdentity, identity(payload)))
    observed.disposition = record.phase == RecoveryPhase::Restored ? MutationRecoveryDisposition::Restored
        : MutationRecoveryDisposition::SourceAtOriginal;
  else observed.uncertain = true;
  return observed;
}
}
MutationResult CrossVolumeMove::inspect(const QString &operationId, const MutationCancellation &cancel) {
  RecoveryCatalog catalog(m_catalogPath);
  auto result = catalog.admit(false);
  if (!result.ok()) {
    if (result.error == MutationError::Vanished && operationId.isEmpty()) {
      result = {}; result.diagnostic = QStringLiteral("Recovery inspection complete; no catalog is present.");
      return result;
    }
    return result;
  }
  const auto all = catalog.inspect();
  if (!all.result.ok()) return all.result;
  bool found = operationId.isEmpty();
  for (const auto &locator : all.records) {
    if (!operationId.isEmpty() && operationId != locator.operationId) continue;
    found = true;
    if (cancelled(cancel)) { result.error = MutationError::Cancelled; break; }
    std::optional<RecoveryDirectoryAdmission> storage;
    const auto read = currentRecord(locator, storage);
    auto observed = inspectPlacement(read.record ? *read.record : locator, storage);
    observed.restoreAvailable = read.result.ok() && read.record &&
        observed.disposition == MutationRecoveryDisposition::SourceRetained &&
        read.record->phase != RecoveryPhase::Restored;
    if (!read.result.ok()) {
      observed.uncertain = true;
      result.error = read.result.error;
      result.diagnostic = QStringLiteral("Recovery location unavailable or uncertain; its discovery record is preserved.");
    }
    result.recoveryReceipts.append(observed);
    result.recovery = observed;
  }
  if (!found) return failure(MutationError::Vanished, QStringLiteral("Recovery operation is not in this catalog."));
  if (result.ok()) result.diagnostic = QStringLiteral("Recovery inspection only; no operation was replayed.");
  return result;
}
MutationResult CrossVolumeMove::restore(const QString &operationId, const MutationCancellation &cancel) {
  RecoveryCatalog catalog(m_catalogPath);
  auto result = catalog.admit(false);
  if (!result.ok()) return result;
  const auto locator = catalog.lookup(operationId);
  if (!locator.result.ok() || !locator.record) return locator.result;
  std::optional<RecoveryDirectoryAdmission> storage;
  const auto read = currentRecord(*locator.record, storage);
  auto observed = inspectPlacement(read.record ? *read.record : *locator.record, storage);
  auto stop = [&](MutationResult error) { observed.uncertain = error.error != MutationError::Cancelled; error.recovery = observed; return error; };
  if (!read.result.ok() || !read.record || !storage) return stop(read.result);
  auto record = *read.record;
  if (record.phase == RecoveryPhase::Restored)
    return stop(failure(MutationError::InvalidRequest, QStringLiteral("This recovery operation was already restored; no replay.")));
  if (observed.disposition != MutationRecoveryDisposition::SourceRetained)
    return stop(failure(MutationError::Changed, QStringLiteral("Original source retention is unconfirmed; restore refused.")));
  // A process interruption may leave a valid write-ahead Retiring record.
  // A deliberate request supplies fresh admission; first durably record the
  // newly inspected required state. It does not resume the old Move.
  RecoveryRecordStore store(*storage, m_storeFault);
  if (record.phase != RecoveryPhase::Retained && record.phase != RecoveryPhase::Required) {
    if (!recoveryTransitionAllowed(record.phase, RecoveryPhase::Required))
      return stop(failure(MutationError::InvalidRequest, QStringLiteral("Recovery phase cannot admit restore.")));
    auto required = record; ++required.sequence; required.phase = RecoveryPhase::Required;
    required.uncertain = true;
    const auto written = store.append(required);
    if (!written.result.ok()) return stop(written.result);
    record = required;
  }
  auto target = RecoveryDirectoryAdmission::open(QFileInfo(record.sourcePath).absolutePath(), result);
  if (!target) return stop(result);
  if (target->observation() != record.sourceParent ||
      target->observation().device != storage->observation().device)
    return stop(failure(MutationError::Changed, QStringLiteral("Original parent or source volume changed; restore refused.")));
  const auto before = captureRecoveryManifest(*storage, "payload", cancel);
  if (!before.result.ok()) return stop(before.result);
  if (!before.manifest || !sameRoot(record.sourceIdentity, before.manifest->entries.front().identity))
    return stop(failure(MutationError::Changed, QStringLiteral("Retained payload identity changed; restore refused.")));
  if (cancelled(cancel)) return stop(failure(MutationError::Cancelled, QStringLiteral("Restore cancelled before rename.")));
  result = step(CrossVolumeStep::BeforeRestore); if (!result.ok()) return stop(result);
  result = target->current(); if (!result.ok()) return stop(result);
  result = storage->privateCurrent(); if (!result.ok()) return stop(result);
  const auto fenced = captureRecoveryManifest(*storage, "payload", cancel);
  if (!fenced.result.ok()) return stop(fenced.result);
  if (!fenced.manifest || !sameRecoveryTree(*before.manifest, *fenced.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Retained tree changed before restore.")));
  result = step(CrossVolumeStep::RestoreWindow); if (!result.ok()) return stop(result);
  result = renameNoReplace(storage->descriptor(), "payload", target->descriptor(), leaf(record.sourcePath));
  if (!result.ok()) return stop(result);
  observed.observedEffects = true;
  observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
  result = step(CrossVolumeStep::AfterRestore); if (!result.ok()) return stop(result);
  result = target->current(); if (!result.ok()) return stop(result);
  result = storage->privateCurrent(); if (!result.ok()) return stop(result);
  const auto restored = captureRecoveryManifest(*target, leaf(record.sourcePath), cancel);
  if (!restored.result.ok()) return stop(restored.result);
  if (!restored.manifest || !sameRecoveryTree(*before.manifest, *restored.manifest, true))
    return stop(failure(MutationError::Changed, QStringLiteral("Restored placement or contents changed; nothing was deleted.")));
  observed.disposition = MutationRecoveryDisposition::Restored;
  result = synchronize(target->descriptor()); if (!result.ok()) return stop(result);
  result = synchronize(storage->descriptor()); if (!result.ok()) return stop(result);
  auto final = record; ++final.sequence; final.phase = RecoveryPhase::Restored; final.uncertain = false;
  const auto written = store.append(final);
  if (!written.result.ok()) return stop(written.result);
  result = target->current(); if (!result.ok()) return stop(result);
  const auto completion = captureRecoveryManifest(*target, leaf(record.sourcePath), cancel);
  if (!completion.result.ok()) return stop(completion.result);
  if (!completion.manifest || !sameRecoveryTree(*restored.manifest, *completion.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Restored bytes changed during receipt durability.")));
  observed.phase = recoveryPhaseKey(final.phase); observed.uncertain = false;
  observed.restoreAvailable = false;
  result.recovery = observed;
  result.diagnostic = QStringLiteral("Current retained source restored; published destination was preserved.");
  return result;
}
} // namespace QindaQt::Apps::FileManager
