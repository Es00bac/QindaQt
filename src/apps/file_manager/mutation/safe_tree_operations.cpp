// SPDX-License-Identifier: GPL-3.0-or-later
#include "safe_tree_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
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
    return MutationError::InvalidRequest;
  case ELOOP:
    return MutationError::SymlinkEscape;
  default:
    return MutationError::IoError;
  }
}

[[nodiscard]] bool cancelled(const MutationCancellation &token) {
  return token && token->load(std::memory_order_relaxed);
}

[[nodiscard]] qint64 modifiedNanoseconds(const struct stat &status) {
#if defined(Q_OS_LINUX)
  return static_cast<qint64>(status.st_mtim.tv_sec) * 1'000'000'000LL +
      status.st_mtim.tv_nsec;
#else
  return static_cast<qint64>(status.st_mtime) * 1'000'000'000LL;
#endif
}

[[nodiscard]] bool sameIdentity(const struct stat &left,
                                const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
      left.st_size == right.st_size && left.st_mode == right.st_mode &&
      modifiedNanoseconds(left) == modifiedNanoseconds(right);
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

void report(const MutationProgressCallback &progress, int complete,
            const QString &text) {
  if (progress) {
    progress({complete, 0, text});
  }
}

[[nodiscard]] MutationResult copyFileAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const struct stat &initial,
    const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied) {
  UniqueFd source(::openat(sourceParent, sourceName.constData(),
                           O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat opened {};
  if (!source.valid() || ::fstat(source.get(), &opened) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The source file could not be opened"));
  }
  if (!S_ISREG(opened.st_mode) || !sameIdentity(initial, opened)) {
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed before it could be copied"));
  }
  UniqueFd destination(::openat(
      destinationParent, destinationName.constData(),
      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
      opened.st_mode & 07777));
  if (!destination.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination file could not be created"));
  }

  std::array<char, 64 * 1024> buffer{};
  while (!cancelled(token)) {
    const ssize_t count = ::read(source.get(), buffer.data(), buffer.size());
    if (count == 0) {
      break;
    }
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return failure(errorForErrno(errno),
                     QStringLiteral("The source file could not be read"));
    }
    ssize_t offset = 0;
    while (offset < count) {
      const ssize_t written = ::write(destination.get(), buffer.data() + offset,
                                      static_cast<size_t>(count - offset));
      if (written < 0 && errno == EINTR) {
        continue;
      }
      if (written <= 0) {
        return failure(errorForErrno(errno),
                       QStringLiteral("The destination file could not be written"));
      }
      offset += written;
    }
    report(progress, *copied, QStringLiteral("Copying file data"));
  }
  if (cancelled(token)) {
    return failure(MutationError::Cancelled, QStringLiteral("Copy cancelled"));
  }
  const struct timespec times[2] = {opened.st_atim, opened.st_mtim};
  const int ignoredPermissions = ::fchmod(destination.get(), opened.st_mode & 07777);
  const int ignoredTimes = ::futimens(destination.get(), times);
  Q_UNUSED(ignoredPermissions);
  Q_UNUSED(ignoredTimes);
  if (::fsync(destination.get()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The copied file could not be committed"));
  }
  struct stat finalSource {};
  if (::fstat(source.get(), &finalSource) != 0 ||
      !sameIdentity(opened, finalSource)) {
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed while it was being copied"));
  }
  ++*copied;
  report(progress, *copied, QStringLiteral("Copied %1 items").arg(*copied));
  return {};
}

[[nodiscard]] MutationResult copyEntryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems);

[[nodiscard]] MutationResult copyDirectoryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const struct stat &initial,
    const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems) {
  UniqueFd source(::openat(sourceParent, sourceName.constData(),
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  struct stat opened {};
  if (!source.valid() || ::fstat(source.get(), &opened) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A source folder could not be opened"));
  }
  if (!S_ISDIR(opened.st_mode) || !sameIdentity(initial, opened)) {
    return failure(MutationError::Changed,
                   QStringLiteral("A source folder changed before it could be copied"));
  }
  if (::mkdirat(destinationParent, destinationName.constData(),
                opened.st_mode & 07777) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A destination folder could not be created"));
  }
  UniqueFd destination(::openat(destinationParent, destinationName.constData(),
                                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!destination.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A destination folder could not be opened"));
  }
  const int enumerationFd = ::dup(source.get());
  DIR *directory = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!directory) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return failure(errorForErrno(errno),
                   QStringLiteral("A source folder could not be enumerated"));
  }
  MutationResult result;
  errno = 0;
  while (const dirent *entry = ::readdir(directory)) {
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    result = copyEntryAt(source.get(), name, destination.get(), name, token,
                         progress, copied, maximumItems);
    if (!result.ok()) {
      break;
    }
    errno = 0;
  }
  const int enumerationError = errno;
  ::closedir(directory);
  if (result.ok() && enumerationError != 0) {
    result = failure(errorForErrno(enumerationError),
                     QStringLiteral("A source folder could not be enumerated"));
  }
  struct stat finalSource {};
  if (result.ok() && (::fstat(source.get(), &finalSource) != 0 ||
                      !sameIdentity(opened, finalSource))) {
    result = failure(MutationError::Changed,
                     QStringLiteral("A source folder changed while it was being copied"));
  }
  if (!result.ok()) {
    return result;
  }
  const struct timespec times[2] = {opened.st_atim, opened.st_mtim};
  const int ignoredPermissions = ::fchmod(destination.get(), opened.st_mode & 07777);
  const int ignoredTimes = ::futimens(destination.get(), times);
  Q_UNUSED(ignoredPermissions);
  Q_UNUSED(ignoredTimes);
  if (::fsync(destination.get()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A copied folder could not be committed"));
  }
  ++*copied;
  report(progress, *copied, QStringLiteral("Copied %1 items").arg(*copied));
  return {};
}

MutationResult copyEntryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems) {
  if (cancelled(token)) {
    return failure(MutationError::Cancelled, QStringLiteral("Copy cancelled"));
  }
  if (*copied >= maximumItems) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("The copy exceeds the item safety bound"));
  }
  struct stat status {};
  if (::fstatat(sourceParent, sourceName.constData(), &status,
                AT_SYMLINK_NOFOLLOW) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A source item vanished"));
  }
  if (S_ISLNK(status.st_mode)) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("A symbolic link was found inside the copy"));
  }
  if (S_ISREG(status.st_mode)) {
    return copyFileAt(sourceParent, sourceName, destinationParent,
                      destinationName, status, token, progress, copied);
  }
  if (S_ISDIR(status.st_mode)) {
    return copyDirectoryAt(sourceParent, sourceName, destinationParent,
                           destinationName, status, token, progress, copied,
                           maximumItems);
  }
  return failure(MutationError::Unsupported,
                 QStringLiteral("Only regular files and folders can be copied"));
}

