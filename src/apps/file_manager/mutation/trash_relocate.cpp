// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_relocate.h"
#include "safe_tree_access_p.h"
#include <linux/fs.h>
#include <sys/syscall.h>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;
MutationResult admission(const RecoveryDirectoryAdmission &source,
    const RecoveryDirectoryAdmission &destination, const TrashStorage &store,
    const TrashRecord &record, const MutationCancellation &cancel) {
  if (cancel && cancel->load(std::memory_order_relaxed))
    return failure(MutationError::Cancelled, QStringLiteral("Trash operation cancelled; entries retained."));
  for (const auto *entry : {&source, &destination}) {
    const auto result = entry->current();
    if (!result.ok()) return result;
  }
  if (source.observation().mountId != destination.observation().mountId ||
      source.observation().device != destination.observation().device)
    return failure(MutationError::CrossDevice, QStringLiteral("Trash restore requires the same mounted filesystem."));
  return store.recordCurrent(record);
}
} // namespace
MutationResult relocateTrashEntry(
    const QString &source, const RecoveryDirectoryAdmission &sourceParent,
    const QString &destination, const RecoveryDirectoryAdmission &destinationParent,
    const FileIdentity &expected, const TrashStorage &store, const TrashRecord &record,
    const MutationCancellation &cancel, const TrashControl &control) {
  auto result = admission(sourceParent, destinationParent, store, record, cancel);
  if (!result.ok()) return result;
  const auto sourceName = QFile::encodeName(QFileInfo(source).fileName());
  const auto destinationName = QFile::encodeName(QFileInfo(destination).fileName());
  if (sourceName.isEmpty() || destinationName.isEmpty())
    return failure(MutationError::InvalidRequest, QStringLiteral("A filesystem root cannot be trashed."));
  struct stat before {};
  if (::fstatat(sourceParent.descriptor(), sourceName.constData(), &before, AT_SYMLINK_NOFOLLOW) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash source is unavailable."));
  if (identity(before) != expected)
    return failure(MutationError::Changed, QStringLiteral("Trash source changed before publication."));
  if (!S_ISREG(before.st_mode) && !S_ISDIR(before.st_mode) && !S_ISLNK(before.st_mode))
    return failure(MutationError::Unsupported, QStringLiteral("This filesystem entry cannot be trashed."));
  result = trashControl(control, TrashStep::BeforeRename);
  if (!result.ok()) return result;
  result = admission(sourceParent, destinationParent, store, record, cancel);
  if (!result.ok()) return result;
  // AGENT-GUARD: final links are opaque entries. Do not substitute open() or
  // QFileInfo::canonicalPath(), and never use a preflight exists()+rename().
#if defined(Q_OS_LINUX) && defined(SYS_renameat2)
  if (::syscall(SYS_renameat2, sourceParent.descriptor(), sourceName.constData(),
      destinationParent.descriptor(), destinationName.constData(), RENAME_NOREPLACE) != 0)
    return failure(errorForErrno(errno), QStringLiteral("Trash destination could not be published without replacing an entry."));
#else
  return failure(MutationError::Unsupported, QStringLiteral("No-replace Trash relocation is unavailable."));
#endif
  MutationOutputObservation observed;
  observed.path = destination;
  observed.disposition = MutationOutputDisposition::Unconfirmed;
  struct stat observedParent {};
  if (::fstat(destinationParent.descriptor(), &observedParent) == 0)
    observed.parentIdentity = identity(observedParent);
  const auto partial = [&](MutationResult value) {
    value.outputPath = destination;
    value.outputObservation = observed;
    value.diagnostic += QStringLiteral(" A rename occurred; retain the payload and metadata and inspect the reported path.");
    return value;
  };
  result = trashControl(control, TrashStep::AfterRename);
  if (!result.ok()) return partial(result);
  struct stat after {};
  if (::fstatat(destinationParent.descriptor(), destinationName.constData(), &after,
                AT_SYMLINK_NOFOLLOW) != 0)
    return partial(failure(MutationError::Changed, QStringLiteral("Trash payload placement cannot be confirmed.")));
  observed.observedIdentity = identity(after);
  if (identity(after) != expected) {
    observed.disposition = MutationOutputDisposition::Replaced;
    return partial(failure(MutationError::Changed, QStringLiteral("An unexpected entry was captured; it was retained without cleanup.")));
  }
  result = admission(sourceParent, destinationParent, store, record, {});
  if (!result.ok()) return partial(result);
  result = trashControl(control, TrashStep::SyncParents);
  if (!result.ok()) return partial(result);
  if (::fsync(destinationParent.descriptor()) != 0 || ::fsync(sourceParent.descriptor()) != 0)
    return partial(failure(errorForErrno(errno), QStringLiteral("Trash placement sync is uncertain.")));
  result = admission(sourceParent, destinationParent, store, record, {});
  if (!result.ok()) return partial(result);
  result = trashControl(control, TrashStep::ReadPayload);
  if (!result.ok()) return partial(result);
  result = admission(sourceParent, destinationParent, store, record, {});
  if (!result.ok()) return partial(result);
  struct stat confirmed {};
  if (::fstatat(destinationParent.descriptor(), destinationName.constData(), &confirmed,
                AT_SYMLINK_NOFOLLOW) != 0 || identity(confirmed) != expected)
    return partial(failure(MutationError::Changed, QStringLiteral("Trash payload changed after sync.")));
  result.outputPath = destination;
  result.outputIdentity = expected;
  return result;
}
} // namespace QindaQt::Apps::FileManager
