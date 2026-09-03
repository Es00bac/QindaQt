// SPDX-License-Identifier: GPL-3.0-or-later
#include "safe_path_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

class UniqueFd final {
public:
  UniqueFd() = default;
  explicit UniqueFd(int descriptor) : m_descriptor(descriptor) {}
  ~UniqueFd() {
    if (m_descriptor >= 0) {
      ::close(m_descriptor);
    }
  }
  UniqueFd(const UniqueFd &) = delete;
  UniqueFd &operator=(const UniqueFd &) = delete;
  UniqueFd(UniqueFd &&other) noexcept
      : m_descriptor(std::exchange(other.m_descriptor, -1)) {}
  UniqueFd &operator=(UniqueFd &&other) noexcept {
    if (this != &other) {
      if (m_descriptor >= 0) {
        ::close(m_descriptor);
      }
      m_descriptor = std::exchange(other.m_descriptor, -1);
    }
    return *this;
  }
  [[nodiscard]] int get() const { return m_descriptor; }
  [[nodiscard]] bool valid() const { return m_descriptor >= 0; }

private:
  int m_descriptor = -1;
};

[[nodiscard]] MutationResult failure(MutationError error,
                                     const QString &message) {
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
  case EINVAL:
    return MutationError::InvalidRequest;
  case ELOOP:
    return MutationError::SymlinkEscape;
#if defined(ENOTSUP)
  case ENOTSUP:
#endif
  case ENOSYS:
    return MutationError::Unsupported;
  default:
    return MutationError::IoError;
  }
}

[[nodiscard]] qint64 modifiedNanoseconds(const struct stat &status) {
#if defined(Q_OS_LINUX)
  return static_cast<qint64>(status.st_mtim.tv_sec) * 1'000'000'000LL +
      status.st_mtim.tv_nsec;
#else
  return static_cast<qint64>(status.st_mtime) * 1'000'000'000LL;
#endif
}

[[nodiscard]] FileIdentity identity(const struct stat &status) {
  return {static_cast<quint64>(status.st_dev),
          static_cast<quint64>(status.st_ino),
          static_cast<qint64>(status.st_size), modifiedNanoseconds(status),
          static_cast<quint32>(status.st_mode)};
}

[[nodiscard]] UniqueFd openAbsoluteDirectory(const QString &path) {
  if (!QFileInfo(path).isAbsolute()) {
    errno = EINVAL;
    return {};
  }
  UniqueFd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!current.valid()) {
    return {};
  }
  const QStringList parts = QDir::cleanPath(path).split(
      QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    const QByteArray encoded = QFile::encodeName(part);
    UniqueFd next(::openat(current.get(), encoded.constData(),
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!next.valid()) {
      return {};
    }
    current = std::move(next);
  }
  return current;
}

[[nodiscard]] bool sameIdentity(const struct stat &left,
                                const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
      left.st_size == right.st_size && left.st_mode == right.st_mode &&
      modifiedNanoseconds(left) == modifiedNanoseconds(right);
}

[[nodiscard]] bool writeAll(int descriptor, const QByteArray &contents) {
  qsizetype offset = 0;
  while (offset < contents.size()) {
    const ssize_t count = ::write(
        descriptor, contents.constData() + offset,
        static_cast<size_t>(contents.size() - offset));
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    offset += count;
  }
  return true;
}

[[nodiscard]] int renameNoReplace(int sourceParent, const char *sourceName,
                                  int destinationParent,
                                  const char *destinationName) {
#if defined(Q_OS_LINUX) && defined(SYS_renameat2)
  // AGENT-GUARD: P2-2 showed that an existence check followed by renameat()
  // can erase a destination created by a racing writer. RENAME_NOREPLACE is
  // the commit-time authority; the earlier fstatat remains diagnostic only.
  return static_cast<int>(::syscall(SYS_renameat2, sourceParent, sourceName,
                                    destinationParent, destinationName,
                                    RENAME_NOREPLACE));
#else
  Q_UNUSED(sourceParent);
  Q_UNUSED(sourceName);
  Q_UNUSED(destinationParent);
  Q_UNUSED(destinationName);
  errno = ENOSYS;
  return -1;
#endif
}

} // namespace

