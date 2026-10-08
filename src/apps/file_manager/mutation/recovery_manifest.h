// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "recovery_mount_admission.h"

namespace QindaQt::Apps::FileManager {

struct RecoveryManifestEntry final {
  QByteArray relativePath; // Raw native bytes; empty names only the root.
  FileIdentity identity;
  qint64 changedNanoseconds = 0;
  quint64 mountId = 0;
  quint32 owner = 0;
  quint32 group = 0;
  QByteArray contentDigest; // SHA256 for files; empty for directories.
  bool operator==(const RecoveryManifestEntry &) const = default;
};
struct RecoveryManifest final {
  QVector<RecoveryManifestEntry> entries; // Strict native-byte lexical order.
  quint64 regularBytes = 0;
  bool operator==(const RecoveryManifest &) const = default;
};
struct RecoveryManifestRead final {
  MutationResult result;
  std::optional<RecoveryManifest> manifest;
};

// Same worker thread as borrowed live parent; owning result, no retained fds.
// Reads regular bytes through no-follow/nonblocking descriptors, rechecks each
// identity/ctime/name/mount after hashing, and binds exact directory child sets.
// No symlinks/special entries/nested mounts. Cancellation between bounded reads.
// A capture is an observed traversal, not a globally atomic snapshot or grant;
// publication/retirement must revalidate, and final retention protects late data.
[[nodiscard]] RecoveryManifestRead captureRecoveryManifest(
    const RecoveryDirectoryAdmission &parent, const QByteArray &leaf,
    const MutationCancellation &cancellation = {},
    const MutationProgressCallback &progress = {});

// Pure owning-value hashes. Empty means malformed or over bounds. Content
// comparison covers names, kind, permission bits, mtime and regular bytes,
// deliberately excluding filesystem-specific directory sizes and new inodes.
// Snapshot additionally includes live identity/ctime/mount/owner/group facts.
// Ownership/ACL/xattr clone semantics are not asserted by the content digest.
[[nodiscard]] QByteArray recoveryManifestDigest(const RecoveryManifest &manifest,
                                                bool includeIdentity);

// Identity and bytes must match. The one root ctime may change after an owner's
// successful whole-entry rename; callers may allow only that expected effect.
// This comparison still grants no rename/unlink authority.
[[nodiscard]] bool sameRecoveryTree(const RecoveryManifest &before,
                                    const RecoveryManifest &after,
                                    bool allowRootRename = false);
} // namespace QindaQt::Apps::FileManager
