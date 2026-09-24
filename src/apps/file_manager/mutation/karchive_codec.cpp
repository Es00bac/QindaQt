// SPDX-License-Identifier: GPL-3.0-or-later
#include "karchive_codec.h"

#include "karchive_io.h"

#include <KZip>

#include <algorithm>
#include <array>
#include <dirent.h>

namespace QindaQt::Apps::FileManager {

using namespace KArchiveIo;

namespace {

struct ZipWriter {
  KZip &zip;
  const MutationCancellation &cancellation;
  const MutationProgressCallback &progress;
  int written = 0;

  void count() {
    ++written;
    if (progress) {
      progress({written, 0, QStringLiteral("Compressed %1 items").arg(written)});
    }
  }
};

[[nodiscard]] MutationResult writeFailure() {
  return failure(MutationError::IoError, QStringLiteral("The archive could not be written"));
}

[[nodiscard]] MutationResult addEntry(ZipWriter &writer, int parent, const QByteArray &name,
                                      const QString &archiveName, int depth);

[[nodiscard]] MutationResult addFile(ZipWriter &writer, int parent, const QByteArray &name,
                                     const QString &archiveName, const struct stat &listed) {
  UniqueFd input(::openat(parent, name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat opened {};
  if (!input.valid() || ::fstat(input.get(), &opened) != 0) {
    return failure(mutationErrorForErrno(errno), QStringLiteral("A file could not be opened"));
  }
  if (!S_ISREG(opened.st_mode) || opened.st_dev != listed.st_dev || opened.st_ino != listed.st_ino) {
    return failure(MutationError::Changed,
                   QStringLiteral("A file changed before it could be compressed"));
  }
  const QDateTime modified = modifiedTime(opened);
  if (!writer.zip.prepareWriting(archiveName, QString(), QString(), opened.st_size,
                                 S_IFREG | (opened.st_mode & 0777), modified, modified, modified)) {
    return writeFailure();
  }
  std::array<char, 64 * 1024> buffer{};
  qint64 total = 0;
  while (true) {
    if (cancelled(writer.cancellation)) {
      return failure(MutationError::Cancelled, QStringLiteral("Compress cancelled"));
    }
    const ssize_t count = ::read(input.get(), buffer.data(), buffer.size());
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count < 0) {
      return failure(mutationErrorForErrno(errno), QStringLiteral("A file could not be read"));
    }
    if (count == 0) {
      break;
    }
    if (!writer.zip.writeData(buffer.data(), count)) {
      return writeFailure();
    }
    total += count;
  }
  if (total != opened.st_size) {
    return failure(MutationError::Changed,
                   QStringLiteral("A file changed while it was being compressed"));
  }
  if (!writer.zip.finishWriting(total)) {
    return writeFailure();
  }
  writer.count();
  return {};
}

[[nodiscard]] MutationResult addLink(ZipWriter &writer, int parent, const QByteArray &name,
                                     const QString &archiveName, const struct stat &listed) {
  std::array<char, 4096> target{};
  const ssize_t length = ::readlinkat(parent, name.constData(), target.data(), target.size());
  if (length <= 0 || length >= static_cast<ssize_t>(target.size())) {
    return failure(length < 0 ? mutationErrorForErrno(errno) : MutationError::Unsupported,
                   QStringLiteral("A link could not be read"));
  }
  const QDateTime modified = modifiedTime(listed);
  if (!writer.zip.writeSymLink(archiveName, QFile::decodeName(QByteArray(target.data(), length)),
                               QString(), QString(), S_IFLNK | 0777, modified, modified,
                               modified)) {
    return writeFailure();
  }
  writer.count();
  return {};
}

[[nodiscard]] MutationResult addDirectory(ZipWriter &writer, int parent, const QByteArray &name,
                                          const QString &archiveName, const struct stat &listed,
                                          int depth) {
  if (depth > KArchiveCodec::maximumDepth) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("A folder is nested too deeply to compress"));
  }
  UniqueFd directory(::openat(parent, name.constData(), directoryFlags));
  struct stat opened {};
  if (!directory.valid() || ::fstat(directory.get(), &opened) != 0) {
    return failure(mutationErrorForErrno(errno), QStringLiteral("A folder could not be opened"));
  }
  if (opened.st_dev != listed.st_dev || opened.st_ino != listed.st_ino) {
    return failure(MutationError::Changed,
                   QStringLiteral("A folder changed before it could be compressed"));
  }
  const QDateTime modified = modifiedTime(opened);
  if (!writer.zip.writeDir(archiveName, QString(), QString(), S_IFDIR | (opened.st_mode & 0777),
                           modified, modified, modified)) {
    return writeFailure();
  }
  writer.count();
  const int enumerationFd = ::fcntl(directory.get(), F_DUPFD_CLOEXEC, 0);
  DIR *entries = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!entries) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return failure(mutationErrorForErrno(errno), QStringLiteral("A folder could not be read"));
  }
  QList<QByteArray> names;
  errno = 0;
  while (const dirent *entry = ::readdir(entries)) {
    const QByteArray child(entry->d_name);
    if (child != "." && child != "..") {
      names.append(child);
    }
    if (writer.written + names.size() > KArchiveCodec::maximumEntries) {
      break;
    }
    errno = 0;
  }
  const int enumerationError = errno;
  ::closedir(entries);
  if (enumerationError != 0) {
    return failure(mutationErrorForErrno(enumerationError),
                   QStringLiteral("A folder could not be read"));
  }
  std::sort(names.begin(), names.end()); // a deterministic archive order
  for (const QByteArray &child : names) {
    const auto added = addEntry(writer, directory.get(), child,
                                archiveName + QLatin1Char('/') + QFile::decodeName(child), depth + 1);
    if (!added.ok()) {
      return added;
    }
  }
  return {};
}

