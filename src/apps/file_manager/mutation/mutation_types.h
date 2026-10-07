// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Apps::FileManager {

enum class MutationKind {
  CreateFolder,
  Rename,
  Copy,
  Move,
  Trash,
  Restore,
  EmptyTrash,
  // ADR-0269 (the right-click set). CreateFile makes an empty regular file
  // at destinationPath; Link makes a symbolic link there whose literal
  // target is linkTarget; Delete removes sourcePath's tree permanently,
  // never through Trash; Compress writes a new archive at destinationPath
  // holding archiveSources; Extract unpacks the archive at sourcePath into a
  // new folder at destinationPath.
  CreateFile,
  Link,
  Delete,
  Compress,
  Extract,
};

enum class MutationError {
  None,
  InvalidRequest,
  PermissionDenied,
  AlreadyExists,
  CrossDevice,
  DiskFull,
  Vanished,
  Changed,
  SymlinkEscape,
  Cancelled,
  Unsupported,
  IoError,
  Busy,
};

struct FileIdentity final {
  quint64 device = 0;
  quint64 inode = 0;
  qint64 size = 0;
  qint64 modifiedNanoseconds = 0;
  quint32 mode = 0;

  [[nodiscard]] bool operator==(const FileIdentity &) const = default;
  [[nodiscard]] bool valid() const { return device != 0 && inode != 0; }
};

struct MutationRequest final {
  MutationKind kind = MutationKind::CreateFolder;
  QString sourcePath;
  QString destinationPath;
  QString trashToken;
  QStringList declaredRoots;
  std::optional<FileIdentity> expectedSource;
  std::optional<FileIdentity> expectedParent;
  // Link only: the new link's literal target text (the source's own name,
  // since a link is made beside what it points to).
  QString linkTarget = {};
  // Compress only: every item the archive holds, each with the identity the
  // user saw; sourcePath and expectedSource stay empty.
  QStringList archiveSources = {};
  QVector<FileIdentity> archiveSourceIdentities = {};
};

struct MutationProgress final {
  int completedItems = 0;
  int totalItems = 0;
  QString accessibleText;
};

// AGENT-CONTRACT: These are immutable observations from one worker call,
// never authority to open, remove, restore or reuse a filesystem entry.
enum class MutationOutputDisposition {
  None,
  RetainedPartial,
  RetainedCopy,
  Replaced,
  Unconfirmed,
};

struct MutationOutputObservation final {
  MutationOutputDisposition disposition = MutationOutputDisposition::None;
  QString path;
  // The descriptor written by this copy. A directory's mkdir/open pair is
  // not atomic; only an O_EXCL regular-file descriptor proves creation.
  std::optional<FileIdentity> writtenIdentity;
  bool exclusiveCreation = false;
  // Traversal/write/fsync reached its end; not a content manifest, source
  // postcheck, or a guarantee the current destination is this descriptor.
  bool copyFinished = false;
  std::optional<FileIdentity> observedIdentity;
  std::optional<FileIdentity> parentIdentity;
};

struct MutationItemOutcome final {
  bool attempted = false;
  QString sourcePath;
  QString destinationPath;
  MutationError error = MutationError::None;
  MutationOutputObservation output;
};

struct MutationResult final {
  MutationError error = MutationError::None;
  QString diagnostic;
  QString outputPath;
  QString trashToken;
  QString originalPath;
  std::optional<FileIdentity> outputIdentity;
  std::shared_ptr<MutationRequest> undoRequest;
  MutationOutputObservation outputObservation;
  // Controller value copies in request order, including unattempted suffix.
  QVector<MutationItemOutcome> itemOutcomes;

  [[nodiscard]] bool ok() const { return error == MutationError::None; }
};

using MutationCancellation = std::shared_ptr<std::atomic_bool>;
using MutationProgressCallback = std::function<void(const MutationProgress &)>;

[[nodiscard]] QString mutationOutputDispositionKey(MutationOutputDisposition value);
[[nodiscard]] QString mutationErrorKey(MutationError error);
[[nodiscard]] QString boundedMutationDiagnostic(const QString &message);
// The typed error for a failed system call's errno (ADR-0269 code uses this
// one mapping instead of another private copy).
[[nodiscard]] MutationError mutationErrorForErrno(int error);

} // namespace QindaQt::Apps::FileManager