[[nodiscard]] bool removeEntryAt(
    int parent, const QByteArray &name,
    const MutationCancellation &cancellation = {},
    const MutationProgressCallback &progress = {}, int *removed = nullptr) {
  if (cancelled(cancellation)) {
    return false;
  }
  struct stat status {};
  if (::fstatat(parent, name.constData(), &status, AT_SYMLINK_NOFOLLOW) != 0) {
    return errno == ENOENT;
  }
  if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
    if (::unlinkat(parent, name.constData(), 0) != 0) {
      return false;
    }
    if (removed) {
      ++*removed;
      report(progress, *removed,
             QStringLiteral("Permanently removed %1 Trash items").arg(*removed));
    }
    return true;
  }
  UniqueFd child(::openat(parent, name.constData(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!child.valid()) {
    return false;
  }
  const int enumerationFd = ::dup(child.get());
  DIR *directory = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!directory) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return false;
  }
  bool success = true;
  while (true) {
    errno = 0;
    const dirent *entry = ::readdir(directory);
    if (!entry) {
      success = success && errno == 0;
      break;
    }
    const QByteArray childName(entry->d_name);
    if (childName != "." && childName != ".." &&
        !removeEntryAt(child.get(), childName, cancellation, progress,
                       removed)) {
      success = false;
      break;
    }
  }
  ::closedir(directory);
  if (!success || ::unlinkat(parent, name.constData(), AT_REMOVEDIR) != 0) {
    return false;
  }
  if (removed) {
    ++*removed;
    report(progress, *removed,
           QStringLiteral("Permanently removed %1 Trash items").arg(*removed));
  }
  return true;
}

} // namespace

