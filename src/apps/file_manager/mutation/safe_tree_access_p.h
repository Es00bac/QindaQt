// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

// Private descriptor helpers shared by copying and the unchanged explicit
// delete/Trash traversal. No pathname or observation is deletion authority.
namespace QindaQt::Apps::FileManager::SafeTreeAccess {
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
  [[nodiscard]] inline int get() const { return m_descriptor; }
  [[nodiscard]] inline bool valid() const { return m_descriptor >= 0; }

private:
  int m_descriptor = -1;
};

[[nodiscard]] inline MutationResult failure(MutationError error,
                                     const QString &message) {
  MutationResult result;
  result.error = error;
  result.diagnostic = boundedMutationDiagnostic(message);
  return result;
}

[[nodiscard]] inline MutationError errorForErrno(int error) {
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

[[nodiscard]] inline bool cancelled(const MutationCancellation &token) {
  return token && token->load(std::memory_order_relaxed);
}

[[nodiscard]] inline qint64 modifiedNanoseconds(const struct stat &status) {
#if defined(Q_OS_LINUX)
  return static_cast<qint64>(status.st_mtim.tv_sec) * 1'000'000'000LL +
      status.st_mtim.tv_nsec;
#else
  return static_cast<qint64>(status.st_mtime) * 1'000'000'000LL;
#endif
}

[[nodiscard]] inline bool sameIdentity(const struct stat &left,
                                const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
      left.st_size == right.st_size && left.st_mode == right.st_mode &&
      modifiedNanoseconds(left) == modifiedNanoseconds(right);
}

[[nodiscard]] inline FileIdentity identity(const struct stat &status) {
  return {static_cast<quint64>(status.st_dev),
          static_cast<quint64>(status.st_ino),
          static_cast<qint64>(status.st_size), modifiedNanoseconds(status),
          static_cast<quint32>(status.st_mode)};
}

[[nodiscard]] inline UniqueFd openAbsoluteDirectory(const QString &path) {
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

inline void report(const MutationProgressCallback &progress, int complete,
            const QString &text) {
  if (progress) {
    progress({complete, 0, text});
  }
}

} // namespace QindaQt::Apps::FileManager::SafeTreeAccess
