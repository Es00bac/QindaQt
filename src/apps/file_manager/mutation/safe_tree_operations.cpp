// SPDX-License-Identifier: GPL-3.0-or-later
#include "safe_tree_operations.h"
#include "safe_tree_access_p.h"

#include <dirent.h>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;

[[nodiscard]] bool removeEntryAt(
    int parent, const QByteArray &name,
    const MutationCancellation &cancellation = {},
    const MutationProgressCallback &progress = {}, int *removed = nullptr) {
  if (cancelled(cancellation)) {
    return false;
  }
  struct stat status {};
  if (::fstatat(parent, name.constData(), &status, AT_SYMLINK_NOFOLLOW) != 0) {
    return errno == ENOENT;
  }
  if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
    if (::unlinkat(parent, name.constData(), 0) != 0) {
      return false;
    }
    if (removed) {
      ++*removed;
      report(progress, *removed,
             QStringLiteral("Permanently removed %1 items").arg(*removed));
    }
    return true;
  }
  UniqueFd child(::openat(parent, name.constData(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!child.valid()) {
    return false;
  }
  const int enumerationFd = ::dup(child.get());
  DIR *directory = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!directory) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return false;
  }
  bool success = true;
  while (true) {
    errno = 0;
    const dirent *entry = ::readdir(directory);
    if (!entry) {
      success = success && errno == 0;
      break;
    }
    const QByteArray childName(entry->d_name);
    if (childName != "." && childName != ".." &&
        !removeEntryAt(child.get(), childName, cancellation, progress,
                       removed)) {
      success = false;
      break;
    }
  }
  ::closedir(directory);
  if (!success || ::unlinkat(parent, name.constData(), AT_REMOVEDIR) != 0) {
    return false;
  }
  if (removed) {
    ++*removed;
    report(progress, *removed,
           QStringLiteral("Permanently removed %1 items").arg(*removed));
  }
  return true;
}

} // namespace

bool removeLocalTreeNoFollow(const QString &path) {
  const QString name = QFileInfo(path).fileName();
  if (name.isEmpty()) {
    return false;
  }
  UniqueFd parent = openAbsoluteDirectory(QFileInfo(path).absolutePath());
  return parent.valid() && removeEntryAt(parent.get(), QFile::encodeName(name));
}

MutationResult deleteLocalTreeNoFollow(const QString &path, const FileIdentity &expected,
                                       const MutationCancellation &cancellation,
                                       const MutationProgressCallback &progress) {
  const QByteArray name = QFile::encodeName(QFileInfo(path).fileName());
  UniqueFd parent = name.isEmpty() ? UniqueFd() : openAbsoluteDirectory(QFileInfo(path).absolutePath());
  struct stat status {};
  int removed = 0;
  if (!parent.valid() || ::fstatat(parent.get(), name.constData(), &status, AT_SYMLINK_NOFOLLOW) != 0) {
    return failure(name.isEmpty() ? MutationError::InvalidRequest : errorForErrno(errno),
                   QStringLiteral("The item could not be reached safely"));
  }
  if (identity(status) != expected) {
    return failure(MutationError::Changed, QStringLiteral("The item changed before it could be deleted"));
  }
  return removeEntryAt(parent.get(), name, cancellation, progress, &removed)
      ? MutationResult{}
      : failure(cancelled(cancellation) ? MutationError::Cancelled : MutationError::PermissionDenied,
                QStringLiteral("Deleting stopped after %1 items").arg(removed));
}

MutationResult emptyLocalDirectoryNoFollow(
    const QString &path, const MutationCancellation &cancellation,
    const MutationProgressCallback &progress, int *removed) {
  if (!removed) {
    return failure(MutationError::InvalidRequest,
                   QStringLiteral("Empty Trash requires a progress counter"));
  }
  UniqueFd directory = openAbsoluteDirectory(path);
  if (!directory.valid()) {
    return errno == ENOENT
        ? MutationResult{}
        : failure(errorForErrno(errno),
                  QStringLiteral("A Trash directory could not be opened safely"));
  }
  const int enumerationFd = ::dup(directory.get());
  DIR *entries = enumerationFd >= 0 ? ::fdopendir(enumerationFd) : nullptr;
  if (!entries) {
    if (enumerationFd >= 0) {
      ::close(enumerationFd);
    }
    return failure(errorForErrno(errno),
                   QStringLiteral("A Trash directory could not be enumerated"));
  }
  MutationResult result;
  while (true) {
    errno = 0;
    const dirent *entry = ::readdir(entries);
    if (!entry) {
      if (errno != 0) {
        result = failure(errorForErrno(errno),
                         QStringLiteral("A Trash directory could not be enumerated"));
      }
      break;
    }
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    if (!removeEntryAt(directory.get(), name, cancellation, progress, removed)) {
      result = cancelled(cancellation)
          ? failure(MutationError::Cancelled, QStringLiteral("Empty Trash cancelled"))
          : failure(MutationError::PermissionDenied,
                    QStringLiteral("A Trash item could not be permanently removed"));
      break;
    }
  }
  ::closedir(entries);
  return result;
}

} // namespace QindaQt::Apps::FileManager
