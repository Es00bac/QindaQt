// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"

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
};

} // namespace QindaQt::Apps::FileManager
