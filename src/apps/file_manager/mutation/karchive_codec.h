// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "archive_codec.h"

namespace QindaQt::Apps::FileManager {

// ADR-0269: ArchiveCodec over KDE Frameworks' KArchive, the only File Manager
// code that names the library. Compress writes zip (deflate); Extract reads
// zip and tar, plain or gzip/bzip2/xz/zstd-compressed, as far as the
// installed KArchive supports each compression. Every descriptor is opened
// relative to a parent opened without following links, the archive is read
// from the very descriptor whose identity was checked, and extracted entries
// are created exclusively beneath the destination, so neither direction can
// be redirected through a link. Stateless; called on the mutation worker.
class KArchiveCodec final : public ArchiveCodec {
public:
  // Per-request safety bounds: entries written or extracted, folder depth.
  static constexpr int maximumEntries = 20'000;
  static constexpr int maximumDepth = 64;

  [[nodiscard]] MutationResult compress(
      const QStringList &sources, const QString &archivePath,
      const FileIdentity &expectedParent, const MutationCancellation &cancellation,
      const MutationProgressCallback &progress) override;

  [[nodiscard]] MutationResult extract(
      const QString &archivePath, const FileIdentity &expectedArchive,
      const QString &destination, const MutationCancellation &cancellation,
      const MutationProgressCallback &progress) override;
};

} // namespace QindaQt::Apps::FileManager
