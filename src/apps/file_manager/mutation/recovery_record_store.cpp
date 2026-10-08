// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_record_store.h"
#include "safe_tree_access_p.h"

#include <dirent.h>
#include <set>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;

QByteArray slotName(quint32 sequence) {
  return QStringLiteral("record-%1.json").arg(sequence, 2, 10, QLatin1Char('0')).toLatin1();
}

bool unchangedFile(const struct stat &before, const struct stat &after) {
  return sameIdentity(before, after) && before.st_uid == after.st_uid &&
      before.st_nlink == after.st_nlink &&
      before.st_ctim.tv_sec == after.st_ctim.tv_sec &&
      before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
}

RecoveryRecordRead readSlot(int parent, quint32 sequence,
                            const struct stat *writtenVersion = nullptr) {
  const auto name = slotName(sequence);
  UniqueFd file(::openat(parent, name.constData(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
  if (!file.valid())
    return {failure(errorForErrno(errno), QStringLiteral("Recovery record cannot be opened safely.")), {}};
  struct stat before {};
  if (::fstat(file.get(), &before) != 0 || !S_ISREG(before.st_mode) ||
      (before.st_mode & 07777) != 0600 || before.st_uid != ::getuid() ||
      before.st_nlink != 1 || before.st_size <= 0 ||
      before.st_size > maximumRecoveryRecordBytes)
    return {failure(MutationError::InvalidRequest, QStringLiteral("Recovery record storage is unsafe or incomplete.")), {}};
  if (writtenVersion && !unchangedFile(*writtenVersion, before))
    return {failure(MutationError::Changed, QStringLiteral("Recovery readback is not the synced record version.")), {}};
  QByteArray bytes;
  bytes.resize(static_cast<qsizetype>(before.st_size));
  qsizetype offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::read(file.get(), bytes.data() + offset,
                              static_cast<size_t>(bytes.size() - offset));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0)
      return {failure(MutationError::IoError, QStringLiteral("Recovery record read was incomplete.")), {}};
    offset += static_cast<qsizetype>(count);
  }
  struct stat after {};
  struct stat named {};
  if (::fstat(file.get(), &after) != 0 ||
      ::fstatat(parent, name.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
      !unchangedFile(before, after) || !unchangedFile(after, named))
    return {failure(MutationError::Changed, QStringLiteral("Recovery record changed during inspection.")), {}};
  return decodeRecoveryRecord(bytes);
}

} // namespace

RecoveryRecordStore::RecoveryRecordStore(RecoveryDirectoryAdmission &directory,
                                         RecoveryStoreFault fault)
    : m_directory(directory), m_fault(std::move(fault)) {}

MutationResult RecoveryRecordStore::fault(RecoveryStoreStep step) const {
  const auto error = m_fault ? m_fault(step) : MutationError::None;
  return error == MutationError::None ? MutationResult{} :
      failure(error, QStringLiteral("Recovery durability step failed; preserve all entries."));
}

RecoveryRecordRead RecoveryRecordStore::inspect() const {
  const auto admitted = m_directory.privateCurrent();
  if (!admitted.ok()) return {admitted, {}};
  const int enumeration = ::openat(m_directory.descriptor(), ".",
                                    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (enumeration < 0)
    return {failure(errorForErrno(errno), QStringLiteral("Recovery records cannot be enumerated.")), {}};
  DIR *stream = ::fdopendir(enumeration);
  if (!stream) {
    ::close(enumeration);
    return {failure(errorForErrno(errno), QStringLiteral("Recovery records cannot be enumerated.")), {}};
  }
  std::set<QByteArray> recordSlots;
  MutationResult enumerationResult;
  errno = 0;
  while (const auto *entry = ::readdir(stream)) {
    const QByteArray name(entry->d_name);
    if (name == "." || name == ".." || name == "payload") continue;
    bool known = false;
    for (quint32 sequence = 0; sequence < maximumRecoveryRecords; ++sequence) {
      if (name == slotName(sequence)) { known = true; break; }
    }
    if (!known || recordSlots.size() >= maximumRecoveryRecords) {
      enumerationResult = failure(MutationError::InvalidRequest,
          QStringLiteral("Recovery directory contains unknown or excessive records."));
      break;
    }
    recordSlots.insert(name);
    errno = 0;
  }
  const int enumerationError = errno;
  ::closedir(stream);
  if (!enumerationResult.ok()) return {enumerationResult, {}};
  if (enumerationError != 0)
    return {failure(errorForErrno(enumerationError), QStringLiteral("Recovery enumeration was incomplete.")), {}};
  RecoveryRecordRead latest;
  for (quint32 sequence = 0; sequence < static_cast<quint32>(recordSlots.size()); ++sequence) {
    if (!recordSlots.contains(slotName(sequence))) {
      latest.result = failure(MutationError::InvalidRequest, QStringLiteral("Recovery sequence has a missing record."));
      return latest;
    }
    const auto next = readSlot(m_directory.descriptor(), sequence);
    if (!next.result.ok() || !next.record) {
      latest.result = next.result;
      return latest;
    }
    const auto &record = *next.record;
    if (record.sequence != sequence ||
        record.recoveryDirectory != m_directory.path() || record.recoveryStorage != m_directory.observation() ||
        (sequence == 0 && record.phase != RecoveryPhase::Prepared) ||
        (latest.record && (!sameRecoveryOperation(*latest.record, record) ||
          !recoveryTransitionAllowed(latest.record->phase, record.phase)))) {
      latest.result = failure(MutationError::InvalidRequest, QStringLiteral("Recovery operation or phase sequence is inconsistent."));
      return latest;
    }
    latest = next;
  }
  latest.result = m_directory.privateCurrent();
  return latest;
}

RecoveryRecordWrite RecoveryRecordStore::append(const RecoveryRecord &record) {
  RecoveryRecordWrite written;
  const QByteArray bytes = encodeRecoveryRecord(record);
  if (bytes.isEmpty() || record.recoveryDirectory != m_directory.path() || record.recoveryStorage != m_directory.observation()) {
    written.result = failure(MutationError::InvalidRequest, QStringLiteral("Recovery record is invalid."));
    return written;
  }
  const auto previous = inspect();
  if (!previous.result.ok()) { written.result = previous.result; return written; }
  if ((!previous.record && (record.sequence != 0 || record.phase != RecoveryPhase::Prepared)) ||
      (previous.record && (record.sequence != previous.record->sequence + 1 ||
          !sameRecoveryOperation(*previous.record, record) ||
          !recoveryTransitionAllowed(previous.record->phase, record.phase)))) {
    written.result = failure(MutationError::InvalidRequest, QStringLiteral("Recovery record does not extend this operation."));
    return written;
  }
  written.result = fault(RecoveryStoreStep::CreateRecord);
  if (!written.result.ok()) return written;
  written.result = m_directory.privateCurrent();
  if (!written.result.ok()) return written;
  const auto name = slotName(record.sequence);
  UniqueFd file(::openat(m_directory.descriptor(), name.constData(),
                         O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (!file.valid()) {
    written.result = failure(errorForErrno(errno), QStringLiteral("Recovery record slot is unavailable."));
    return written;
  }
  written.entryCreated = true;
  written.result = fault(RecoveryStoreStep::WriteRecord);
  if (!written.result.ok()) return written;
  qsizetype offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::write(file.get(), bytes.constData() + offset,
                               static_cast<size_t>(bytes.size() - offset));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      written.result = failure(errorForErrno(count < 0 ? errno : EIO),
                                QStringLiteral("Recovery record write failed; partial record retained."));
      return written;
    }
    offset += static_cast<qsizetype>(count);
  }
  written.result = fault(RecoveryStoreStep::SyncRecord);
  if (!written.result.ok()) return written;
  // AGENT-GUARD: Baseline before the acknowledged file durability barrier.
  // A same-inode rewrite afterward must not borrow that earlier acknowledgement.
  // Metadata is observed evidence, not mandatory exclusion of external writers.
  struct stat syncedVersion {};
  if (::fstat(file.get(), &syncedVersion) != 0) {
    written.result = failure(errorForErrno(errno), QStringLiteral("Recovery record version cannot be observed."));
    return written;
  }
  if (::fsync(file.get()) != 0) {
    written.result = failure(errorForErrno(errno), QStringLiteral("Recovery record durability is uncertain."));
    return written;
  }
  written.fileSynced = true;
  written.result = fault(RecoveryStoreStep::SyncDirectory);
  if (!written.result.ok()) return written;
  if (::fsync(m_directory.descriptor()) != 0) {
    written.result = failure(errorForErrno(errno), QStringLiteral("Recovery directory durability is uncertain."));
    return written;
  }
  written.directorySynced = true;
  written.result = m_directory.privateCurrent();
  if (!written.result.ok()) return written;
  struct stat held {};
  struct stat named {};
  if (::fstat(file.get(), &held) != 0 ||
      ::fstatat(m_directory.descriptor(), name.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
      !unchangedFile(syncedVersion, held) || !unchangedFile(held, named)) {
    written.result = failure(MutationError::Changed, QStringLiteral("Recovery record name changed after publication."));
    return written;
  }
  written.result = fault(RecoveryStoreStep::ReadRecord);
  if (!written.result.ok()) return written;
  // The separately opened reader must match the writer's synced version;
  // identical decoded bytes on a substituted inode do not establish durability.
  const auto readback = readSlot(m_directory.descriptor(), record.sequence, &syncedVersion);
  if (!readback.result.ok() || !readback.record || *readback.record != record) {
    written.result = readback.result.ok() ?
        failure(MutationError::Changed, QStringLiteral("Recovery record readback differs.")) : readback.result;
  }
  if (written.result.ok() &&
      (::fstat(file.get(), &held) != 0 ||
       ::fstatat(m_directory.descriptor(), name.constData(), &named, AT_SYMLINK_NOFOLLOW) != 0 ||
       !unchangedFile(syncedVersion, held) || !unchangedFile(held, named)))
    written.result = failure(MutationError::Changed, QStringLiteral("Recovery record version changed after readback."));
  if (written.result.ok()) written.result = m_directory.privateCurrent();
  return written;
}

} // namespace QindaQt::Apps::FileManager
