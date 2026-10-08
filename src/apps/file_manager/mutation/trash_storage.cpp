// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_storage.h"
#include "safe_tree_access_p.h"
#include <QFileInfo>
#include <sys/stat.h>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;
MutationResult privateDirectory(const QString &path, bool create) {
  // No chmod repair: a pre-existing object belongs to its current owner,
  // regardless of who won mkdir/open. Existing ancestors are only traversal.
  auto current = openAbsoluteDirectory(QStringLiteral("/"));
  if (!current.valid()) return failure(errorForErrno(errno), QStringLiteral("Trash ancestry unavailable."));
  const auto parts = path.mid(1).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (parts.isEmpty()) return failure(MutationError::InvalidRequest, QStringLiteral("Filesystem root is not Trash."));
  for (const auto &part : parts) {
    const auto name = QFile::encodeName(part);
    UniqueFd next(::openat(current.get(), name.constData(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!next.valid() && errno == ENOENT && create) {
      if (::mkdirat(current.get(), name.constData(), 0700) != 0 && errno != EEXIST)
        return failure(errorForErrno(errno), QStringLiteral("Trash directory cannot be created."));
      if (::fsync(current.get()) != 0)
        return failure(errorForErrno(errno), QStringLiteral("Trash directory creation could not be synced."));
      next = UniqueFd(::openat(current.get(), name.constData(),
                              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    }
    if (!next.valid()) {
      const int openError = errno;
      struct stat entry {};
      if (::fstatat(current.get(), name.constData(), &entry, AT_SYMLINK_NOFOLLOW) == 0 &&
          S_ISLNK(entry.st_mode))
        return failure(MutationError::SymlinkEscape, QStringLiteral("Trash directory is a symbolic link; target retained."));
      return failure(openError == EROFS ? MutationError::PermissionDenied : errorForErrno(openError),
                     QStringLiteral("Trash directory cannot be opened without following links."));
    }
    current = std::move(next);
  }
  struct stat status {};
  if (::fstat(current.get(), &status) != 0 ||
      status.st_uid != ::getuid() || (status.st_mode & 07777) != 0700)
    return failure(MutationError::PermissionDenied, QStringLiteral("Trash requires an existing private owned directory; permissions were not changed."));
  return {};
}
bool sameVersion(const struct stat &a, const struct stat &b) {
  return sameIdentity(a, b) && a.st_uid == b.st_uid && a.st_gid == b.st_gid &&
      a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
bool writeAll(int fd, const QByteArray &bytes) {
  qsizetype done = 0;
  while (done < bytes.size()) {
    const auto count = ::write(fd, bytes.constData() + done,
                               static_cast<size_t>(bytes.size() - done));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    done += count;
  }
  return true;
}
MutationResult readDescriptor(int fd, int parent, const QByteArray &name,
                              QByteArray &destination, const struct stat *writer = nullptr) {
  struct stat before {}, after {}, named {};
  if (::fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) || before.st_nlink != 1 ||
      before.st_uid != ::getuid() || (before.st_mode & 0022) != 0 ||
      before.st_size < 0 || before.st_size > TrashMetadataCodec::maximumBytes)
    return failure(MutationError::InvalidRequest, QStringLiteral("Trash metadata is not a bounded private regular record."));
  if (writer && !sameVersion(before, *writer))
    return failure(MutationError::Changed, QStringLiteral("Trash metadata reader is not the synced writer."));
  QByteArray bytes(static_cast<qsizetype>(before.st_size), Qt::Uninitialized);
  qsizetype done = 0;
  while (done < bytes.size()) {
    const auto count = ::pread(fd, bytes.data() + done,
        static_cast<size_t>(bytes.size() - done), static_cast<off_t>(done));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return failure(MutationError::Changed, QStringLiteral("Trash metadata changed while being read."));
    done += count;
  }
  if (::fstat(fd, &after) != 0 ||
      ::fstatat(parent, name.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
      !sameVersion(before, after) || !sameVersion(before, named))
    return failure(MutationError::Changed, QStringLiteral("Trash metadata was rewritten or replaced."));
  destination = bytes;
  return {};
}
} // namespace

bool trashPathWithin(const QString &path, const QString &root) {
  return path == root || path.startsWith(root == QStringLiteral("/") ? root : root + QLatin1Char('/'));
}
bool validTrashToken(const QString &token) {
  return !token.isEmpty() && token != QStringLiteral(".") && token != QStringLiteral("..") &&
      !token.contains(QLatin1Char('/')) && !token.contains(QChar::Null) &&
      QFile::decodeName(QFile::encodeName(token)) == token && QFile::encodeName(token).size() <= 240;
}
QStringList volumeTrashRoots(const QString &topDirectory) {
  const auto uid = QString::number(::getuid());
  return {QDir(topDirectory).filePath(QStringLiteral(".Trash/") + uid),
          QDir(topDirectory).filePath(QStringLiteral(".Trash-") + uid)};
}
QString TrashTopDirectory::forPath(const QString &path, MutationResult &result) const {
  QString candidate = QFileInfo(path).absolutePath();
  auto admitted = RecoveryDirectoryAdmission::open(candidate, result);
  if (!admitted) return {};
  const auto mountId = admitted->observation().mountId;
  int ancestors = 0;
  while (candidate != QStringLiteral("/")) {
    if (++ancestors > 256) {
      result = failure(MutationError::Unsupported, QStringLiteral("Trash mount ancestry exceeds its bounded admission depth."));
      return {};
    }
    const auto parent = QFileInfo(candidate).absolutePath();
    auto next = RecoveryDirectoryAdmission::open(parent, result);
    if (!next) return {};
    if (next->observation().mountId != mountId) break;
    candidate = parent;
    admitted = std::move(next);
  }
  result = admitted->current();
  return result.ok() ? candidate : QString{};
}
TrashStorage::TrashStorage(TrashLocation location, RecoveryDirectoryAdmission root,
    RecoveryDirectoryAdmission info, RecoveryDirectoryAdmission files,
    std::optional<RecoveryDirectoryAdmission> top, std::optional<RecoveryDirectoryAdmission> shared,
    TrashControl control)
    : m_location(std::move(location)), m_control(std::move(control)), m_root(std::move(root)),
      m_info(std::move(info)), m_files(std::move(files)),
      m_top(std::move(top)), m_shared(std::move(shared)) {}

std::optional<TrashStorage> TrashStorage::open(
    const TrashLocation &location, bool create, MutationResult &result, TrashControl control) {
  if (!location.root.startsWith(QLatin1Char('/')) ||
      QDir::cleanPath(location.root) != location.root || location.root.contains(QChar::Null) ||
      QFile::decodeName(QFile::encodeName(location.root)) != location.root) {
    result = failure(MutationError::InvalidRequest, QStringLiteral("Trash storage path is invalid."));
    return std::nullopt;
  }
  std::optional<RecoveryDirectoryAdmission> top, shared;
  // Shared-directory policy is mandatory on reads as well as writes.
  if (!location.home) {
    if (!location.topDirectory.startsWith(QLatin1Char('/')) ||
        QDir::cleanPath(location.topDirectory) != location.topDirectory ||
        location.topDirectory.contains(QChar::Null) ||
        QFile::decodeName(QFile::encodeName(location.topDirectory)) != location.topDirectory) {
      result = failure(MutationError::InvalidRequest, QStringLiteral("Trash top directory is invalid."));
      return std::nullopt;
    }
    const auto roots = volumeTrashRoots(location.topDirectory);
    if (!roots.contains(location.root)) {
      result = failure(MutationError::InvalidRequest, QStringLiteral("Trash store is not a standard volume location."));
      return std::nullopt;
    }
    top = RecoveryDirectoryAdmission::open(location.topDirectory, result);
    if (!top) return std::nullopt;
    if (location.root == roots.front()) {
      shared = RecoveryDirectoryAdmission::open(
          QDir(location.topDirectory).filePath(QStringLiteral(".Trash")), result);
      if (!shared) return std::nullopt;
      if ((shared->observation().mode & S_ISVTX) == 0 ||
          shared->observation().mountId != top->observation().mountId) {
        result = failure(MutationError::PermissionDenied, QStringLiteral("Shared Trash is not a sticky directory on this volume."));
        return std::nullopt;
      }
    }
  }
  for (const auto &path : {location.root, QDir(location.root).filePath(QStringLiteral("info")),
                          QDir(location.root).filePath(QStringLiteral("files"))}) {
    result = privateDirectory(path, create);
    if (!result.ok()) return std::nullopt;
  }
  auto root = RecoveryDirectoryAdmission::open(location.root, result);
  if (!root) return std::nullopt;
  auto info = RecoveryDirectoryAdmission::open(QDir(location.root).filePath(QStringLiteral("info")), result);
  if (!info) return std::nullopt;
  auto files = RecoveryDirectoryAdmission::open(QDir(location.root).filePath(QStringLiteral("files")), result);
  if (!files) return std::nullopt;
  TrashStorage storage(location, std::move(*root), std::move(*info), std::move(*files),
                       std::move(top), std::move(shared), std::move(control));
  result = storage.current();
  return result.ok() ? std::optional<TrashStorage>(std::move(storage)) : std::nullopt;
}
MutationResult TrashStorage::current() const {
  for (const auto *entry : {&m_root, &m_info, &m_files}) {
    auto result = entry->privateCurrent();
    if (!result.ok()) return result;
    if (entry->observation().mountId != m_root.observation().mountId)
      return failure(MutationError::Changed, QStringLiteral("Trash storage crosses mount boundaries."));
  }
  for (const auto *entry : {&m_top, &m_shared}) {
    if (!*entry) continue;
    const auto result = (*entry)->current();
    if (!result.ok()) return result;
    if ((*entry)->observation().mountId != m_root.observation().mountId)
      return failure(MutationError::Changed, QStringLiteral("Trash top/shared directory changed."));
  }
  return {};
}
MutationResult TrashStorage::reserve(const QString &token, const QByteArray &bytes, std::optional<TrashRecord> &record, bool &created) const {
  created = false;
  if (!validTrashToken(token) || bytes.size() > TrashMetadataCodec::maximumBytes)
    return failure(MutationError::InvalidRequest, QStringLiteral("Trash record exceeds its bounds."));
  auto result = current();
  if (!result.ok()) return result;
  const auto name = QFile::encodeName(token + QStringLiteral(".trashinfo"));
  result = trashControl(m_control, TrashStep::OpenRecord);
  if (!result.ok()) return result;
  UniqueFd writer(::openat(m_info.descriptor(), name.constData(),
      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (!writer.valid()) return failure(errorForErrno(errno), QStringLiteral("Trash record could not be reserved."));
  created = true;
  struct stat baseline {}, synced {};
  // AGENT-GUARD: capture writer version BEFORE fsync and bind the reader to
  // it. Same-inode rewrite or same-bytes name replacement is not durability.
  result = trashControl(m_control, TrashStep::WriteRecord);
  if (!result.ok()) return result;
  if (!writeAll(writer.get(), bytes) || ::fstat(writer.get(), &baseline) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash write failed; reserved metadata was retained."));
  result = trashControl(m_control, TrashStep::SyncRecord);
  if (!result.ok()) return result;
  if (::fsync(writer.get()) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash writer sync failed; metadata retained."));
  result = trashControl(m_control, TrashStep::SyncInfo);
  if (!result.ok()) return result;
  if (::fsync(m_info.descriptor()) != 0 || ::fstat(writer.get(), &synced) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash record sync failed; reserved metadata was retained."));
  if (!sameVersion(baseline, synced))
    return failure(MutationError::Changed, QStringLiteral("Trash writer changed after sync; metadata was retained."));
  result = trashControl(m_control, TrashStep::ReadRecord);
  if (!result.ok()) return result;
  UniqueFd reader(::openat(m_info.descriptor(), name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (!reader.valid()) return failure(errorForErrno(errno), QStringLiteral("Trash record readback unavailable; metadata was retained."));
  QByteArray readback;
  result = readDescriptor(reader.get(), m_info.descriptor(), name, readback, &baseline);
  if (!result.ok()) return result;
  if (readback != bytes) return failure(MutationError::Changed, QStringLiteral("Trash record readback differs."));
  result = current();
  if (result.ok()) record.emplace(TrashRecord{std::move(reader), std::move(readback), baseline, name});
  return result;
}
MutationResult TrashStorage::read(const QString &token, std::optional<TrashRecord> &record) const {
  if (!validTrashToken(token)) return failure(MutationError::InvalidRequest, QStringLiteral("Trash token is invalid."));
  auto result = current();
  if (!result.ok()) return result;
  const auto name = QFile::encodeName(token + QStringLiteral(".trashinfo"));
  UniqueFd reader(::openat(m_info.descriptor(), name.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
  if (!reader.valid()) return failure(errorForErrno(errno), QStringLiteral("Trash record unavailable."));
  QByteArray readback;
  result = readDescriptor(reader.get(), m_info.descriptor(), name, readback);
  if (!result.ok()) return result;
  result = current();
  struct stat baseline {};
  if (result.ok() && ::fstat(reader.get(), &baseline) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash reader evidence unavailable."));
  if (result.ok()) {
    // Re-read against the captured baseline: no gap between bytes and the
    // version held for subsequent publication admission.
    QByteArray verified;
    result = readDescriptor(reader.get(), m_info.descriptor(), name, verified, &baseline);
    if (result.ok() && verified != readback)
      result = failure(MutationError::Changed, QStringLiteral("Trash metadata changed after observation."));
    if (result.ok()) record.emplace(TrashRecord{std::move(reader), std::move(readback), baseline, name});
  }
  return result;
}
MutationResult TrashStorage::recordCurrent(const TrashRecord &record) const {
  auto result = current();
  if (!result.ok()) return result;
  QByteArray bytes;
  result = readDescriptor(record.reader.get(), m_info.descriptor(), record.name, bytes, &record.baseline);
  if (result.ok() && bytes != record.bytes)
    return failure(MutationError::Changed, QStringLiteral("Trash record contents no longer match."));
  return result;
}
} // namespace QindaQt::Apps::FileManager
