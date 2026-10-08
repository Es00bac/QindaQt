// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_directory_lister.h"
#include "mutation/local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QSet>

#include <cerrno>
#include <dirent.h>
#include <QFileInfo>
#include <QFileInfoList>

#include <algorithm>

namespace QindaQt::Apps::FileManager {

namespace {

// AGENT-GUARD: QDir has already decoded native bytes by the time QFileInfo
// reaches us. A bad-byte name can therefore alias a real U+FFFD sibling.
// Preserve visible rows, but never mint mutation authority for either alias.
// Bound the native check; incomplete enumeration cannot prove any name safe.
struct NativeNames final {
  QSet<QString> refused;
  QFileInfoList infos;
  bool truncated = false;
  bool complete = false;
};

[[nodiscard]] NativeNames inspectNativeNames(const QString &path) {
  NativeNames result;
  const QByteArray encoded = QFile::encodeName(path);
  if (QFile::decodeName(encoded) != path) {
    return result;
  }
  DIR *directory = ::opendir(encoded.constData());
  if (directory == nullptr) {
    return result;
  }
  qsizetype count = 0;
  errno = 0;
  while (const auto *entry = ::readdir(directory)) {
    const QByteArray bytes(entry->d_name);
    if (bytes == "." || bytes == "..") {
      continue;
    }
    if (++count > LocalDirectoryLister::maximumEntries) {
      result.truncated = true;
      break;
    }
    // Keep an initial U+FEFF filename character; it is not a document BOM.
    const QString decoded = QFile::decodeName(QByteArray("/") + bytes).mid(1);
    if (QFile::encodeName(decoded) != bytes) {
      result.refused.insert(decoded);
    }
    result.infos.append(QFileInfo(QDir(path).filePath(decoded)));
    errno = 0;
  }
  result.complete = count <= LocalDirectoryLister::maximumEntries && errno == 0;
  ::closedir(directory);
  return result;
}

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

void LocalDirectoryLister::fillStatFacts(const QFileInfo &info, DirectoryEntry &entry) {
  // QFileInfo reports an unknown owner or group as uint(-2).
  constexpr uint unknownId = static_cast<uint>(-2);
  entry.created = info.birthTime();
  entry.accessed = info.lastRead();
  entry.ownerId = info.ownerId() == unknownId ? -1 : static_cast<qint64>(info.ownerId());
  entry.groupId = info.groupId() == unknownId ? -1 : static_cast<qint64>(info.groupId());
}

DirectoryEntry LocalDirectoryLister::entryFor(const QFileInfo &info) {
  DirectoryEntry entry;
  entry.name = info.fileName();
  entry.absolutePath = info.absoluteFilePath();
  entry.isDirectory = info.isDir();
  entry.isSymlink = info.isSymLink();
  entry.isHidden = entry.name.startsWith(QLatin1Char('.'));
  entry.isReadable = info.isReadable();
  entry.size = entry.isDirectory ? 0 : info.size();
  entry.lastModified = info.lastModified();
  fillStatFacts(info, entry);
  if (const auto identity = LocalMutationBackend::identityForPath(entry.absolutePath)) {
    entry.device = identity->device;
    entry.inode = identity->inode;
    entry.identitySize = identity->size;
    entry.modifiedNanoseconds = identity->modifiedNanoseconds;
    entry.mode = identity->mode;
  }
  return entry;
}

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

  const NativeNames nativeNames = inspectNativeNames(absolutePath);
  // Use the same native enumeration for displayed rows and identity admission;
  // a second QDir enumeration could introduce an unchecked decoded alias.
  const QFileInfoList &infos = nativeNames.infos;
  result.truncated = nativeNames.truncated;
  if (!nativeNames.complete && !nativeNames.truncated) {
    result.error = ListingError::PermissionDenied;
    result.diagnostic = QStringLiteral("%1 could not be completely enumerated").arg(absolutePath);
    return result;
  }
  result.entries.reserve(
      static_cast<qsizetype>(std::min<qsizetype>(infos.size(), maximumEntries)));
  for (const QFileInfo &info : infos) {
    if (result.entries.size() >= maximumEntries) {
      result.truncated = true;
      break;
    }
    DirectoryEntry entry = entryFor(info);
    if (!nativeNames.complete || nativeNames.refused.contains(entry.name)) {
      entry.device = 0;
      entry.inode = 0;
      entry.identitySize = 0;
      entry.modifiedNanoseconds = 0;
      entry.mode = 0;
    }
    result.entries.append(entry);
  }
  std::sort(result.entries.begin(), result.entries.end(), lessThan);
  return result;
}

} // namespace QindaQt::Apps::FileManager
