// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_mutation_backend.h"

#include "safe_path_operations.h"
#include "safe_tree_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] MutationResult failure(MutationError error, const QString &message) {
  MutationResult result;
  result.error = error;
  result.diagnostic = boundedMutationDiagnostic(message);
  return result;
}

[[nodiscard]] MutationError errorForErrno(int error) {
  switch (error) {
  case EACCES:
  case EPERM:
    return MutationError::PermissionDenied;
  case EEXIST:
  case ENOTEMPTY:
    return MutationError::AlreadyExists;
  case EXDEV:
    return MutationError::CrossDevice;
  case ENOSPC:
#ifdef EDQUOT
  case EDQUOT:
#endif
    return MutationError::DiskFull;
  case ENOENT:
  case ENOTDIR:
    return MutationError::Vanished;
  case ENAMETOOLONG:
    return MutationError::InvalidRequest;
  case ELOOP:
    return MutationError::SymlinkEscape;
  default:
    return MutationError::IoError;
  }
}

[[nodiscard]] bool containsPath(const QString &root, const QString &path) {
  const QString cleanRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
  const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

[[nodiscard]] MutationResult validatePath(const QString &path,
                                          const QStringList &roots,
                                          bool mayBeMissing) {
  if (!QFileInfo(path).isAbsolute() || roots.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("The operation requires absolute paths and a declared root"));
  }
  QString matchingRoot;
  for (const QString &root : roots) {
    if (QFileInfo(root).isAbsolute() && containsPath(root, path)) {
      matchingRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
      break;
    }
  }
  if (matchingRoot.isEmpty()) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("The path is outside the operation's declared roots"));
  }

  QString current = QStringLiteral("/");
  QStringList paths;
  const QStringList parts = QDir::cleanPath(path).split(
      QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    current = QDir(current).filePath(part);
    paths.append(current);
  }
  for (qsizetype index = 0; index < paths.size(); ++index) {
    struct stat status {};
    if (::lstat(QFile::encodeName(paths.at(index)).constData(), &status) != 0) {
      if (errno == ENOENT && mayBeMissing) {
        return {};
      }
      return failure(errorForErrno(errno), QStringLiteral("A path component vanished"));
    }
    if (S_ISLNK(status.st_mode)) {
      return failure(MutationError::SymlinkEscape,
                     QStringLiteral("Symbolic links are not followed by file operations"));
    }
  }
  return {};
}

[[nodiscard]] MutationResult verifyIdentity(
    const QString &path, const std::optional<FileIdentity> &expected) {
  if (!expected || !expected->valid()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("The operation has no valid identity precondition"));
  }
  const auto current = LocalMutationBackend::identityForPath(path);
  if (!current) {
    return failure(MutationError::Vanished,
                   QStringLiteral("The selected item vanished before the operation began"));
  }
  if (*current != *expected) {
    return failure(MutationError::Changed,
                   QStringLiteral("The selected item changed before the operation began"));
  }
  return {};
}

[[nodiscard]] bool cancelled(const MutationCancellation &token) {
  return token && token->load(std::memory_order_relaxed);
}

} // namespace

LocalMutationBackend::LocalMutationBackend(QString homeTrashRoot,
                                           DeviceResolverPtr deviceResolver)
    : m_deviceResolver(deviceResolver),
      m_homeTrash(std::move(homeTrashRoot), std::move(deviceResolver)) {}

std::optional<FileIdentity>
LocalMutationBackend::identityForPath(const QString &path) {
  struct stat status {};
  if (::lstat(QFile::encodeName(path).constData(), &status) != 0) {
    return std::nullopt;
  }
#if defined(Q_OS_LINUX)
  const qint64 modified = static_cast<qint64>(status.st_mtim.tv_sec) * 1'000'000'000LL +
                          status.st_mtim.tv_nsec;
#else
  const qint64 modified = static_cast<qint64>(status.st_mtime) * 1'000'000'000LL;
#endif
  return FileIdentity{static_cast<quint64>(status.st_dev),
                      static_cast<quint64>(status.st_ino),
                      static_cast<qint64>(status.st_size), modified,
                      static_cast<quint32>(status.st_mode)};
}

MutationResult LocalMutationBackend::execute(
    const MutationRequest &request, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress) {
  if (cancelled(cancellation)) {
    return failure(MutationError::Cancelled, QStringLiteral("Operation cancelled"));
  }
  switch (request.kind) {
  case MutationKind::CreateFolder:
    return createFolder(request);
  case MutationKind::Rename:
    return relocate(request, true);
  case MutationKind::Move:
    return relocate(request, false);
  case MutationKind::Copy:
    return copy(request, cancellation, progress);
  case MutationKind::Trash:
    return m_homeTrash.trash(request);
  case MutationKind::Restore:
    return m_homeTrash.restore(request);
  case MutationKind::EmptyTrash:
    return m_homeTrash.empty(cancellation, progress);
  }
  return failure(MutationError::Unsupported, QStringLiteral("Unsupported operation"));
}

