// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace QindaQt::Apps::FileManager {

// The archive kinds Extract understands, decided by file name alone: zip, and
// tar either plain or compressed with gzip, bzip2, xz or zstd.
enum class ArchiveFormat {
  None,
  Zip,
  Tar,
  TarGzip,
  TarBzip2,
  TarXz,
  TarZstd,
};

// Case-insensitive suffix match (".zip", ".tar", ".tar.gz"/".tgz",
// ".tar.bz2"/".tbz2"/".tbz", ".tar.xz"/".txz", ".tar.zst"/".tzst"). Pure;
// the controller's Extract admission and every codec share it.
[[nodiscard]] ArchiveFormat archiveFormatForName(const QString &fileName);

// The folder name Extract creates for an archive: the file name without its
// archive suffix ("photos.tar.gz" -> "photos"). A name that would be empty or
// that is not an archive name is returned unchanged.
[[nodiscard]] QString archiveBaseName(const QString &fileName);

// AGENT-CONTRACT (ADR-0269): the archive work behind Compress and Extract.
// LocalMutationBackend owns policy -- declared roots, symlink-free paths, the
// optimistic identities, unique destination names, and removing a partial
// Extract folder -- and calls a codec from the mutation worker only. A codec
// owns the archive library (production: KArchiveCodec, the only File Manager
// code that names KArchive). It must poll `cancellation` between entries and
// between data chunks, report progress through `progress`, return every
// expected failure as a typed MutationResult, and never follow a symbolic
// link or write outside the one path or folder it was given.
class ArchiveCodec {
public:
  virtual ~ArchiveCodec() = default;

  // Writes a new zip archive at archivePath, whose folder must still be
  // `expectedParent`; each source tree is stored under its own name, links
  // as links. The archive is created exclusively (an existing name is never
  // replaced), and on any failure, cancellation included, nothing is left at
  // archivePath.
  [[nodiscard]] virtual MutationResult compress(
      const QStringList &sources, const QString &archivePath,
      const FileIdentity &expectedParent, const MutationCancellation &cancellation,
      const MutationProgressCallback &progress) = 0;

  // Unpacks archivePath, which must still be `expectedArchive`, into the
  // existing, empty folder `destination`. Entry names are single path
  // components; ".", "..", empty and separator-bearing names are refused, as
  // are special files. Links are recreated as links and never followed. On
  // failure the partial contents stay; the backend removes the folder.
  [[nodiscard]] virtual MutationResult extract(
      const QString &archivePath, const FileIdentity &expectedArchive,
      const QString &destination, const MutationCancellation &cancellation,
      const MutationProgressCallback &progress) = 0;
};

using ArchiveCodecPtr = std::shared_ptr<ArchiveCodec>;

} // namespace QindaQt::Apps::FileManager
