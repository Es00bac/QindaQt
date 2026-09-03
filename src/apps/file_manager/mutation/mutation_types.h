// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

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

} // namespace QindaQt::Apps::FileManager
