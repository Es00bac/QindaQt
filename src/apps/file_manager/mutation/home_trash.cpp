// SPDX-License-Identifier: GPL-3.0-or-later
#include "home_trash.h"

#include "local_mutation_backend.h"
#include "safe_path_operations.h"
#include "safe_tree_operations.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <cerrno>
#include <sys/stat.h>

#include <algorithm>

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
  default:
    return MutationError::IoError;
  }
}

[[nodiscard]] bool containsPath(const QString &root, const QString &path) {
  const QString cleanRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
  const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

[[nodiscard]] MutationResult validateNoSymlinkPath(const QString &path,
                                                   bool mayBeMissing) {
  if (!QFileInfo(path).isAbsolute()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Trash requires an absolute local path"));
  }
  QString current = QStringLiteral("/");
  const QStringList parts = QDir::cleanPath(path).split(
      QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    current = QDir(current).filePath(part);
    struct stat status {};
    if (::lstat(QFile::encodeName(current).constData(), &status) != 0) {
      if (errno == ENOENT && mayBeMissing) {
        return {};
      }
      return failure(errorForErrno(errno),
                     QStringLiteral("A Trash path component is unavailable"));
    }
    if (S_ISLNK(status.st_mode)) {
      return failure(MutationError::SymlinkEscape,
                     QStringLiteral("Trash does not follow symbolic links"));
    }
  }
  return {};
}

[[nodiscard]] MutationResult validateTrashStorage(const QString &root,
                                                   bool mayBeMissing) {
  for (const QString &path :
       {root, QDir(root).filePath(QStringLiteral("info")),
        QDir(root).filePath(QStringLiteral("files"))}) {
    if (const auto valid = validateNoSymlinkPath(path, mayBeMissing); !valid.ok()) {
      return valid;
    }
  }
  return {};
}

[[nodiscard]] MutationResult validateDestination(const QString &path,
                                                 const QStringList &roots) {
  QString matchingRoot;
  for (const QString &root : roots) {
    if (QFileInfo(root).isAbsolute() && containsPath(root, path)) {
      matchingRoot = QDir::cleanPath(root);
      break;
    }
  }
  if (matchingRoot.isEmpty()) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("The restore path is outside its declared root"));
  }
  return validateNoSymlinkPath(path, true);
}

[[nodiscard]] MutationResult verifyIdentity(
    const QString &path, const std::optional<FileIdentity> &expected) {
  if (!expected || !expected->valid()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("The Trash request has no identity precondition"));
  }
  const auto current = LocalMutationBackend::identityForPath(path);
  if (!current) {
    return failure(MutationError::Vanished, QStringLiteral("The Trash item vanished"));
  }
  return *current == *expected
      ? MutationResult{}
      : failure(MutationError::Changed, QStringLiteral("The Trash item changed"));
}

[[nodiscard]] QString boundedTrashBaseName(const QString &name) {
  QString result;
  for (const QChar character : name) {
    result.append(character);
    if (QFile::encodeName(result).size() > 180) {
      result.chop(1);
      break;
    }
  }
  return result.isEmpty() ? QStringLiteral("item") : result;
}

[[nodiscard]] bool cancelled(const MutationCancellation &token) {
  return token && token->load(std::memory_order_relaxed);
}

} // namespace

HomeTrash::HomeTrash(QString root, DeviceResolverPtr deviceResolver)
    : m_root(QDir::cleanPath(std::move(root))),
      m_deviceResolver(std::move(deviceResolver)) {}

MutationResult HomeTrash::trash(const MutationRequest &request) {
  if (const auto storage = validateTrashStorage(m_root, true); !storage.ok()) {
    return storage;
  }
  if (containsPath(m_root, request.sourcePath)) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("An item already in Trash cannot be trashed again"));
  }
  if (!std::any_of(request.declaredRoots.cbegin(), request.declaredRoots.cend(),
                   [&](const QString &root) { return containsPath(root, request.sourcePath); })) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("The item is outside the operation's declared root"));
  }
  if (const auto valid = validateDestination(request.sourcePath, request.declaredRoots);
      !valid.ok()) {
    return valid;
  }
  if (const auto identity = verifyIdentity(request.sourcePath, request.expectedSource);
      !identity.ok()) {
    return identity;
  }
  const QString infoRoot = QDir(m_root).filePath(QStringLiteral("info"));
  const QString filesRoot = QDir(m_root).filePath(QStringLiteral("files"));
  for (const QString &directory : {m_root, infoRoot, filesRoot}) {
    const MutationResult ensured = ensureLocalDirectoryNoFollow(directory, 0700);
    if (!ensured.ok()) {
      return ensured;
    }
  }
  if (const auto storage = validateTrashStorage(m_root, false); !storage.ok()) {
    return storage;
  }
  const auto sourceDevice = m_deviceResolver->deviceForPath(request.sourcePath);
  const auto trashDevice = m_deviceResolver->deviceForPath(m_root);
  if (!sourceDevice || !trashDevice) {
    return failure(MutationError::Vanished, QStringLiteral("Trash or the selected item vanished"));
  }
  if (*sourceDevice != *trashDevice) {
    return failure(MutationError::CrossDevice,
                   QStringLiteral("This item is on another filesystem; it was not deleted"));
  }

  const QString baseName = boundedTrashBaseName(
      QFileInfo(request.sourcePath).fileName());
  for (int suffix = 0; suffix < 10'000; ++suffix) {
    const QString token = suffix == 0 ? baseName : baseName + QStringLiteral(".%1").arg(suffix);
    const QString infoPath = QDir(infoRoot).filePath(token + QStringLiteral(".trashinfo"));
    const QByteArray encoded = QUrl::toPercentEncoding(
        QFileInfo(request.sourcePath).absoluteFilePath(), QByteArray("/"));
    const QByteArray metadata = QByteArray("[Trash Info]\nPath=") + encoded +
        QByteArray("\nDeletionDate=") +
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss")).toLatin1() +
        QByteArray("\n");
    const MutationResult metadataResult = writeExclusiveLocalFileNoFollow(
        infoPath, metadata, 0600);
    if (metadataResult.error == MutationError::AlreadyExists) {
      continue;
    }
    if (!metadataResult.ok()) {
      return metadataResult;
    }
    const QString payload = QDir(filesRoot).filePath(token);
    const auto filesIdentity = LocalMutationBackend::identityForPath(filesRoot);
    if (!filesIdentity) {
      const bool removedMetadata = removeLocalTreeNoFollow(infoPath);
      Q_UNUSED(removedMetadata);
      return failure(MutationError::Vanished,
                     QStringLiteral("The home Trash changed before the item was moved"));
    }
    const MutationResult moved = relocateLocalNoFollow(
        request.sourcePath, payload, *request.expectedSource, *filesIdentity);
    if (!moved.ok()) {
      const bool removedMetadata = removeLocalTreeNoFollow(infoPath);
      Q_UNUSED(removedMetadata);
      return moved;
    }
    MutationResult result;
    result.outputPath = payload;
    result.originalPath = QFileInfo(request.sourcePath).absoluteFilePath();
    result.trashToken = token;
    result.outputIdentity = LocalMutationBackend::identityForPath(payload);
    return result;
  }
  return failure(MutationError::AlreadyExists,
                 QStringLiteral("Trash could not allocate a unique item name"));
}

