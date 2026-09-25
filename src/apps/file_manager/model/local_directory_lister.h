// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"

class QFileInfo;

namespace QindaQt::Apps::FileManager {

class LocalDirectoryLister final : public DirectoryLister {
public:
  // AGENT-NOTE: Bounds one listing so a pathological local directory (for
  // example a build cache with hundreds of thousands of entries) cannot block
  // the GUI thread building an unbounded QML model. Paging/virtualization is
  // a later slice; ListingResult::truncated tells the caller this bound was
  // hit rather than silently dropping entries.
  static constexpr qsizetype maximumEntries = 20000;

  [[nodiscard]] ListingResult list(const QString &absolutePath) const override;

  // ADR-0270: copies the Details view's extra facts (created, accessed,
  // owner and group ids) out of a QFileInfo whose stat is already cached, so
  // every local listing -- this one and SearchController's -- fills them the
  // same way at no extra I/O. Unknown values stay invalid / -1.
  static void fillStatFacts(const QFileInfo &info, DirectoryEntry &entry);

  // One entry exactly as list() reports it: name, flags, size, stat facts and
  // the lstat identity mutations check. ADR-0272: the Recents place builds
  // its rows with this, so a recent file is the same entry its folder lists.
  [[nodiscard]] static DirectoryEntry entryFor(const QFileInfo &info);
};

} // namespace QindaQt::Apps::FileManager