MutationResult LocalMutationBackend::createFolder(const MutationRequest &request) {
  const QString parent = QFileInfo(request.destinationPath).absolutePath();
  if (const auto valid = validatePath(request.destinationPath, request.declaredRoots, true);
      !valid.ok()) {
    return valid;
  }
  if (const auto identity = verifyIdentity(parent, request.expectedParent); !identity.ok()) {
    return identity;
  }
  MutationResult result = createLocalDirectoryNoFollow(
      request.destinationPath, *request.expectedParent);
  if (!result.ok()) {
    return result;
  }
  result.outputIdentity = identityForPath(result.outputPath);
  auto undo = std::make_shared<MutationRequest>();
  undo->kind = MutationKind::Trash;
  undo->sourcePath = result.outputPath;
  undo->declaredRoots = request.declaredRoots;
  undo->expectedSource = result.outputIdentity;
  result.undoRequest = std::move(undo);
  return result;
}

MutationResult LocalMutationBackend::relocate(const MutationRequest &request,
                                              bool renameOnly) {
  if (const auto valid = validatePath(request.sourcePath, request.declaredRoots, false);
      !valid.ok()) {
    return valid;
  }
  if (const auto valid = validatePath(request.destinationPath, request.declaredRoots, true);
      !valid.ok()) {
    return valid;
  }
  if (const auto identity = verifyIdentity(request.sourcePath, request.expectedSource);
      !identity.ok()) {
    return identity;
  }
  const QString destinationParent =
      QFileInfo(request.destinationPath).absolutePath();
  if (const auto identity = verifyIdentity(destinationParent,
                                           request.expectedParent);
      !identity.ok()) {
    return identity;
  }
  if (renameOnly && QFileInfo(request.sourcePath).absolutePath() !=
                        QFileInfo(request.destinationPath).absolutePath()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Rename cannot change the parent folder"));
  }
  const auto sourceDevice = m_deviceResolver->deviceForPath(request.sourcePath);
  const auto destinationDevice = m_deviceResolver->deviceForPath(
      QFileInfo(request.destinationPath).absolutePath());
  if (!sourceDevice || !destinationDevice) {
    return failure(MutationError::Vanished, QStringLiteral("A source or destination vanished"));
  }
  if (*sourceDevice != *destinationDevice) {
    return failure(MutationError::CrossDevice,
                   QStringLiteral("Moving across filesystems is not supported"));
  }
  MutationResult result = relocateLocalNoFollow(
      request.sourcePath, request.destinationPath, *request.expectedSource,
      *request.expectedParent);
  if (!result.ok()) {
    return result;
  }
  result.outputIdentity = identityForPath(result.outputPath);
  auto undo = std::make_shared<MutationRequest>();
  undo->kind = request.kind;
  undo->sourcePath = result.outputPath;
  undo->destinationPath = request.sourcePath;
  undo->declaredRoots = request.declaredRoots;
  undo->expectedSource = result.outputIdentity;
  undo->expectedParent = identityForPath(
      QFileInfo(request.sourcePath).absolutePath());
  result.undoRequest = std::move(undo);
  return result;
}

MutationResult LocalMutationBackend::copy(
    const MutationRequest &request, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress) {
  if (const auto valid = validatePath(request.sourcePath, request.declaredRoots, false);
      !valid.ok()) {
    return valid;
  }
  if (const auto valid = validatePath(request.destinationPath, request.declaredRoots, true);
      !valid.ok()) {
    return valid;
  }
  if (containsPath(request.sourcePath, request.destinationPath)) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("A folder cannot be copied into itself"));
  }
  if (const auto identity = verifyIdentity(request.sourcePath, request.expectedSource);
      !identity.ok()) {
    return identity;
  }
  const QString destinationParent =
      QFileInfo(request.destinationPath).absolutePath();
  if (const auto identity = verifyIdentity(destinationParent,
                                           request.expectedParent);
      !identity.ok()) {
    return identity;
  }
  if (QFileInfo::exists(request.destinationPath)) {
    return failure(MutationError::AlreadyExists, QStringLiteral("The destination already exists"));
  }
  MutationResult result = copyLocalTreeNoFollow(
      request.sourcePath, request.destinationPath, *request.expectedParent,
      cancellation, progress,
      maximumCopiedItems);
  const auto finalIdentity = identityForPath(request.sourcePath);
  if (result.ok() && !finalIdentity) {
    const bool removedPartial = removeLocalTreeNoFollow(request.destinationPath);
    Q_UNUSED(removedPartial);
    return failure(MutationError::Vanished,
                   QStringLiteral("The source vanished while it was being copied"));
  }
  if (result.ok() && *finalIdentity != *request.expectedSource) {
    const bool removedPartial = removeLocalTreeNoFollow(request.destinationPath);
    Q_UNUSED(removedPartial);
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed while it was being copied"));
  }
  if (result.ok()) {
    result.outputIdentity = identityForPath(result.outputPath);
  }
  return result;
}

} // namespace QindaQt::Apps::FileManager
