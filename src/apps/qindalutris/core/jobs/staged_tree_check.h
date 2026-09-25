// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the second path-escape gate for Proton builds (ADR-0275
// section 2). After `tar --extract` and BEFORE the atomic rename into the
// compatibility-tools root, ProtonInstallJob verifies the real staged tree
// `<extractDir>/<toolName>`:
//  - the top folder is a real directory (not a link);
//  - a bounded walk that never follows links (lstat on every entry) finds
//    only directories, regular files and symlinks -- no devices, FIFOs or
//    sockets, and no setuid/setgid file;
//  - every symlink, resolved physically against the staged tree itself
//    (link_resolution.h: each link component replaced by its own target,
//    absolute targets refused, at most kMaxLinkHops), stays inside
//    `<extractDir>/<toolName>`;
//  - every regular file with more than one hard link has ALL its links
//    inside the tree (st_nlink equals the number of names found for that
//    inode), so no name in the build shares an inode with a file outside.
// Any failure refuses the whole install. Guarantee: a committed build holds
// no link, followed on disk, that leads outside the build. It does NOT
// judge links that dangle inside the build (they cannot be followed out).
// Pure filesystem reads, no Qt objects: safe to run on a worker thread.

struct StagedTreeVerdict final {
  bool ok = false;
  QString reason;       // for the details log when !ok
  qsizetype entries = 0; // entries examined
};

inline constexpr qsizetype kMaxStagedEntries = 300000;

[[nodiscard]] StagedTreeVerdict verifyStagedBuild(const QString &extractDir,
                                                  const QString &toolName,
                                                  qsizetype maxEntries = kMaxStagedEntries);

} // namespace QindaQt::QindaLutris
