// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_catalog.h"
#include "safe_tree_access_p.h"
#include <QUuid>
#include <dirent.h>
#include <sys/file.h>
namespace QindaQt::Apps::FileManager {
using namespace SafeTreeAccess;
namespace {
bool stable(const struct stat &left, const struct stat &right) {
  return sameIdentity(left, right) && left.st_uid == right.st_uid &&
      left.st_gid == right.st_gid && left.st_nlink == right.st_nlink &&
      left.st_ctim.tv_sec == right.st_ctim.tv_sec && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}
RecoveryRecordRead readIndex(int parent, const QByteArray &name,
                             const struct stat *writtenVersion = nullptr) {
  UniqueFd fd(::openat(parent, name.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
  struct stat before {}, after {}, named {};
  if (!fd.valid() || ::fstat(fd.get(), &before) != 0 || !S_ISREG(before.st_mode) ||
      before.st_uid != ::getuid() || (before.st_mode & 07777) != 0600 || before.st_nlink != 1 ||
      before.st_size <= 0 || before.st_size > maximumRecoveryRecordBytes)
    return {failure(MutationError::InvalidRequest, QStringLiteral("Recovery index entry is unsafe; preserve it.")), {}};
  if (writtenVersion && !stable(*writtenVersion, before))
    return {failure(MutationError::Changed, QStringLiteral("Catalog reader is not the synced writer version.")), {}};
  QByteArray bytes(static_cast<qsizetype>(before.st_size), Qt::Uninitialized);
  qsizetype offset = 0;
  while (offset < bytes.size()) {
    const ssize_t count = ::read(fd.get(), bytes.data() + offset, static_cast<size_t>(bytes.size() - offset));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return {failure(MutationError::IoError, QStringLiteral("Recovery index read was incomplete.")), {}};
    offset += static_cast<qsizetype>(count);
  }
  if (::fstat(fd.get(), &after) != 0 || ::fstatat(parent, name.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
      !stable(before, after) || !stable(after, named))
    return {failure(MutationError::Changed, QStringLiteral("Recovery index changed during inspection.")), {}};
  return decodeRecoveryRecord(bytes);
}
}
RecoveryCatalog::RecoveryCatalog(QString path, RecoveryCatalogFault fault)
    : m_path(std::move(path)), m_fault(std::move(fault)) {}
RecoveryCatalog::~RecoveryCatalog() {
  if (m_locked && m_directory) ::flock(m_directory->descriptor(), LOCK_UN);
}
MutationResult RecoveryCatalog::admit(bool create) {
  MutationResult result;
  if (m_path.size() > 4096 || !m_path.startsWith(QLatin1Char('/')) || QDir::cleanPath(m_path) != m_path ||
      m_path.contains(QChar::Null) || QFile::encodeName(m_path).size() > 4096)
    return failure(MutationError::InvalidRequest, QStringLiteral("Recovery catalog path is invalid."));
  if (create) {
    // Walk from / through descriptors. Ancestors are never followed as links;
    // only newly created components receive private mode. The final catalog
    // must be owned/private even if a pre-existing ancestor is public.
    auto parent = openAbsoluteDirectory(QStringLiteral("/"));
    for (const QString &part : m_path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
      const auto name = QFile::encodeName(part);
      if (part == QStringLiteral(".") || part == QStringLiteral("..") || name.contains(0))
        return failure(MutationError::InvalidRequest, QStringLiteral("Recovery catalog path is invalid."));
      const bool created = ::mkdirat(parent.get(), name.constData(), 0700) == 0;
      if (!created && errno != EEXIST)
        return failure(errorForErrno(errno), QStringLiteral("Recovery catalog cannot be created."));
      UniqueFd next(::openat(parent.get(), name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
      if (!next.valid()) return failure(errorForErrno(errno), QStringLiteral("Recovery catalog ancestor is unsafe."));
      if (created && ::fsync(parent.get()) != 0) return failure(errorForErrno(errno), QStringLiteral("Recovery catalog creation durability failed."));
      parent = std::move(next);
    }
  }
  m_directory = RecoveryDirectoryAdmission::open(m_path, result);
  if (!m_directory) return result;
  result = m_directory->privateCurrent();
  if (!result.ok()) return result;
  if (::flock(m_directory->descriptor(), LOCK_EX | LOCK_NB) != 0)
    return failure(MutationError::Busy, QStringLiteral("Another recovery request owns the catalog."));
  m_locked = true;
  return {};
}
RecoveryCatalogRead RecoveryCatalog::inspect() const {
  if (!m_directory || !m_locked) return {failure(MutationError::InvalidRequest, QStringLiteral("Recovery catalog is not admitted.")), {}};
  const auto current = m_directory->privateCurrent();
  if (!current.ok()) return {current, {}};
  UniqueFd enumeration(::openat(m_directory->descriptor(), ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  const int owned = enumeration.valid() ? ::fcntl(enumeration.get(), F_DUPFD_CLOEXEC, 0) : -1;
  DIR *stream = owned >= 0 ? ::fdopendir(owned) : nullptr;
  if (!stream) { if (owned >= 0) ::close(owned); return {failure(MutationError::IoError, QStringLiteral("Recovery catalog cannot be enumerated.")), {}}; }
  RecoveryCatalogRead read;
  errno = 0;
  while (const auto *entry = ::readdir(stream)) {
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..") continue;
    if (read.records.size() >= maximumRecoveryOperations) { read.result = failure(MutationError::Unsupported, QStringLiteral("Recovery catalog reached its bound; no records evicted.")); break; }
    const auto record = readIndex(m_directory->descriptor(), name);
    if (!record.result.ok() || !record.record || record.record->phase != RecoveryPhase::Prepared ||
        name != record.record->operationId.toLatin1() + ".json") {
      read.result = record.result.ok() ? failure(MutationError::InvalidRequest, QStringLiteral("Recovery catalog contains an invalid locator.")) : record.result;
      break;
    }
    read.records.append(*record.record);
    errno = 0;
  }
  const int error = errno; ::closedir(stream);
  if (read.result.ok() && error != 0) read.result = failure(errorForErrno(error), QStringLiteral("Recovery catalog enumeration failed."));
  if (read.result.ok()) read.result = m_directory->privateCurrent();
  return read;
}
MutationResult RecoveryCatalog::add(const RecoveryRecord &record) {
  const auto existing = inspect();
  if (!existing.result.ok()) return existing.result;
  if (existing.records.size() >= maximumRecoveryOperations)
    return failure(MutationError::Unsupported, QStringLiteral("Recovery catalog is full; retained data is not evicted."));
  const auto bytes = encodeRecoveryRecord(record);
  if (bytes.isEmpty() || record.phase != RecoveryPhase::Prepared || record.sequence != 0)
    return failure(MutationError::InvalidRequest, QStringLiteral("Recovery locator is invalid."));
  const auto name = record.operationId.toLatin1() + ".json";
  UniqueFd fd(::openat(m_directory->descriptor(), name.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (!fd.valid()) return failure(errorForErrno(errno), QStringLiteral("Recovery locator collision or write failure."));
  qsizetype offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::write(fd.get(), bytes.constData() + offset, static_cast<size_t>(bytes.size() - offset));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return failure(errorForErrno(count < 0 ? errno : EIO), QStringLiteral("Incomplete recovery locator retained."));
    offset += static_cast<qsizetype>(count);
  }
  // AGENT-GUARD: keep the pre-fsync writer version through the separately
  // opened reader; equal bytes on another inode do not prove durability.
  struct stat syncedVersion {}, held {}, named {};
  if (::fstat(fd.get(), &syncedVersion) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Recovery locator writer version cannot be observed."));
  if (::fsync(fd.get()) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Recovery locator durability failed; source unchanged."));
  auto inject = [&](RecoveryCatalogStep step) {
    const auto error = m_fault ? m_fault(step) : MutationError::None;
    return error == MutationError::None ? MutationResult{} :
        failure(error, QStringLiteral("Catalog durability step refused; preserve all entries."));
  };
  auto result = inject(RecoveryCatalogStep::SyncDirectory);
  if (!result.ok()) return result;
  if (::fsync(m_directory->descriptor()) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Recovery catalog durability failed; source unchanged."));
  auto writerCurrent = [&]() {
    return ::fstat(fd.get(), &held) == 0 &&
        ::fstatat(m_directory->descriptor(), name.constData(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
        stable(syncedVersion, held) && stable(held, named);
  };
  if (!writerCurrent())
    return failure(MutationError::Changed, QStringLiteral("Recovery locator changed after synchronization."));
  result = inject(RecoveryCatalogStep::ReadRecord);
  if (!result.ok()) return result;
  const auto read = readIndex(m_directory->descriptor(), name, &syncedVersion);
  if (!read.result.ok()) return read.result;
  if (!read.record || *read.record != record || !writerCurrent())
    return failure(MutationError::Changed, QStringLiteral("Recovery locator readback changed."));
  return m_directory->privateCurrent();
}
RecoveryRecordRead RecoveryCatalog::lookup(const QString &id) const {
  const auto all = inspect();
  if (!all.result.ok()) return {all.result, {}};
  for (const auto &record : all.records) if (record.operationId == id) return {{}, record};
  return {failure(MutationError::Vanished, QStringLiteral("Recovery operation is not in this catalog.")), {}};
}
std::optional<RecoveryDirectoryAdmission> createRecoveryDirectory(
    RecoveryDirectoryAdmission &parent, const QString &name, MutationResult &result) {
  result = parent.current();
  if (!result.ok()) return {};
  const auto encoded = QFile::encodeName(name);
  if (name.isEmpty() || encoded.contains('/') || encoded.contains(0) || encoded == "." || encoded == "..") {
    result = failure(MutationError::InvalidRequest, QStringLiteral("Private recovery name is invalid.")); return {};
  }
  if (::mkdirat(parent.descriptor(), encoded.constData(), 0700) != 0) {
    result = failure(errorForErrno(errno), QStringLiteral("Private recovery location collision or failure.")); return {};
  }
  if (::fsync(parent.descriptor()) != 0) { result = failure(errorForErrno(errno), QStringLiteral("Private recovery creation is not durable.")); return {}; }
  auto child = RecoveryDirectoryAdmission::open(QDir(parent.path()).filePath(name), result);
  if (!child) return {};
  result = child->privateCurrent();
  if (result.ok()) result = parent.current();
  if (result.ok() && (child->observation().device != parent.observation().device ||
      child->observation().mountId != parent.observation().mountId))
    result = failure(MutationError::Changed, QStringLiteral("Private recovery location crossed a mount."));
  if (!result.ok()) return {};
  return child;
}
} // namespace QindaQt::Apps::FileManager
