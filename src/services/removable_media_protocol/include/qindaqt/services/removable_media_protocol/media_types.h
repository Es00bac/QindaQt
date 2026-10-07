// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_limits.h>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <optional>

namespace QindaQt::RemovableMedia {
// Enum ordinals are closed protocol-v1 wire values, never UDisks constants.
enum class Availability : quint32 { Loading = 0, Ready = 1, Unavailable = 2 };
enum class MountState : quint32 { Unknown = 0, Unmounted = 1, Mounted = 2 };
enum class ReadOnlyState : quint32 { Unknown = 0, ReadOnly = 1, Writable = 2 };
enum class Action : quint32 {
  Mount = 0, MountReadOnly = 1, Unmount = 2, Remove = 3, ShowDetails = 4
};
enum class ProgressPhase : quint32 {
  Idle = 0, Confirming = 1, Mounting = 2, Unmounting = 3, Locking = 4,
  Ejecting = 5, PoweringOff = 6, Refreshing = 7
};
enum class OperationStatus : quint32 {
  None = 0, Applied = 1, Refused = 2, Cancelled = 3, Busy = 4, Gone = 5,
  Uncertain = 6
};
enum class RemovalMode : quint32 {
  None = 0, Unmounted = 1, Ejected = 2, PoweredOff = 3
};
enum class AdmissionStatus : quint32 {
  Accepted = 0, Unsupported = 1, Unavailable = 2, Stale = 3, Gone = 4,
  Busy = 5, NotAdmitted = 6, Invalid = 7
};
enum class DiagnosticCode : quint32 {
  None = 0, Unsupported = 1, Unavailable = 2, Stale = 3, Gone = 4,
  Busy = 5, NotAdmitted = 6, Invalid = 7, Cancelled = 8, Uncertain = 9
};
enum class DisabledReason : quint32 {
  None = 0, Unsupported = 1, Unavailable = 2, Stale = 3, Gone = 4,
  Busy = 5, Locked = 6, ReadOnly = 7, NotMounted = 8,
  AlreadyMounted = 9, NotAdmitted = 10
};

// All values own their text/collections. Copies may cross threads; callers
// synchronize shared mutable instances. No value owns a QObject or authority.
struct Diagnostic final {
  DiagnosticCode code = DiagnosticCode::None;
  QString message;
  bool operator==(const Diagnostic &) const = default;
};
struct Lineage final {
  QString owner;
  QString epoch;
  quint64 revision = 0;
  bool operator==(const Lineage &) const = default;
};
// AGENT-GUARD: ids, roots and handles are untrusted presentation/correlation
// data. Even a decoded handle requires current owning-backend admission.
struct Attachment final {
  QString handle;
  quint64 generation = 0;
  bool operator==(const Attachment &) const = default;
};
struct ActionAvailability final {
  bool enabled = false;
  DisabledReason reason = DisabledReason::Unsupported;
  bool operator==(const ActionAvailability &) const = default;
};
struct RowActions final {
  ActionAvailability open, mount, mountReadOnly, unmount, remove, showDetails;
  bool operator==(const RowActions &) const = default;
};
struct VolumeRow final {
  QString driveDisplayId, volumeDisplayId, displayName, kind;
  quint32 partitionNumber = 0; // zero means unknown/not a partition
  quint64 sizeBytes = 0;
  Attachment attachment;
  QStringList mountRoots;
  QString preferredRoot;
  MountState mountState = MountState::Unknown;
  ReadOnlyState readOnly = ReadOnlyState::Unknown;
  bool encrypted = false, locked = false, optical = false;
  RowActions actions;
  ProgressPhase progress = ProgressPhase::Idle;
  OperationStatus outcome = OperationStatus::None;
  Diagnostic diagnostic;
  bool operator==(const VolumeRow &) const = default;
};
// No format, unlock, arbitrary path/options, preference or credential fields.
struct ActionRequest final {
  quint32 protocolVersion = kProtocolVersion;
  QString requestId;
  Lineage lineage;
  Attachment attachment;
  Action action = Action::ShowDetails;
  bool operator==(const ActionRequest &) const = default;
};
struct PendingOperation final {
  QString operationId;
  ActionRequest request;
  ProgressPhase phase = ProgressPhase::Confirming;
  bool operator==(const PendingOperation &) const = default;
};
struct Snapshot final {
  quint32 protocolVersion = kProtocolVersion;
  Lineage lineage;
  Availability availability = Availability::Loading;
  QList<VolumeRow> rows;
  // Removal may retire its row before its final callback; initiating lineage
  // remains recorded here without turning the retired handle into authority.
  std::optional<PendingOperation> pending;
  Diagnostic diagnostic;
  bool operator==(const Snapshot &) const = default;
};
struct ActionAdmission final {
  ActionRequest request;
  AdmissionStatus status = AdmissionStatus::Invalid;
  QString operationId; // nonempty exactly for Accepted
  Diagnostic diagnostic;
  bool operator==(const ActionAdmission &) const = default;
};
struct OperationResult final {
  ActionRequest request;
  QString operationId;
  OperationStatus status = OperationStatus::Uncertain;
  Diagnostic diagnostic;
  std::optional<quint64> confirmingRevision;
  // Only Applied Remove has a non-None mode; it reports owner-observed final
  // steps. Codec validity cannot establish physical safety or convergence.
  RemovalMode removalMode = RemovalMode::None;
  bool operator==(const OperationResult &) const = default;
};
} // namespace QindaQt::RemovableMedia
