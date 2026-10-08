// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cross_volume_move.h"
#include "safe_tree_access_p.h"
#include <sys/syscall.h>
#include <linux/fs.h>
namespace QindaQt::Apps::FileManager::RecoveryMovePrivate {
using namespace SafeTreeAccess;
inline QByteArray leaf(const QString &path) { return QFile::encodeName(QFileInfo(path).fileName()); }
inline bool sameRoot(const FileIdentity &left, const FileIdentity &right) {
  return left.device == right.device && left.inode == right.inode &&
      (left.mode & S_IFMT) == (right.mode & S_IFMT);
}
inline MutationResult renameNoReplace(int sourceParent, const QByteArray &source,
                                     int targetParent, const QByteArray &target) {
#if defined(Q_OS_LINUX) && defined(SYS_renameat2)
  if (::syscall(SYS_renameat2, sourceParent, source.constData(), targetParent,
                target.constData(), RENAME_NOREPLACE) == 0) return {};
  return failure(errorForErrno(errno), QStringLiteral("No-replace rename refused; preserve every candidate."));
#else
  Q_UNUSED(sourceParent); Q_UNUSED(source); Q_UNUSED(targetParent); Q_UNUSED(target);
  return failure(MutationError::Unsupported, QStringLiteral("Atomic no-replace rename is unavailable."));
#endif
}
inline MutationRecoveryReceipt receipt(const RecoveryRecord &record) {
  MutationRecoveryReceipt value;
  value.operationId = record.operationId; value.phase = recoveryPhaseKey(record.phase);
  value.sourcePath = record.sourcePath; value.destinationPath = record.destinationPath;
  value.stageDirectory = record.stageDirectory; value.recoveryDirectory = record.recoveryDirectory;
  value.retainedBytesEstimate = record.retainedBytesEstimate; value.uncertain = record.uncertain;
  return value;
}
inline bool sameContent(const RecoveryManifest &left, const RecoveryManifest &right) {
  const auto a = recoveryManifestDigest(left, false);
  return !a.isEmpty() && a == recoveryManifestDigest(right, false);
}
inline MutationResult synchronize(int descriptor) {
  return ::fsync(descriptor) == 0 ? MutationResult{} :
      failure(errorForErrno(errno), QStringLiteral("Filesystem durability failed; retained data requires inspection."));
}
} // namespace QindaQt::Apps::FileManager::RecoveryMovePrivate
