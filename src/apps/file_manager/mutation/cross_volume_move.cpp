// SPDX-License-Identifier: GPL-3.0-or-later
#include "cross_volume_move_p.h"
#include "safe_tree_operations.h"
#include <QUuid>
namespace QindaQt::Apps::FileManager {
using namespace RecoveryMovePrivate;
CrossVolumeMove::CrossVolumeMove(QString path, CrossVolumeFault fault, RecoveryStoreFault storeFault)
    : m_catalogPath(std::move(path)), m_fault(std::move(fault)), m_storeFault(std::move(storeFault)) {}
MutationResult CrossVolumeMove::step(CrossVolumeStep phase) const {
  const auto error = m_fault ? m_fault(phase) : MutationError::None;
  return error == MutationError::None ? MutationResult{} :
      failure(error, QStringLiteral("Move interrupted; all source and destination candidates are preserved."));
}
MutationResult CrossVolumeMove::move(const MutationRequest &request,
    const MutationCancellation &cancel, const MutationProgressCallback &progress) {
  MutationResult result;
  for (const auto &path : {request.sourcePath, request.destinationPath}) {
    if (path.size() > 4096) return failure(MutationError::InvalidRequest, QStringLiteral("Move path exceeds its bound."));
    const auto encoded = QFile::encodeName(path);
    if (!path.startsWith(QLatin1Char('/')) || path == QStringLiteral("/") ||
        QDir::cleanPath(path) != path || path.contains(QChar::Null) ||
        encoded.size() > 4096 || QFile::decodeName(encoded) != path)
      return failure(MutationError::InvalidRequest, QStringLiteral("Move requires bounded literal normalized paths."));
  }
  auto source = RecoveryDirectoryAdmission::open(QFileInfo(request.sourcePath).absolutePath(), result);
  if (!source) return result;
  auto destination = RecoveryDirectoryAdmission::open(QFileInfo(request.destinationPath).absolutePath(), result);
  if (!destination) return result;
  const auto sourceName = leaf(request.sourcePath), destinationName = leaf(request.destinationPath);
  if (!request.expectedSource || !request.expectedParent || sourceName.isEmpty() || destinationName.isEmpty() ||
      source->observation().device == destination->observation().device ||
      destination->observation().device != request.expectedParent->device ||
      destination->observation().inode != request.expectedParent->inode)
    return failure(MutationError::CrossDevice, QStringLiteral("Cross-volume admission does not match the requested identities."));
  const auto original = captureRecoveryManifest(*source, sourceName, cancel, progress);
  if (!original.result.ok() || !original.manifest) return original.result;
  if (original.manifest->entries.front().identity != *request.expectedSource)
    return failure(MutationError::Changed, QStringLiteral("The selected source changed before Move."));
  struct stat existing {};
  if (::fstatat(destination->descriptor(), destinationName.constData(), &existing, AT_SYMLINK_NOFOLLOW) == 0)
    return failure(MutationError::AlreadyExists, QStringLiteral("The destination exists; nothing was changed."));
  if (errno != ENOENT) return failure(errorForErrno(errno), QStringLiteral("The destination cannot be admitted."));
  RecoveryCatalog catalog(m_catalogPath);
  result = catalog.admit(true); if (!result.ok()) return result;
  const auto capacity = catalog.inspect();
  if (!capacity.result.ok()) return capacity.result;
  if (capacity.records.size() >= maximumRecoveryOperations)
    return failure(MutationError::Unsupported, QStringLiteral("Recovery is full; no retained data was evicted."));
  RecoveryRecord record;
  record.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  record.sourcePath = request.sourcePath; record.destinationPath = request.destinationPath;
  record.sourceIdentity = *request.expectedSource;
  record.sourceParent = source->observation(); record.destinationParent = destination->observation();
  record.manifestDigest = recoveryManifestDigest(*original.manifest, true);
  record.retainedBytesEstimate = original.manifest->regularBytes;
  auto recovery = createRecoveryDirectory(*source, QStringLiteral(".qindaqt-recovery-") + record.operationId, result);
  if (!recovery) return result;
  record.recoveryDirectory = recovery->path(); record.recoveryStorage = recovery->observation();
  auto stage = createRecoveryDirectory(*destination, QStringLiteral(".qindaqt-stage-") + record.operationId, result);
  if (!stage) { result.recovery = receipt(record); result.recovery.observedEffects = true; return result; }
  record.stageDirectory = stage->path();
  RecoveryRecordStore store(*recovery, m_storeFault);
  MutationRecoveryReceipt observed = receipt(record);
  observed.disposition = MutationRecoveryDisposition::SourceAtOriginal; observed.observedEffects = true;
  bool retired = false;
  auto stop = [&](MutationResult error) {
    if (retired) {
      struct stat now {};
      if (!recovery->privateCurrent().ok() ||
          ::fstatat(recovery->descriptor(), "payload", &now, AT_SYMLINK_NOFOLLOW) != 0)
        observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
      else if (!sameRoot(record.sourceIdentity, identity(now)))
        observed.disposition = MutationRecoveryDisposition::UnexpectedEntryRetained;
      else observed.disposition = MutationRecoveryDisposition::SourceRetained;
    }
    else {
      struct stat named {};
      if (!source->current().ok() ||
          ::fstatat(source->descriptor(), sourceName.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
          !sameRoot(record.sourceIdentity, identity(named)))
        observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
    }
    record.uncertain = error.error != MutationError::Cancelled ||
        observed.disposition == MutationRecoveryDisposition::DestinationPublished || retired;
    record.diagnostic = boundedMutationDiagnostic(error.diagnostic).left(512);
    observed.uncertain = record.uncertain;
    if (recoveryTransitionAllowed(record.phase, RecoveryPhase::Required)) {
      RecoveryRecord failed = record; ++failed.sequence; failed.phase = RecoveryPhase::Required;
      const auto written = store.append(failed);
      if (written.result.ok()) record = failed;
    }
    observed.phase = recoveryPhaseKey(record.phase);
    const auto journal = store.inspect();
    observed.restoreAvailable = journal.result.ok() && journal.record &&
        observed.disposition == MutationRecoveryDisposition::SourceRetained && source->current().ok();
    error.recovery = observed;
    return error;
  };
  auto advance = [&](RecoveryPhase phase) {
    RecoveryRecord next = record;
    if (phase != RecoveryPhase::Prepared) ++next.sequence;
    next.phase = phase;
    const auto written = store.append(next);
    if (written.result.ok()) { record = next; observed.phase = recoveryPhaseKey(phase); }
    return written.result;
  };
  auto fence = [&]() {
    for (const auto *admission : {&*source, &*destination, &*stage, &*recovery}) {
      const auto current = admission->current(); if (!current.ok()) return current;
    }
    if (cancelled(cancel)) return failure(MutationError::Cancelled, QStringLiteral("Move cancelled; retained bytes were not removed."));
    return MutationResult{};
  };
  result = advance(RecoveryPhase::Prepared); if (!result.ok()) return stop(result);
  result = catalog.add(record); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::Prepared); if (!result.ok()) return stop(result);
  result = advance(RecoveryPhase::Copying); if (!result.ok()) return stop(result);
  result = fence(); if (!result.ok()) return stop(result);
  observed.disposition = MutationRecoveryDisposition::PartialStage;
  result = step(CrossVolumeStep::Copy); if (!result.ok()) return stop(result);
  result = copyRecoveryTreeAt(source->descriptor(), sourceName, stage->descriptor(), "payload", cancel, progress, 20000);
  if (!result.ok()) return stop(result);
  const auto copied = captureRecoveryManifest(*stage, "payload", cancel, progress);
  const auto checkedSource = captureRecoveryManifest(*source, sourceName, cancel, progress);
  if (!copied.result.ok()) return stop(copied.result);
  if (!checkedSource.result.ok()) return stop(checkedSource.result);
  if (!copied.manifest || !checkedSource.manifest ||
      !sameContent(*original.manifest, *copied.manifest) || !sameRecoveryTree(*original.manifest, *checkedSource.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Source or copied content changed; source retained at its original name.")));
  result = synchronize(stage->descriptor()); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::Verified); if (!result.ok()) return stop(result);
  result = advance(RecoveryPhase::Verified); if (!result.ok()) return stop(result);
  std::optional<RecoveryManifest> publishedSnapshot;
  auto destinationVerified = [&]() {
    auto current = destination->current(); if (!current.ok()) return current;
    const auto now = captureRecoveryManifest(*destination, destinationName, cancel, progress);
    if (!now.result.ok()) return now.result;
    const auto &reference = publishedSnapshot ? *publishedSnapshot : *copied.manifest;
    if (!now.manifest || !sameRecoveryTree(reference, *now.manifest, !publishedSnapshot))
      return failure(MutationError::Changed, QStringLiteral("Published destination changed; no source data was deleted."));
    if (!publishedSnapshot) publishedSnapshot = *now.manifest;
    return MutationResult{};
  };
  result = step(CrossVolumeStep::BeforePublish); if (!result.ok()) return stop(result);
  result = fence(); if (!result.ok()) return stop(result);
  const auto finalStage = captureRecoveryManifest(*stage, "payload", cancel, progress);
  if (!finalStage.result.ok()) return stop(finalStage.result);
  if (!finalStage.manifest || !sameRecoveryTree(*copied.manifest, *finalStage.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Staging entry was replaced; preserve it.")));
  result = step(CrossVolumeStep::PublicationWindow); if (!result.ok()) return stop(result);
  result = renameNoReplace(stage->descriptor(), "payload", destination->descriptor(), destinationName);
  if (!result.ok()) return stop(result);
  observed.disposition = MutationRecoveryDisposition::DestinationPublished;
  result = step(CrossVolumeStep::AfterPublish); if (!result.ok()) return stop(result);
  result = destinationVerified(); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::SyncDestination); if (!result.ok()) return stop(result);
  result = synchronize(destination->descriptor()); if (!result.ok()) return stop(result);
  result = advance(RecoveryPhase::Published); if (!result.ok()) return stop(result);
  result = advance(RecoveryPhase::Retiring); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::BeforeRetirement); if (!result.ok()) return stop(result);
  result = fence(); if (!result.ok()) return stop(result);
  result = destinationVerified(); if (!result.ok()) return stop(result);
  const auto retiring = captureRecoveryManifest(*source, sourceName, cancel, progress);
  if (!retiring.result.ok()) return stop(retiring.result);
  if (!retiring.manifest || !sameRecoveryTree(*original.manifest, *retiring.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Source changed before retirement; leave it at its original name.")));
  result = step(CrossVolumeStep::RetirementWindow); if (!result.ok()) return stop(result);
  result = renameNoReplace(source->descriptor(), sourceName, recovery->descriptor(), "payload");
  if (!result.ok()) return stop(result);
  retired = true;
  // AGENT-GUARD: establish captured placement independently of destination or
  // cancellation. No recursive unlink exists, even for unexpected captured data.
  const auto retained = captureRecoveryManifest(*recovery, "payload");
  if (!retained.result.ok() || !retained.manifest) {
    observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
    return stop(retained.result);
  }
  if (!sameRoot(original.manifest->entries.front().identity, retained.manifest->entries.front().identity)) {
    observed.disposition = MutationRecoveryDisposition::UnexpectedEntryRetained;
    // AGENT-GUARD: return only this stable captured entry, into the exact
    // original parent with an absent name. Never overwrite or recursively
    // remove a foreign candidate. Cancellation/mount loss forbids repair.
    auto repair = step(CrossVolumeStep::BeforeUnexpectedReturn);
    if (repair.ok()) repair = fence();
    if (repair.ok()) {
      const auto current = captureRecoveryManifest(*recovery, "payload", cancel, progress);
      if (!current.result.ok()) repair = current.result;
      else if (!current.manifest || !sameRecoveryTree(*retained.manifest, *current.manifest))
        repair = failure(MutationError::Changed, QStringLiteral("Captured entry changed before repair."));
    }
    if (repair.ok()) {
      record.uncertain = true;
      record.diagnostic = QStringLiteral("Unexpected capture; best-effort no-replace return pending. Original source unconfirmed.");
      repair = advance(RecoveryPhase::Required);
    }
    struct stat occupied {};
    if (repair.ok()) {
      if (::fstatat(source->descriptor(), sourceName.constData(), &occupied, AT_SYMLINK_NOFOLLOW) == 0)
        repair = failure(MutationError::AlreadyExists, QStringLiteral("The original name is occupied."));
      else if (errno != ENOENT)
        repair = failure(errorForErrno(errno), QStringLiteral("The original name cannot be admitted for repair."));
    }
    if (repair.ok()) repair = fence();
    if (repair.ok()) {
      const auto finalCapture = captureRecoveryManifest(*recovery, "payload", cancel, progress);
      if (!finalCapture.result.ok()) repair = finalCapture.result;
      else if (!finalCapture.manifest || !sameRecoveryTree(*retained.manifest, *finalCapture.manifest))
        repair = failure(MutationError::Changed, QStringLiteral("Captured entry changed during repair admission."));
    }
    if (repair.ok()) repair = step(CrossVolumeStep::UnexpectedReturnWindow);
    if (repair.ok()) repair = renameNoReplace(recovery->descriptor(), "payload", source->descriptor(), sourceName);
    if (!repair.ok())
      return stop(failure(MutationError::Changed, QStringLiteral("Unexpected entry retained; safe return was refused (%1). Original source placement is unconfirmed.").arg(repair.diagnostic)));
    retired = false;
    observed.disposition = MutationRecoveryDisposition::UnknownPlacement;
    // The rename is an observed effect even if readback/durability fails.
    // It does not prove the selected original source returned.
    repair = step(CrossVolumeStep::AfterUnexpectedReturn);
    if (repair.ok()) repair = synchronize(recovery->descriptor());
    if (repair.ok()) repair = synchronize(source->descriptor());
    if (repair.ok()) repair = fence();
    if (repair.ok()) {
      const auto returned = captureRecoveryManifest(*source, sourceName, cancel, progress);
      if (!returned.result.ok()) repair = returned.result;
      else if (!returned.manifest || !sameRecoveryTree(*retained.manifest, *returned.manifest, true))
        repair = failure(MutationError::Changed, QStringLiteral("Returned unexpected entry changed during repair."));
    }
    return stop(failure(MutationError::Changed, repair.ok()
        ? QStringLiteral("Unexpected captured entry returned to its vacant original name; selected original source placement remains unconfirmed.")
        : QStringLiteral("Unexpected-entry return occurred, but verification or durability failed (%1); inspect all candidates. Selected original source placement remains unconfirmed.").arg(repair.diagnostic)));
  }
  observed.disposition = MutationRecoveryDisposition::SourceRetained;
  result = step(CrossVolumeStep::AfterRetirement); if (!result.ok()) return stop(result);
  if (!sameRecoveryTree(*original.manifest, *retained.manifest, true))
    return stop(failure(MutationError::Changed, QStringLiteral("Retained source has changed contents; inspect current retained bytes.")));
  result = step(CrossVolumeStep::SyncRecovery); if (!result.ok()) return stop(result);
  result = synchronize(recovery->descriptor()); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::SyncSource); if (!result.ok()) return stop(result);
  result = synchronize(source->descriptor()); if (!result.ok()) return stop(result);
  result = step(CrossVolumeStep::FinalVerification); if (!result.ok()) return stop(result);
  result = fence(); if (!result.ok()) return stop(result);
  result = destinationVerified(); if (!result.ok()) return stop(result);
  const auto finalRetained = captureRecoveryManifest(*recovery, "payload", cancel, progress);
  if (!finalRetained.result.ok()) return stop(finalRetained.result);
  if (!finalRetained.manifest || !sameRecoveryTree(*retained.manifest, *finalRetained.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Retained source changed during final verification.")));
  result = advance(RecoveryPhase::Retained); if (!result.ok()) return stop(result);
  result = fence(); if (!result.ok()) return stop(result);
  result = destinationVerified(); if (!result.ok()) return stop(result);
  const auto completionRetained = captureRecoveryManifest(*recovery, "payload", cancel, progress);
  if (!completionRetained.result.ok()) return stop(completionRetained.result);
  if (!completionRetained.manifest || !sameRecoveryTree(*retained.manifest, *completionRetained.manifest))
    return stop(failure(MutationError::Changed, QStringLiteral("Retained bytes changed during receipt durability.")));
  // AGENT-GUARD: a newly created source name belongs to its creator; do not
  // overwrite it or claim the original name was vacated at completion.
  struct stat sourceReplacement {};
  if (::fstatat(source->descriptor(), sourceName.constData(), &sourceReplacement, AT_SYMLINK_NOFOLLOW) == 0)
    return stop(failure(MutationError::AlreadyExists, QStringLiteral("A new entry occupies the source name; retained original and destination are preserved.")));
  if (errno != ENOENT)
    return stop(failure(errorForErrno(errno), QStringLiteral("The original source name cannot be verified absent.")));
  observed.disposition = MutationRecoveryDisposition::CompletedWithRetention;
  observed.restoreAvailable = true;
  result.recovery = observed;
  result.diagnostic = QStringLiteral("Moved; source retained for recovery (%1 bytes on the original volume)").arg(record.retainedBytesEstimate);
  result.outputPath = record.destinationPath;
  result.outputIdentity = copied.manifest->entries.front().identity;
  auto undo = std::make_shared<MutationRequest>(); undo->kind = MutationKind::RestoreRecovery;
  undo->recoveryOperationId = record.operationId; result.undoRequest = std::move(undo);
  return result;
}
} // namespace QindaQt::Apps::FileManager
