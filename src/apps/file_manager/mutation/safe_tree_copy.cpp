// SPDX-License-Identifier: GPL-3.0-or-later
#include "safe_tree_operations.h"
#include "safe_tree_access_p.h"

#include <array>
#include <dirent.h>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;

struct CreatedCopyRoot {
  bool created = false;
  bool exclusiveCreation = false;
  std::optional<FileIdentity> identity;
};

// Keep evidence from the descriptor we created, even when its pathname is
// replaced by a callback or another writer. Observations never permit cleanup.
class CopyRootCapture final {
public:
  CopyRootCapture(int descriptor, CreatedCopyRoot *output)
      : m_descriptor(descriptor), m_output(output) {
    if (m_output) {
      m_output->created = true;
      observe();
    }
  }
  ~CopyRootCapture() { observe(); }
private:
  void observe() {
    if (!m_output)
      return;
    struct stat status {};
    if (::fstat(m_descriptor, &status) == 0)
      m_output->identity = SafeTreeAccess::identity(status);
    else
      m_output->identity.reset();
  }
  int m_descriptor;
  CreatedCopyRoot *m_output;
};

[[nodiscard]] MutationResult copyFileAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const struct stat &initial,
    const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied,
    CreatedCopyRoot *createdRoot) {
  UniqueFd source(::openat(sourceParent, sourceName.constData(),
                           O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat opened {};
  if (!source.valid() || ::fstat(source.get(), &opened) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The source file could not be opened"));
  }
  if (!S_ISREG(opened.st_mode) || !sameIdentity(initial, opened)) {
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed before it could be copied"));
  }
  UniqueFd destination(::openat(
      destinationParent, destinationName.constData(),
      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
      opened.st_mode & 07777));
  if (!destination.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination file could not be created"));
  }

  if (createdRoot)
    createdRoot->exclusiveCreation = true;
  CopyRootCapture capture(destination.get(), createdRoot);

  std::array<char, 64 * 1024> buffer{};
  while (!cancelled(token)) {
    const ssize_t count = ::read(source.get(), buffer.data(), buffer.size());
    if (count == 0) {
      break;
    }
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return failure(errorForErrno(errno),
                     QStringLiteral("The source file could not be read"));
    }
    ssize_t offset = 0;
    while (offset < count) {
      const ssize_t written = ::write(destination.get(), buffer.data() + offset,
                                      static_cast<size_t>(count - offset));
      if (written < 0 && errno == EINTR) {
        continue;
      }
      if (written <= 0) {
        return failure(errorForErrno(errno),
                       QStringLiteral("The destination file could not be written"));
      }
      offset += written;
    }
    report(progress, *copied, QStringLiteral("Copying file data"));
  }
  if (cancelled(token)) {
    return failure(MutationError::Cancelled, QStringLiteral("Copy cancelled"));
  }
  const struct timespec times[2] = {opened.st_atim, opened.st_mtim};
  const int ignoredPermissions = ::fchmod(destination.get(), opened.st_mode & 07777);
  const int ignoredTimes = ::futimens(destination.get(), times);
  Q_UNUSED(ignoredPermissions);
  Q_UNUSED(ignoredTimes);
  if (::fsync(destination.get()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The copied file could not be committed"));
  }
  struct stat finalSource {};
  if (::fstat(source.get(), &finalSource) != 0 ||
      !sameIdentity(opened, finalSource)) {
    return failure(MutationError::Changed,
                   QStringLiteral("The source changed while it was being copied"));
  }
  ++*copied;
  report(progress, *copied, QStringLiteral("Copied %1 items").arg(*copied));
  return {};
}

[[nodiscard]] MutationResult copyEntryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems,
    CreatedCopyRoot *createdRoot = nullptr);

[[nodiscard]] MutationResult copyDirectoryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const struct stat &initial,
    const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems,
    CreatedCopyRoot *createdRoot) {
  UniqueFd source(::openat(sourceParent, sourceName.constData(),
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  struct stat opened {};
  if (!source.valid() || ::fstat(source.get(), &opened) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A source folder could not be opened"));
  }
  if (!S_ISDIR(opened.st_mode) || !sameIdentity(initial, opened)) {
    return failure(MutationError::Changed,
                   QStringLiteral("A source folder changed before it could be copied"));
  }
  if (::mkdirat(destinationParent, destinationName.constData(),
                opened.st_mode & 07777) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A destination folder could not be created"));
  }
  if (createdRoot)
    createdRoot->created = true;
  UniqueFd destination(::openat(destinationParent, destinationName.constData(),
                                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!destination.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A destination folder could not be opened"));
  }
  // mkdirat does not return a descriptor. This opened directory is what
  // traversal writes; do not claim atomic creation ownership for it.
  CopyRootCapture capture(destination.get(), createdRoot);
  const int enumerationFd = ::dup(source.get());
  DIR *directory = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!directory) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return failure(errorForErrno(errno),
                   QStringLiteral("A source folder could not be enumerated"));
  }
  MutationResult result;
  errno = 0;
  while (const dirent *entry = ::readdir(directory)) {
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    result = copyEntryAt(source.get(), name, destination.get(), name, token,
                         progress, copied, maximumItems);
    if (!result.ok()) {
      break;
    }
    errno = 0;
  }
  const int enumerationError = errno;
  ::closedir(directory);
  if (result.ok() && enumerationError != 0) {
    result = failure(errorForErrno(enumerationError),
                     QStringLiteral("A source folder could not be enumerated"));
  }
  struct stat finalSource {};
  if (result.ok() && (::fstat(source.get(), &finalSource) != 0 ||
                      !sameIdentity(opened, finalSource))) {
    result = failure(MutationError::Changed,
                     QStringLiteral("A source folder changed while it was being copied"));
  }
  if (!result.ok()) {
    return result;
  }
  const struct timespec times[2] = {opened.st_atim, opened.st_mtim};
  const int ignoredPermissions = ::fchmod(destination.get(), opened.st_mode & 07777);
  const int ignoredTimes = ::futimens(destination.get(), times);
  Q_UNUSED(ignoredPermissions);
  Q_UNUSED(ignoredTimes);
  if (::fsync(destination.get()) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A copied folder could not be committed"));
  }
  ++*copied;
  report(progress, *copied, QStringLiteral("Copied %1 items").arg(*copied));
  return {};
}

MutationResult copyEntryAt(
    int sourceParent, const QByteArray &sourceName, int destinationParent,
    const QByteArray &destinationName, const MutationCancellation &token,
    const MutationProgressCallback &progress, int *copied, int maximumItems,
    CreatedCopyRoot *createdRoot) {
  if (cancelled(token)) {
    return failure(MutationError::Cancelled, QStringLiteral("Copy cancelled"));
  }
  if (*copied >= maximumItems) {
    return failure(MutationError::Unsupported,
                   QStringLiteral("The copy exceeds the item safety bound"));
  }
  struct stat status {};
  if (::fstatat(sourceParent, sourceName.constData(), &status,
                AT_SYMLINK_NOFOLLOW) != 0) {
    return failure(errorForErrno(errno),
                   QStringLiteral("A source item vanished"));
  }
  if (S_ISLNK(status.st_mode)) {
    return failure(MutationError::SymlinkEscape,
                   QStringLiteral("A symbolic link was found inside the copy"));
  }
  if (S_ISREG(status.st_mode)) {
    return copyFileAt(sourceParent, sourceName, destinationParent,
                      destinationName, status, token, progress, copied, createdRoot);
  }
  if (S_ISDIR(status.st_mode)) {
    return copyDirectoryAt(sourceParent, sourceName, destinationParent,
                           destinationName, status, token, progress, copied,
                           maximumItems, createdRoot);
  }
  return failure(MutationError::Unsupported,
                 QStringLiteral("Only regular files and folders can be copied"));
}

} // namespace

