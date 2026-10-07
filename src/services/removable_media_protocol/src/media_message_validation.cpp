// SPDX-License-Identifier: LGPL-3.0-or-later
#include "media_validation_p.h"
#include <QtCore/QSet>

namespace QindaQt::RemovableMedia {
using namespace ValidationPrivate;
ValidationResult validateActionRequest(const ActionRequest &value) {
  if (value.protocolVersion != kProtocolVersion) return {ValueError::UnsupportedVersion};
  if (!validEnum(value.action, Action::ShowDetails)) return {ValueError::InvalidEnum};
  if (const auto error = identifier(value.requestId); error != ValueError::None) return {error};
  if (const auto error = lineage(value.lineage); error != ValueError::None) return {error};
  return {attachment(value.attachment)};
}
ValidationResult validateActionAdmission(const ActionAdmission &value) {
  if (const auto result = validateActionRequest(value.request); !result.accepted()) return result;
  if (!validEnum(value.status, AdmissionStatus::Invalid)) return {ValueError::InvalidEnum};
  if (value.status == AdmissionStatus::Accepted) {
    if (const auto error = identifier(value.operationId); error != ValueError::None) return {error};
  } else if (!value.operationId.isEmpty()) return {ValueError::InconsistentValue};
  return {diagnostic(value.diagnostic)};
}
ValidationResult validateOperationResult(const OperationResult &value) {
  if (const auto result = validateActionRequest(value.request); !result.accepted()) return result;
  if (const auto error = identifier(value.operationId); error != ValueError::None) return {error};
  if (!validEnum(value.status, OperationStatus::Uncertain)
      || value.status == OperationStatus::None
      || !validEnum(value.removalMode, RemovalMode::PoweredOff)) return {ValueError::InvalidEnum};
  if (value.confirmingRevision && (value.status != OperationStatus::Applied
      || *value.confirmingRevision <= value.request.lineage.revision)) return {ValueError::InconsistentValue};
  const bool applied = value.status == OperationStatus::Applied;
  if (applied && (value.request.action == Action::Mount
      || value.request.action == Action::MountReadOnly || value.request.action == Action::Unmount)
      && !value.confirmingRevision) return {ValueError::InconsistentValue};
  const bool removed = applied && value.request.action == Action::Remove;
  if (removed != (value.removalMode != RemovalMode::None)) return {ValueError::InconsistentValue};
  return {diagnostic(value.diagnostic)};
}
ValidationResult validateSnapshot(const Snapshot &value) {
  if (value.protocolVersion != kProtocolVersion) return {ValueError::UnsupportedVersion};
  if (!validEnum(value.availability, Availability::Unavailable)) return {ValueError::InvalidEnum};
  const bool ready = value.availability == Availability::Ready;
  if (const auto error = lineage(value.lineage, !ready); error != ValueError::None) return {error};
  if (!ready && (!value.rows.isEmpty() || value.pending)) return {ValueError::InconsistentValue};
  if (value.rows.size() > kMaxVolumes) return {ValueError::LimitExceeded};
  QSet<QString> drives, volumes, handles;
  for (const auto &volume : value.rows) {
    if (const auto error = row(volume); error != ValueError::None) return {error};
    if (volumes.contains(volume.volumeDisplayId) || handles.contains(volume.attachment.handle))
      return {ValueError::DuplicateIdentity};
    volumes.insert(volume.volumeDisplayId); handles.insert(volume.attachment.handle);
    drives.insert(volume.driveDisplayId);
  }
  if (drives.size() > kMaxDrives) return {ValueError::LimitExceeded};
  if (value.pending) {
    const auto &pending = *value.pending;
    if (const auto error = identifier(pending.operationId); error != ValueError::None) return {error};
    if (const auto result = validateActionRequest(pending.request); !result.accepted()) return result;
    if (!validEnum(pending.phase, ProgressPhase::Refreshing)
        || pending.phase == ProgressPhase::Idle) return {ValueError::InvalidEnum};
    if (pending.request.lineage.owner != value.lineage.owner
        || pending.request.lineage.epoch != value.lineage.epoch
        || pending.request.lineage.revision > value.lineage.revision)
      return {ValueError::InvalidLineage};
  }
  return {diagnostic(value.diagnostic)};
}
} // namespace QindaQt::RemovableMedia