MutationResult ensureLocalDirectoryNoFollow(const QString &path,
                                            quint32 permissions) {
  if (!QFileInfo(path).isAbsolute()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Trash storage requires an absolute path"));
  }
  UniqueFd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!current.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The filesystem root could not be opened"));
  }
  const QStringList parts = QDir::cleanPath(path).split(
      QLatin1Char('/'), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("The filesystem root cannot be Trash"));
  }
  for (const QString &part : parts) {
    const QByteArray encoded = QFile::encodeName(part);
    UniqueFd next(::openat(current.get(), encoded.constData(),
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!next.valid() && errno == ENOENT) {
      if (::mkdirat(current.get(), encoded.constData(),
                    static_cast<mode_t>(permissions)) != 0 &&
          errno != EEXIST) {
        return failure(errorForErrno(errno),
                       QStringLiteral("A Trash directory could not be created"));
      }
      next = UniqueFd(::openat(current.get(), encoded.constData(),
                               O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    }
    if (!next.valid()) {
      return failure(errorForErrno(errno),
                     QStringLiteral("A Trash directory could not be opened safely"));
    }
    current = std::move(next);
  }
  if (::fchmod(current.get(), static_cast<mode_t>(permissions)) != 0 ||
      ::fsync(current.get()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("Trash directory permissions could not be committed"));
  }
  return {};
}

MutationResult writeExclusiveLocalFileNoFollow(
    const QString &path, const QByteArray &contents, quint32 permissions) {
  const QString name = QFileInfo(path).fileName();
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(path).absolutePath());
  if (name.isEmpty() || !parent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The metadata parent could not be opened safely"));
  }
  const QByteArray encoded = QFile::encodeName(name);
  UniqueFd output(::openat(parent.get(), encoded.constData(),
                           O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                           static_cast<mode_t>(permissions)));
  if (!output.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("Trash metadata could not be created"));
  }
  if (!writeAll(output.get(), contents) || ::fsync(output.get()) != 0 ||
      ::fsync(parent.get()) != 0) {
    const int commitError = errno;
    const int ignoredRemoval = ::unlinkat(parent.get(), encoded.constData(), 0);
    Q_UNUSED(ignoredRemoval);
    return failure(errorForErrno(commitError),
                   QStringLiteral("Trash metadata could not be committed"));
  }
  MutationResult result;
  result.outputPath = path;
  return result;
}

SafeFileReadResult readLocalFileNoFollow(const QString &path,
                                         qsizetype maximumBytes) {
  SafeFileReadResult result;
  const QString name = QFileInfo(path).fileName();
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(path).absolutePath());
  UniqueFd input(parent.valid()
                     ? ::openat(parent.get(), QFile::encodeName(name).constData(),
                                O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
                     : -1);
  struct stat initial {};
  if (name.isEmpty() || !input.valid() || ::fstat(input.get(), &initial) != 0) {
    result.result = failure(errorForErrno(errno),
                            QStringLiteral("Trash metadata could not be opened safely"));
    return result;
  }
  if (!S_ISREG(initial.st_mode) || initial.st_size < 0 ||
      initial.st_size > maximumBytes) {
    result.result = failure(MutationError::IoError,
                            QStringLiteral("Trash metadata is not a bounded regular file"));
    return result;
  }
  result.contents.resize(static_cast<qsizetype>(initial.st_size));
  qsizetype offset = 0;
  while (offset < result.contents.size()) {
    const ssize_t count = ::read(
        input.get(), result.contents.data() + offset,
        static_cast<size_t>(result.contents.size() - offset));
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      result.result = failure(MutationError::IoError,
                              QStringLiteral("Trash metadata could not be read"));
      return result;
    }
    offset += count;
  }
  struct stat finalStatus {};
  if (::fstat(input.get(), &finalStatus) != 0 ||
      !sameIdentity(initial, finalStatus)) {
    result.result = failure(MutationError::Changed,
                            QStringLiteral("Trash metadata changed while it was read"));
  }
  return result;
}