using namespace SafeTreeAccess;

MutationOutputObservation observeCopyOutputNoFollow(
    const QString &path, const std::optional<FileIdentity> &writtenIdentity,
    const std::optional<FileIdentity> &parentIdentity, bool copyFinished,
    bool exclusiveCreation) {
  MutationOutputObservation observation;
  observation.disposition = MutationOutputDisposition::Unconfirmed;
  observation.path = path;
  observation.writtenIdentity = writtenIdentity;
  observation.parentIdentity = parentIdentity;
  observation.exclusiveCreation = exclusiveCreation;
  observation.copyFinished = copyFinished;
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(path).absolutePath());
  struct stat parentStatus {};
  // Directory size/time legitimately change when this operation creates an
  // entry; compare stable parent identity/type rather than claiming a lease.
  if (!parentIdentity || !parent.valid() ||
      ::fstat(parent.get(), &parentStatus) != 0 ||
      static_cast<quint64>(parentStatus.st_dev) != parentIdentity->device ||
      static_cast<quint64>(parentStatus.st_ino) != parentIdentity->inode ||
      static_cast<quint32>(parentStatus.st_mode) != parentIdentity->mode)
    return observation;
  struct stat current {};
  const QByteArray name = QFile::encodeName(QFileInfo(path).fileName());
  if (name.isEmpty() ||
      ::fstatat(parent.get(), name.constData(), &current, AT_SYMLINK_NOFOLLOW) != 0)
    return observation;
  observation.observedIdentity = identity(current);
  if (!writtenIdentity)
    return observation;
  if (observation.observedIdentity != writtenIdentity) {
    observation.disposition = MutationOutputDisposition::Replaced;
    return observation;
  }
  observation.disposition = copyFinished ? MutationOutputDisposition::RetainedCopy
                                        : MutationOutputDisposition::RetainedPartial;
  return observation;
}