MutationResult addEntry(ZipWriter &writer, int parent, const QByteArray &name,
                        const QString &archiveName, int depth) {
  if (cancelled(writer.cancellation)) {
    return failure(MutationError::Cancelled, QStringLiteral("Compress cancelled"));
  }
  if (writer.written >= KArchiveCodec::maximumEntries) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("The selection exceeds the item safety bound"));
  }
  struct stat listed {};
  if (::fstatat(parent, name.constData(), &listed, AT_SYMLINK_NOFOLLOW) != 0) {
    return failure(mutationErrorForErrno(errno), QStringLiteral("An item vanished"));
  }
  if (S_ISLNK(listed.st_mode)) {
    return addLink(writer, parent, name, archiveName, listed);
  }
  if (S_ISREG(listed.st_mode)) {
    return addFile(writer, parent, name, archiveName, listed);
  }
  if (S_ISDIR(listed.st_mode)) {
    return addDirectory(writer, parent, name, archiveName, listed, depth);
  }
  return failure(MutationError::Unsupported,
                 QStringLiteral("Only files, folders and links can be compressed"));
}

[[nodiscard]] MutationResult writeZip(QIODevice &device, const QStringList &sources,
                                      const MutationCancellation &cancellation,
                                      const MutationProgressCallback &progress, int *written) {
  KZip zip(&device);
  if (!zip.open(QIODevice::WriteOnly)) {
    return failure(MutationError::IoError, QStringLiteral("The archive could not be started"));
  }
  ZipWriter writer{zip, cancellation, progress};
  MutationResult result;
  for (const QString &source : sources) {
    const QFileInfo info(source);
    UniqueFd parent = openAbsoluteDirectory(info.absolutePath());
    result = parent.valid()
        ? addEntry(writer, parent.get(), QFile::encodeName(info.fileName()), info.fileName(), 0)
        : failure(mutationErrorForErrno(errno),
                  QStringLiteral("An item's folder could not be opened safely"));
    if (!result.ok()) {
      break;
    }
  }
  *written = writer.written;
  if (!zip.close() && result.ok()) {
    result = failure(MutationError::IoError, QStringLiteral("The archive could not be finished"));
  }
  return result;
}

} // namespace

MutationResult KArchiveCodec::compress(const QStringList &sources, const QString &archivePath,
                                       const FileIdentity &expectedParent,
                                       const MutationCancellation &cancellation,
                                       const MutationProgressCallback &progress) {
  const QByteArray name = QFile::encodeName(QFileInfo(archivePath).fileName());
  UniqueFd parent = name.isEmpty() ? UniqueFd() : openAbsoluteDirectory(QFileInfo(archivePath).absolutePath());
  struct stat parentStatus {};
  if (!parent.valid() || ::fstat(parent.get(), &parentStatus) != 0) {
    return failure(name.isEmpty() ? MutationError::InvalidRequest : mutationErrorForErrno(errno),
                   QStringLiteral("The archive's folder could not be opened safely"));
  }
  if (identityOf(parentStatus) != expectedParent) {
    return failure(MutationError::Changed, QStringLiteral("The archive's folder changed"));
  }
  UniqueFd archive(::openat(parent.get(), name.constData(),
                            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0666));
  struct stat created {};
  if (!archive.valid() || ::fstat(archive.get(), &created) != 0) {
    return failure(mutationErrorForErrno(errno), QStringLiteral("The archive could not be created"));
  }
  int written = 0;
  MutationResult result;
  {
    QFile output;
    result = output.open(archive.get(), QIODevice::WriteOnly, QFileDevice::DontCloseHandle)
        ? writeZip(output, sources, cancellation, progress, &written)
        : failure(MutationError::IoError, QStringLiteral("The archive could not be opened"));
    // KArchive::close() closes the device itself; closing again is a no-op
    // that flushes anything still buffered, and error() keeps the outcome.
    output.close();
    if (output.error() != QFileDevice::NoError && result.ok()) {
      result = failure(MutationError::IoError, QStringLiteral("The archive could not be written"));
    }
  }
  if (result.ok() && ::fsync(archive.get()) != 0) {
    result = failure(mutationErrorForErrno(errno), QStringLiteral("The archive could not be committed"));
  }
  if (!result.ok()) {
    // AGENT-GUARD: remove only the file this call created (same inode), so a
    // failed or cancelled Compress never leaves half an archive behind and
    // never removes a name someone else has taken since.
    struct stat current {};
    if (::fstatat(parent.get(), name.constData(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
        current.st_dev == created.st_dev && current.st_ino == created.st_ino) {
      const int ignoredRemoval = ::unlinkat(parent.get(), name.constData(), 0);
      Q_UNUSED(ignoredRemoval);
    }
    return result;
  }
  result.outputPath = archivePath;
  result.diagnostic = QStringLiteral("Compressed %1 items into “%2”")
                          .arg(written)
                          .arg(QFileInfo(archivePath).fileName());
  return result;
}

} // namespace QindaQt::Apps::FileManager
