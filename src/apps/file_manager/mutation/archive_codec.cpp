// SPDX-License-Identifier: GPL-3.0-or-later
#include "archive_codec.h"

#include <array>
#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

// Longest suffixes first, so ".tar.gz" wins over a hypothetical ".gz".
constexpr std::array<std::pair<const char *, ArchiveFormat>, 11> archiveSuffixes{{
    {".tar.bz2", ArchiveFormat::TarBzip2},
    {".tar.zst", ArchiveFormat::TarZstd},
    {".tar.gz", ArchiveFormat::TarGzip},
    {".tar.xz", ArchiveFormat::TarXz},
    {".tbz2", ArchiveFormat::TarBzip2},
    {".tzst", ArchiveFormat::TarZstd},
    {".tar", ArchiveFormat::Tar},
    {".tgz", ArchiveFormat::TarGzip},
    {".tbz", ArchiveFormat::TarBzip2},
    {".txz", ArchiveFormat::TarXz},
    {".zip", ArchiveFormat::Zip},
}};

[[nodiscard]] qsizetype matchedSuffixLength(const QString &fileName, ArchiveFormat *format) {
  for (const auto &[suffix, kind] : archiveSuffixes) {
    const QLatin1String text(suffix);
    if (fileName.size() > text.size() && fileName.endsWith(text, Qt::CaseInsensitive)) {
      *format = kind;
      return text.size();
    }
  }
  *format = ArchiveFormat::None;
  return 0;
}

} // namespace

ArchiveFormat archiveFormatForName(const QString &fileName) {
  ArchiveFormat format = ArchiveFormat::None;
  const qsizetype matched = matchedSuffixLength(fileName, &format);
  Q_UNUSED(matched);
  return format;
}

QString archiveBaseName(const QString &fileName) {
  ArchiveFormat format = ArchiveFormat::None;
  const qsizetype suffix = matchedSuffixLength(fileName, &format);
  return suffix > 0 ? fileName.chopped(suffix) : fileName;
}

} // namespace QindaQt::Apps::FileManager
