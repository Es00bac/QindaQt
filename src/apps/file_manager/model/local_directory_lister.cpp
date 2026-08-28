// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_directory_lister.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

#include <algorithm>

namespace QindaQt::Apps::FileManager {

namespace {

// Directories sort before files; ties break case-insensitively, then
// case-sensitively, so ordering never depends on filesystem enumeration order.
[[nodiscard]] bool lessThan(const DirectoryEntry &a, const DirectoryEntry &b) {
  if (a.isDirectory != b.isDirectory) {
    return a.isDirectory;
  }
  const int caseInsensitive = a.name.compare(b.name, Qt::CaseInsensitive);
  if (caseInsensitive != 0) {
    return caseInsensitive < 0;
  }
  return a.name < b.name;
}

} // namespace

ListingResult LocalDirectoryLister::list(const QString &absolutePath) const {
  ListingResult result;
  result.path = absolutePath;

  const QFileInfo directoryInfo(absolutePath);
  if (!directoryInfo.exists()) {
    result.error = ListingError::NotFound;
    result.diagnostic = QStringLiteral("%1 does not exist").arg(absolutePath);
    return result;
  }
  if (!directoryInfo.isDir()) {
    result.error = ListingError::NotADirectory;
    result.diagnostic = QStringLiteral("%1 is not a folder").arg(absolutePath);
    return result;
  }
  if (!directoryInfo.isReadable()) {
    result.error = ListingError::PermissionDenied;
    result.diagnostic = QStringLiteral("%1 cannot be read").arg(absolutePath);
    return result;
  }

  QDir directory(absolutePath);
  directory.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System |
                       QDir::NoDotAndDotDot);
  const QFileInfoList infos = directory.entryInfoList();

  // AGENT-NOTE: A directory that reports readable permission bits can still
  // return an empty QDir listing on some filesystems this slice already
  // excludes (network/remote mounts). Local POSIX permission bits are already
  // checked above, so this slice does not attempt to distinguish "genuinely
  // empty" from "silently denied" any further.
  result.entries.reserve(
      static_cast<qsizetype>(std::min<qsizetype>(infos.size(), maximumEntries)));
  for (const QFileInfo &info : infos) {
    if (result.entries.size() >= maximumEntries) {
      result.truncated = true;
      break;
    }
    DirectoryEntry entry;
    entry.name = info.fileName();
    entry.absolutePath = info.absoluteFilePath();
    entry.isDirectory = info.isDir();
    entry.isSymlink = info.isSymLink();
    entry.isHidden = entry.name.startsWith(QLatin1Char('.'));
    entry.isReadable = info.isReadable();
    entry.size = entry.isDirectory ? 0 : info.size();
    entry.lastModified = info.lastModified();
    result.entries.append(std::move(entry));
  }
  std::sort(result.entries.begin(), result.entries.end(), lessThan);
  return result;
}

} // namespace QindaQt::Apps::FileManager
