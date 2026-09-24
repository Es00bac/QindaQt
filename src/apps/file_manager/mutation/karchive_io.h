// SPDX-License-Identifier: GPL-3.0-or-later
// Internal to the KArchive codec (karchive_codec.cpp and
// karchive_codec_extract.cpp); nothing else includes this header.
#pragma once

#include "mutation_types.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::Apps::FileManager::KArchiveIo {

// AGENT-NOTE: the mutation module keeps its descriptor helpers private to each
// file (safe_path_operations.cpp, safe_tree_operations.cpp); these are the
// same small helpers, shared only by the KArchive codec's two sources.
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
  UniqueFd(UniqueFd &&other) noexcept : m_descriptor(std::exchange(other.m_descriptor, -1)) {}
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

inline constexpr int directoryFlags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW;

[[nodiscard]] inline MutationResult failure(MutationError error, const QString &message) {
  MutationResult result;
  result.error = error;
  result.diagnostic = boundedMutationDiagnostic(message);
  return result;
}

[[nodiscard]] inline bool cancelled(const MutationCancellation &token) {
  return token && token->load(std::memory_order_relaxed);
}

[[nodiscard]] inline FileIdentity identityOf(const struct stat &status) {
  return {static_cast<quint64>(status.st_dev), static_cast<quint64>(status.st_ino),
          static_cast<qint64>(status.st_size),
          static_cast<qint64>(status.st_mtim.tv_sec) * 1'000'000'000LL + status.st_mtim.tv_nsec,
          static_cast<quint32>(status.st_mode)};
}

[[nodiscard]] inline QDateTime modifiedTime(const struct stat &status) {
  return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(status.st_mtim.tv_sec));
}

[[nodiscard]] inline UniqueFd openAbsoluteDirectory(const QString &path) {
  if (!QFileInfo(path).isAbsolute()) {
    errno = EINVAL;
    return {};
  }
  UniqueFd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  const QStringList parts = QDir::cleanPath(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    if (!current.valid()) {
      break;
    }
    current = UniqueFd(::openat(current.get(), QFile::encodeName(part).constData(), directoryFlags));
  }
  return current;
}

[[nodiscard]] inline bool writeAll(int descriptor, const char *data, qint64 size) {
  qint64 offset = 0;
  while (offset < size) {
    const ssize_t count = ::write(descriptor, data + offset, static_cast<size_t>(size - offset));
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

} // namespace QindaQt::Apps::FileManager::KArchiveIo