MutationResult copyLocalTreeNoFollow(
    const QString &source, const QString &destination,
    const FileIdentity &expectedDestinationParent,
    const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int maximumItems) {
  const QString sourceName = QFileInfo(source).fileName();
  const QString destinationName = QFileInfo(destination).fileName();
  if (sourceName.isEmpty() || destinationName.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Filesystem roots cannot be copied"));
  }
  UniqueFd sourceParent = openAbsoluteDirectory(QFileInfo(source).absolutePath());
  if (!sourceParent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The source parent could not be opened safely"));
  }
  UniqueFd destinationParent =
      openAbsoluteDirectory(QFileInfo(destination).absolutePath());
  if (!destinationParent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination parent could not be opened safely"));
  }
  struct stat destinationParentStatus {};
  if (::fstat(destinationParent.get(), &destinationParentStatus) != 0 ||
      identity(destinationParentStatus) != expectedDestinationParent) {
    return failure(MutationError::Changed,
                   QStringLiteral("The destination parent changed"));
  }
  int copied = 0;
  MutationResult result = copyEntryAt(
      sourceParent.get(), QFile::encodeName(sourceName), destinationParent.get(),
      QFile::encodeName(destinationName), cancellation, progress, &copied,
      maximumItems);
  if (!result.ok()) {
    const bool removed = removeEntryAt(destinationParent.get(),
                                       QFile::encodeName(destinationName));
    Q_UNUSED(removed);
    return result;
  }
  result.outputPath = destination;
  return result;
}

bool removeLocalTreeNoFollow(const QString &path) {
  const QString name = QFileInfo(path).fileName();
  if (name.isEmpty()) {
    return false;
  }
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(path).absolutePath());
  return parent.valid() && removeEntryAt(parent.get(), QFile::encodeName(name));
}

MutationResult emptyLocalDirectoryNoFollow(
    const QString &path, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int *removed) {
  if (!removed) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Empty Trash requires a progress counter"));
  }
  UniqueFd directory = openAbsoluteDirectory(path);
  if (!directory.valid()) {
    return errno == ENOENT
        ? MutationResult{}
        : failure(errorForErrno(errno),
                  QStringLiteral("A Trash directory could not be opened safely"));
  }
  const int enumerationFd = ::dup(directory.get());
  DIR *entries = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!entries) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return failure(errorForErrno(errno),
                   QStringLiteral("A Trash directory could not be enumerated"));
  }
  MutationResult result;
  while (true) {
    errno = 0;
    const dirent *entry = ::readdir(entries);
    if (!entry) {
      if (errno != 0) {
        result = failure(errorForErrno(errno),
                         QStringLiteral("A Trash directory could not be enumerated"));
      }
      break;
    }
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    if (!removeEntryAt(directory.get(), name, cancellation, progress, removed)) {
      result = cancelled(cancellation)
          ? failure(MutationError::Cancelled, QStringLiteral("Empty Trash cancelled"))
          : failure(MutationError::PermissionDenied,
                    QStringLiteral("A Trash item could not be permanently removed"));
      break;
    }
  }
  ::closedir(entries);
  return result;
}

} // namespace QindaQt::Apps::FileManager