MutationResult HomeTrash::restore(const MutationRequest &request) {
  if (const auto storage = validateTrashStorage(m_root, false); !storage.ok()) {
    return storage;
  }
  if (request.trashToken.isEmpty() || request.trashToken.contains(QLatin1Char('/')) ||
      request.trashToken.contains(QLatin1Char('\\'))) {
    return failure(MutationError::InvalidRequest, QStringLiteral("Invalid Trash item identity"));
  }
  const QString infoPath = QDir(m_root).filePath(
      QStringLiteral("info/%1.trashinfo").arg(request.trashToken));
  const QString payload = QDir(m_root).filePath(
      QStringLiteral("files/%1").arg(request.trashToken));
  if (const auto identity = verifyIdentity(payload, request.expectedSource); !identity.ok()) {
    return identity;
  }
  const SafeFileReadResult infoFile = readLocalFileNoFollow(infoPath, 4096);
  if (!infoFile.result.ok()) {
    return infoFile.result;
  }
  const QList<QByteArray> lines = infoFile.contents.split('\n');
  if (lines.isEmpty() || lines.first() != QByteArray("[Trash Info]")) {
    return failure(MutationError::IoError, QStringLiteral("Trash metadata has an invalid header"));
  }
  QByteArray encodedPath;
  for (const QByteArray &line : lines) {
    if (encodedPath.isEmpty() && line.startsWith("Path=")) {
      encodedPath = line.mid(5);
    }
  }
  const QString originalPath = QUrl::fromPercentEncoding(encodedPath);
  if (!QFileInfo(originalPath).isAbsolute() || originalPath != request.destinationPath) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Trash metadata does not match the requested restore path"));
  }
  if (const auto valid = validateDestination(originalPath, request.declaredRoots); !valid.ok()) {
    return valid;
  }
  if (QFileInfo::exists(originalPath)) {
    return failure(MutationError::AlreadyExists,
                   QStringLiteral("The original location is no longer empty"));
  }
  if (!request.expectedParent || !request.expectedParent->valid()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Restore has no destination-parent identity"));
  }
  const auto payloadDevice = m_deviceResolver->deviceForPath(payload);
  const auto destinationDevice =
      m_deviceResolver->deviceForPath(QFileInfo(originalPath).absolutePath());
  if (!payloadDevice || !destinationDevice || *payloadDevice != *destinationDevice) {
    return failure(MutationError::CrossDevice,
                   QStringLiteral("Restore across filesystems is not supported"));
  }
  const MutationResult moved = relocateLocalNoFollow(
      payload, originalPath, *request.expectedSource, *request.expectedParent);
  if (!moved.ok()) {
    return moved;
  }
  if (!removeLocalTreeNoFollow(infoPath)) {
    return failure(MutationError::IoError,
                   QStringLiteral("The item was restored but its Trash metadata remains"));
  }
  MutationResult result;
  result.outputPath = originalPath;
  result.originalPath = originalPath;
  result.outputIdentity = LocalMutationBackend::identityForPath(originalPath);
  return result;
}

MutationResult HomeTrash::empty(const MutationCancellation &cancellation,
                                const MutationProgressCallback &progress) {
  if (cancelled(cancellation)) {
    return failure(MutationError::Cancelled, QStringLiteral("Empty Trash cancelled"));
  }
  if (const auto storage = validateTrashStorage(m_root, true); !storage.ok()) {
    return storage;
  }
  int removed = 0;
  for (const QString &childRoot : {QStringLiteral("files"), QStringLiteral("info")}) {
    const MutationResult emptied = emptyLocalDirectoryNoFollow(
        QDir(m_root).filePath(childRoot), cancellation, progress, &removed);
    if (!emptied.ok()) {
      return emptied;
    }
  }
  const bool removedSizes = removeLocalTreeNoFollow(
      QDir(m_root).filePath(QStringLiteral("directorysizes")));
  Q_UNUSED(removedSizes);
  MutationResult result;
  result.outputPath = m_root;
  return result;
}

} // namespace QindaQt::Apps::FileManager