MutationResult createLocalDirectoryNoFollow(
    const QString &destination, const FileIdentity &expectedParent) {
  const QString name = QFileInfo(destination).fileName();
  if (name.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("A filesystem root cannot be created"));
  }
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(destination).absolutePath());
  struct stat parentStatus {};
  if (!parent.valid() || ::fstat(parent.get(), &parentStatus) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination parent could not be opened safely"));
  }
  if (identity(parentStatus) != expectedParent) {
    return failure(MutationError::Changed,
                   QStringLiteral("The destination parent changed"));
  }
  const QByteArray encoded = QFile::encodeName(name);
  struct stat existing {};
  if (::fstatat(parent.get(), encoded.constData(), &existing,
                AT_SYMLINK_NOFOLLOW) == 0) {
    return failure(MutationError::AlreadyExists,
                   QStringLiteral("An item already has that name"));
  }
  if (errno != ENOENT) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination could not be checked"));
  }
  if (::mkdirat(parent.get(), encoded.constData(), 0777) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The folder could not be created"));
  }
  MutationResult result;
  result.outputPath = destination;
  return result;
}

MutationResult relocateLocalNoFollow(const QString &source,
                                     const QString &destination,
                                     const FileIdentity &expectedSource,
                                     const FileIdentity &expectedDestinationParent) {
  const QString sourceName = QFileInfo(source).fileName();
  const QString destinationName = QFileInfo(destination).fileName();
  if (sourceName.isEmpty() || destinationName.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("A filesystem root cannot be moved"));
  }
  UniqueFd sourceParent = openAbsoluteDirectory(QFileInfo(source).absolutePath());
  UniqueFd destinationParent =
      openAbsoluteDirectory(QFileInfo(destination).absolutePath());
  if (!sourceParent.valid() || !destinationParent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A move parent could not be opened safely"));
  }
  struct stat destinationParentStatus {};
  if (::fstat(destinationParent.get(), &destinationParentStatus) != 0 ||
      identity(destinationParentStatus) != expectedDestinationParent) {
    return failure(MutationError::Changed,
                   QStringLiteral("The destination parent changed"));
  }
  const QByteArray sourceBytes = QFile::encodeName(sourceName);
  const QByteArray destinationBytes = QFile::encodeName(destinationName);
  struct stat sourceStatus {};
  if (::fstatat(sourceParent.get(), sourceBytes.constData(), &sourceStatus,
                AT_SYMLINK_NOFOLLOW) != 0) {
    return failure(errorForErrno(errno), QStringLiteral("The source vanished"));
  }
  if (S_ISLNK(sourceStatus.st_mode)) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("Symbolic links are not moved"));
  }
  if (identity(sourceStatus) != expectedSource) {
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed before it could be moved"));
  }
  struct stat existing {};
  if (::fstatat(destinationParent.get(), destinationBytes.constData(), &existing,
                AT_SYMLINK_NOFOLLOW) == 0) {
    return failure(MutationError::AlreadyExists,
                   QStringLiteral("The destination already exists"));
  }
  if (errno != ENOENT) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination could not be checked"));
  }
  if (renameNoReplace(sourceParent.get(), sourceBytes.constData(),
                      destinationParent.get(),
                      destinationBytes.constData()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The item could not be moved"));
  }
  struct stat movedStatus {};
  if (::fstatat(destinationParent.get(), destinationBytes.constData(),
                &movedStatus, AT_SYMLINK_NOFOLLOW) != 0 ||
      identity(movedStatus) != expectedSource) {
    const int ignoredRollback = renameNoReplace(
        destinationParent.get(), destinationBytes.constData(),
        sourceParent.get(), sourceBytes.constData());
    Q_UNUSED(ignoredRollback);
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed while it was being moved"));
  }
  MutationResult result;
  result.outputPath = destination;
  return result;
}

} // namespace QindaQt::Apps::FileManager