MutationResult copyLocalTreeNoFollow(
    const QString &source, const QString &destination,
    const FileIdentity &expectedDestinationParent,
    const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int maximumItems) {
  const QString sourceName = QFileInfo(source).fileName();
  const QString destinationName = QFileInfo(destination).fileName();
  if (sourceName.isEmpty() || destinationName.isEmpty()) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Filesystem roots cannot be copied"));
  }
  UniqueFd sourceParent = openAbsoluteDirectory(QFileInfo(source).absolutePath());
  if (!sourceParent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The source parent could not be opened safely"));
  }
  UniqueFd destinationParent =
      openAbsoluteDirectory(QFileInfo(destination).absolutePath());
  if (!destinationParent.valid()) {
    return failure(errorForErrno(errno),
                   QStringLiteral("The destination parent could not be opened safely"));
  }
  struct stat destinationParentStatus {};
  if (::fstat(destinationParent.get(), &destinationParentStatus) != 0 ||
      identity(destinationParentStatus) != expectedDestinationParent) {
    return failure(MutationError::Changed,
                   QStringLiteral("The destination parent changed"));
  }
  int copied = 0;
  CreatedCopyRoot createdRoot;
  MutationResult result = copyEntryAt(
      sourceParent.get(), QFile::encodeName(sourceName), destinationParent.get(),
      QFile::encodeName(destinationName), cancellation, progress, &copied,
      maximumItems, &createdRoot);
  // AGENT-GUARD: Failed exclusive creation conveys no ownership. Even a
  // created entry may have been replaced; never recursively clean by pathname.
  if (createdRoot.created) {
    result.outputObservation = observeCopyOutputNoFollow(
        destination, createdRoot.identity, expectedDestinationParent, result.ok(),
        createdRoot.exclusiveCreation);
  }
  if (result.ok()) {
    if (result.outputObservation.disposition !=
        MutationOutputDisposition::RetainedCopy) {
      result.error = MutationError::Changed;
      result.diagnostic = QStringLiteral("The copy output location changed");
      return result;
    }
    result.outputPath = destination;
    result.outputIdentity = createdRoot.identity;
  }
  return result;
}

} // namespace QindaQt::Apps::FileManager
