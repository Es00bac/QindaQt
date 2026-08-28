// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_journal/file_journal_store.h>

#include <qindaqt/services/display_transaction/transaction_journal.h>

#include <QtCore/QByteArray>
#include <QtCore/QFile>

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::DisplayJournal {
namespace {

constexpr char kJournalName[] = "display1-transaction.journal";
constexpr char kTemporaryName[] = ".display1-transaction.journal.tmp";

class ScopedFd {
public:
  ScopedFd() = default;
  explicit ScopedFd(const int fd) noexcept : m_fd(fd) {}
  ~ScopedFd() {
    if (m_fd >= 0) {
      static_cast<void>(::close(m_fd));
    }
  }
  ScopedFd(const ScopedFd &) = delete;
  ScopedFd &operator=(const ScopedFd &) = delete;
  ScopedFd(ScopedFd &&other) noexcept : m_fd(other.release()) {}
  ScopedFd &operator=(ScopedFd &&other) noexcept {
    if (this != &other) {
      if (m_fd >= 0) {
        static_cast<void>(::close(m_fd));
      }
      m_fd = other.release();
    }
    return *this;
  }
  [[nodiscard]] int get() const noexcept { return m_fd; }
  [[nodiscard]] int release() noexcept {
    const int value = m_fd;
    m_fd = -1;
    return value;
  }

private:
  int m_fd = -1;
};

bool retryClose(const int fd) {
  // Linux closes the descriptor even when close reports EINTR. Retrying can
  // close an unrelated descriptor reused by another thread.
  return ::close(fd) == 0;
}

ScopedFd openRoot(const QString &root) {
  if (root.isEmpty() || root.contains(QChar::Null)) {
    return {};
  }
  const QByteArray path = QFile::encodeName(root);
  const int fd =
      ::open(path.constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
  if (fd < 0) {
    return {};
  }
  struct stat metadata{};
  if (::fstat(fd, &metadata) != 0 || !S_ISDIR(metadata.st_mode) ||
      metadata.st_uid != ::geteuid() || (metadata.st_mode & 0022) != 0) {
    static_cast<void>(::close(fd));
    return {};
  }
  return ScopedFd(fd);
}

bool safeExistingJournal(const int rootFd, struct stat *metadata = nullptr) {
  struct stat value{};
  if (::fstatat(rootFd, kJournalName, &value, AT_SYMLINK_NOFOLLOW) != 0) {
    return errno == ENOENT;
  }
  if (!S_ISREG(value.st_mode) || value.st_uid != ::geteuid() ||
      value.st_nlink != 1 || (value.st_mode & 0077) != 0) {
    return false;
  }
  if (metadata != nullptr) {
    *metadata = value;
  }
  return true;
}

bool removeStaleTemporary(const int rootFd) {
  struct stat metadata{};
  if (::fstatat(rootFd, kTemporaryName, &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
    return errno == ENOENT;
  }
  // unlinkat without AT_REMOVEDIR removes a stale regular file or symlink by
  // name and never follows it. A directory at the private temp name is a
  // hostile/nonregular collision and must fail closed.
  return !S_ISDIR(metadata.st_mode) &&
         ::unlinkat(rootFd, kTemporaryName, 0) == 0;
}

bool writeAll(const int fd, const QByteArray &payload) {
  qsizetype offset = 0;
  while (offset < payload.size()) {
    const qsizetype remaining = payload.size() - offset;
    const auto count = static_cast<size_t>(remaining);
    const ssize_t written = ::write(fd, payload.constData() + offset, count);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    offset += static_cast<qsizetype>(written);
  }
  return true;
}

bool syncDirectory(const int rootFd) {
  if (::fsync(rootFd) == 0) {
    return true;
  }
  // Some otherwise atomic filesystems do not implement directory fsync.
  // The caller still gets same-directory atomic replacement, but supported
  // Linux filesystems must acknowledge the metadata durability barrier.
  return errno == EINVAL || errno == ENOTSUP || errno == EOPNOTSUPP;
}

LoadResult rejected(const char *reason) {
  return {.status = LoadStatus::Rejected,
          .journal = {},
          .reasonCode = QString::fromLatin1(reason)};
}

} // namespace

FileJournalStore::FileJournalStore(QString userStateRoot)
    : m_userStateRoot(std::move(userStateRoot)) {}

LoadResult FileJournalStore::load() const {
  const ScopedFd root = openRoot(m_userStateRoot);
  if (root.get() < 0) {
    return rejected("invalid-state-root");
  }

  struct stat pathMetadata{};
  if (::fstatat(root.get(), kJournalName, &pathMetadata, AT_SYMLINK_NOFOLLOW) !=
      0) {
    if (errno == ENOENT) {
      return {.status = LoadStatus::Absent,
              .journal = {},
              .reasonCode = QStringLiteral("journal-absent")};
    }
    return rejected("journal-stat-failed");
  }
  if (!S_ISREG(pathMetadata.st_mode) || pathMetadata.st_uid != ::geteuid() ||
      pathMetadata.st_nlink != 1 || (pathMetadata.st_mode & 0077) != 0) {
    return rejected("unsafe-journal-file");
  }
  if (pathMetadata.st_size < 0 ||
      static_cast<quint64>(pathMetadata.st_size) >
          static_cast<quint64>(DisplayTransaction::kMaximumJournalBytes)) {
    return rejected("journal-too-large");
  }

  ScopedFd file(::openat(root.get(), kJournalName,
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0) {
    return rejected("journal-open-failed");
  }
  struct stat openedMetadata{};
  if (::fstat(file.get(), &openedMetadata) != 0 ||
      !S_ISREG(openedMetadata.st_mode) ||
      openedMetadata.st_dev != pathMetadata.st_dev ||
      openedMetadata.st_ino != pathMetadata.st_ino ||
      openedMetadata.st_uid != ::geteuid() || openedMetadata.st_nlink != 1 ||
      (openedMetadata.st_mode & 0077) != 0) {
    return rejected("journal-replaced-during-open");
  }

  QByteArray payload;
  payload.reserve(static_cast<qsizetype>(openedMetadata.st_size));
  std::array<char, 16 * 1024> chunk{};
  while (true) {
    const ssize_t count = ::read(file.get(), chunk.data(), chunk.size());
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return rejected("journal-read-failed");
    }
    if (count == 0) {
      break;
    }
    if (payload.size() + count > DisplayTransaction::kMaximumJournalBytes) {
      return rejected("journal-too-large");
    }
    payload.append(chunk.data(), static_cast<qsizetype>(count));
  }

  DisplayTransaction::Journal journal;
  const DisplayTransaction::JournalDecodeResult decoded =
      DisplayTransaction::decodeJournal(payload, journal);
  if (!decoded.succeeded()) {
    return {.status = LoadStatus::Rejected,
            .journal = {},
            .reasonCode = decoded.reasonCode};
  }
  const DisplayTransaction::JournalEncodeResult canonical =
      DisplayTransaction::encodeJournal(journal);
  if (!canonical.succeeded() || canonical.payload != payload) {
    return rejected("non-canonical-journal");
  }
  return {.status = LoadStatus::Loaded,
          .journal = std::move(journal),
          .reasonCode = {}};
}

bool FileJournalStore::store(const DisplayTransaction::Journal &journal) {
  const DisplayTransaction::JournalEncodeResult encoded =
      DisplayTransaction::encodeJournal(journal);
  if (!encoded.succeeded()) {
    return false;
  }
  const ScopedFd root = openRoot(m_userStateRoot);
  if (root.get() < 0 || !safeExistingJournal(root.get()) ||
      !removeStaleTemporary(root.get())) {
    return false;
  }

  const int rawTemporary = ::openat(
      root.get(), kTemporaryName,
      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
  if (rawTemporary < 0) {
    return false;
  }
  ScopedFd temporary(rawTemporary);
  bool prepared = ::fchmod(temporary.get(), S_IRUSR | S_IWUSR) == 0 &&
                  writeAll(temporary.get(), encoded.payload) &&
                  ::fsync(temporary.get()) == 0;
  if (!prepared || !retryClose(temporary.release())) {
    static_cast<void>(::unlinkat(root.get(), kTemporaryName, 0));
    return false;
  }

  // AGENT-GUARD: The atomic replacement is the commit point. Every failure
  // before it removes only our same-directory temp and preserves the prior
  // journal. Never substitute QFile::remove+rename, which creates an absent
  // window and defeats restart recovery.
  if (!safeExistingJournal(root.get()) ||
      ::renameat(root.get(), kTemporaryName, root.get(), kJournalName) != 0) {
    static_cast<void>(::unlinkat(root.get(), kTemporaryName, 0));
    return false;
  }
  return syncDirectory(root.get());
}

bool FileJournalStore::clear() {
  const ScopedFd root = openRoot(m_userStateRoot);
  if (root.get() < 0) {
    return false;
  }
  struct stat metadata{};
  if (::fstatat(root.get(), kJournalName, &metadata, AT_SYMLINK_NOFOLLOW) !=
      0) {
    return errno == ENOENT;
  }
  if (!S_ISREG(metadata.st_mode) || metadata.st_uid != ::geteuid() ||
      metadata.st_nlink != 1 || (metadata.st_mode & 0077) != 0) {
    return false;
  }
  if (::unlinkat(root.get(), kJournalName, 0) != 0) {
    return false;
  }
  return syncDirectory(root.get());
}

const QString &FileJournalStore::userStateRoot() const noexcept {
  return m_userStateRoot;
}

} // namespace QindaQt::DisplayJournal
