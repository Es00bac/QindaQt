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

struct MutationResult final {
  MutationError error = MutationError::None;
  QString diagnostic;
  QString outputPath;
  QString trashToken;
  QString originalPath;
  std::optional<FileIdentity> outputIdentity;
  std::shared_ptr<MutationRequest> undoRequest;

  [[nodiscard]] bool ok() const { return error == MutationError::None; }
};

using MutationCancellation = std::shared_ptr<std::atomic_bool>;
using MutationProgressCallback = std::function<void(const MutationProgress &)>;

[[nodiscard]] QString mutationErrorKey(MutationError error);
[[nodiscard]] QString boundedMutationDiagnostic(const QString &message);
// The typed error for a failed system call's errno (ADR-0269 code uses this
// one mapping instead of another private copy).
[[nodiscard]] MutationError mutationErrorForErrno(int error);

} // namespace QindaQt::Apps::FileManager
