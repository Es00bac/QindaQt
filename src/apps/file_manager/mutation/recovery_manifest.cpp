// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_manifest.h"
#include "safe_tree_access_p.h"

#include <QCryptographicHash>
#include <algorithm>
#include <array>
#include <dirent.h>
#include <limits>

namespace QindaQt::Apps::FileManager {
namespace {
using namespace SafeTreeAccess;

std::optional<quint64> mountAt(int descriptor, const QByteArray &name = {}) {
#if defined(Q_OS_LINUX) && defined(STATX_MNT_ID)
  struct statx status {};
  const int flags = AT_SYMLINK_NOFOLLOW | (name.isEmpty() ? AT_EMPTY_PATH : 0);
  if (::statx(descriptor, name.constData(), flags, STATX_MNT_ID, &status) != 0 ||
      (status.stx_mask & STATX_MNT_ID) == 0 || status.stx_mnt_id == 0)
    return std::nullopt;
  return status.stx_mnt_id;
#else
  Q_UNUSED(descriptor)
  Q_UNUSED(name)
  return std::nullopt;
#endif
}

bool timeValue(const struct timespec &time, qint64 &value) {
  constexpr qint64 scale = 1'000'000'000;
  if (time.tv_sec < std::numeric_limits<qint64>::min() / scale ||
      time.tv_sec > std::numeric_limits<qint64>::max() / scale ||
      time.tv_nsec < 0 || time.tv_nsec >= scale) return false;
  const auto seconds = static_cast<qint64>(time.tv_sec) * scale;
  if (seconds > std::numeric_limits<qint64>::max() - time.tv_nsec) return false;
  value = seconds + time.tv_nsec;
  return true;
}
bool sameStat(const struct stat &left, const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
      left.st_size == right.st_size && left.st_mode == right.st_mode &&
      left.st_uid == right.st_uid && left.st_gid == right.st_gid &&
      left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
      left.st_mtim.tv_nsec == right.st_mtim.tv_nsec &&
      left.st_ctim.tv_sec == right.st_ctim.tv_sec &&
      left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

class Capture final {
public:
  Capture(quint64 mount, quint64 device, const MutationCancellation &cancel,
          const MutationProgressCallback &progress)
      : m_mount(mount), m_device(device), m_cancel(cancel), m_progress(progress) {}

  MutationResult entry(int parent, const QByteArray &name,
                       const QByteArray &relative, int depth) {
    if (cancelled(m_cancel)) return failure(MutationError::Cancelled, QStringLiteral("Verification cancelled."));
    if (depth > 128 || relative.size() > 4096 || m_manifest.entries.size() >= 20'000)
      return failure(MutationError::Unsupported, QStringLiteral("The tree exceeds recovery bounds."));
    m_budget += relative.size() + 128;
    if (m_budget > 16 * 1024 * 1024)
      return failure(MutationError::Unsupported, QStringLiteral("The manifest exceeds its byte bound."));
    struct stat namedBefore {};
    if (::fstatat(parent, name.constData(), &namedBefore, AT_SYMLINK_NOFOLLOW) != 0)
      return failure(errorForErrno(errno), QStringLiteral("A recovery tree entry vanished."));
    if (S_ISLNK(namedBefore.st_mode))
      return failure(MutationError::SymlinkEscape, QStringLiteral("Recovery refuses symbolic links."));
    if (!S_ISDIR(namedBefore.st_mode) && !S_ISREG(namedBefore.st_mode))
      return failure(MutationError::Unsupported, QStringLiteral("Recovery supports regular files and directories only."));
    UniqueFd fd(::openat(parent, name.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK |
                        O_CLOEXEC | (S_ISDIR(namedBefore.st_mode) ? O_DIRECTORY : 0)));
    struct stat before {};
    if (!fd.valid() || ::fstat(fd.get(), &before) != 0)
      return failure(errorForErrno(errno), QStringLiteral("A recovery tree entry cannot be pinned."));
    if (!sameStat(namedBefore, before))
      return failure(MutationError::Changed, QStringLiteral("A recovery tree entry changed before opening."));
    const auto mount = mountAt(fd.get());
    if (!mount || *mount != m_mount || static_cast<quint64>(before.st_dev) != m_device)
      return failure(MutationError::Unsupported, QStringLiteral("Nested mounts or missing mount evidence are unsupported."));
    RecoveryManifestEntry value;
    value.relativePath = relative;
    qint64 modified = 0;
    if (before.st_size < 0 || !timeValue(before.st_mtim, modified) ||
        !timeValue(before.st_ctim, value.changedNanoseconds))
      return failure(MutationError::Unsupported, QStringLiteral("File metadata exceeds recovery bounds."));
    value.identity = {static_cast<quint64>(before.st_dev), static_cast<quint64>(before.st_ino),
                      static_cast<qint64>(before.st_size), modified, static_cast<quint32>(before.st_mode)};
    value.mountId = *mount;
    value.owner = static_cast<quint32>(before.st_uid);
    value.group = static_cast<quint32>(before.st_gid);
    QVector<QByteArray> children;
    MutationResult result;
    if (S_ISDIR(before.st_mode)) {
      result = list(fd.get(), children, true);
      if (!result.ok()) return result;
      // Count this parent before recursion, so the entry bound also limits
      // active stack allocations and an empty-directory-heavy tree.
      m_manifest.entries.append(value);
      for (const auto &child : children) {
        const auto childPath = relative.isEmpty() ? child : relative + '/' + child;
        result = entry(fd.get(), child, childPath, depth + 1);
        if (!result.ok()) return result;
      }
      QVector<QByteArray> finalChildren;
      result = list(fd.get(), finalChildren, false);
      if (!result.ok()) return result;
      if (children != finalChildren)
        return failure(MutationError::Changed, QStringLiteral("A recovery directory child set changed."));
    } else {
      result = hashFile(fd.get(), before.st_size, value.contentDigest);
      if (!result.ok()) return result;
      const auto size = static_cast<quint64>(before.st_size);
      if (size > std::numeric_limits<quint64>::max() - m_manifest.regularBytes)
        return failure(MutationError::Unsupported, QStringLiteral("Recovery retained-byte accounting overflowed."));
      m_manifest.regularBytes += size;
      m_manifest.entries.append(value);
    }
    struct stat after {};
    struct stat namedAfter {};
    if (::fstat(fd.get(), &after) != 0 ||
        ::fstatat(parent, name.constData(), &namedAfter, AT_SYMLINK_NOFOLLOW) != 0 ||
        !sameStat(before, after) || !sameStat(after, namedAfter) ||
        mountAt(parent, name) != mount)
      return failure(MutationError::Changed, QStringLiteral("A recovery tree entry changed during verification."));
    return {};
  }

  RecoveryManifest take() {
    std::sort(m_manifest.entries.begin(), m_manifest.entries.end(),
              [](const auto &left, const auto &right) { return left.relativePath < right.relativePath; });
    return std::move(m_manifest);
  }

private:
  MutationResult list(int descriptor, QVector<QByteArray> &names, bool discover) {
    const int copy = ::openat(descriptor, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (copy < 0) return failure(errorForErrno(errno), QStringLiteral("Recovery directory cannot be enumerated."));
    DIR *stream = ::fdopendir(copy);
    if (!stream) {
      const int error = errno;
      ::close(copy);
      return failure(errorForErrno(error), QStringLiteral("Recovery directory enumeration failed."));
    }
    MutationResult result;
    errno = 0;
    while (const auto *child = ::readdir(stream)) {
      const QByteArray name(child->d_name);
      if (name == "." || name == "..") continue;
      if (cancelled(m_cancel)) { result = failure(MutationError::Cancelled, QStringLiteral("Verification cancelled.")); break; }
      if (names.size() >= 20'000 || (discover && ++m_discovered > 20'000)) {
        result = failure(MutationError::Unsupported, QStringLiteral("The tree exceeds recovery entry bounds."));
        break;
      }
      names.append(name);
      errno = 0;
    }
    const int error = errno;
    ::closedir(stream);
    if (!result.ok()) return result;
    if (error != 0) return failure(errorForErrno(error), QStringLiteral("Recovery enumeration was incomplete."));
    std::sort(names.begin(), names.end());
    if (std::adjacent_find(names.begin(), names.end()) != names.end())
      return failure(MutationError::Changed, QStringLiteral("Recovery enumeration repeated a changed entry."));
    return {};
  }

  MutationResult hashFile(int descriptor, off_t size, QByteArray &digest) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 64 * 1024> buffer {};
    off_t remaining = size;
    while (remaining > 0) {
      if (cancelled(m_cancel)) return failure(MutationError::Cancelled, QStringLiteral("Verification cancelled."));
      const auto wanted = static_cast<size_t>(std::min<off_t>(remaining, static_cast<off_t>(buffer.size())));
      const auto count = ::read(descriptor, buffer.data(), wanted);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) return failure(MutationError::Changed, QStringLiteral("File changed or could not be read completely."));
      hash.addData(QByteArrayView(buffer.data(), static_cast<qsizetype>(count)));
      remaining -= count;
      report(m_progress, static_cast<int>(m_manifest.entries.size()), QStringLiteral("Verifying file contents"));
    }
    char extra = 0;
    ssize_t count;
    do { count = ::read(descriptor, &extra, 1); } while (count < 0 && errno == EINTR);
    if (count != 0) return failure(MutationError::Changed, QStringLiteral("File length changed during verification."));
    digest = hash.result();
    return {};
  }

  quint64 m_mount;
  quint64 m_device;
  const MutationCancellation &m_cancel;
  const MutationProgressCallback &m_progress;
  int m_discovered = 1;
  qsizetype m_budget = 0;
  RecoveryManifest m_manifest;
};
} // namespace

RecoveryManifestRead captureRecoveryManifest(
    const RecoveryDirectoryAdmission &parent, const QByteArray &leaf,
    const MutationCancellation &cancellation, const MutationProgressCallback &progress) {
  if (leaf.isEmpty() || leaf.contains('/') || leaf.contains('\0') ||
      leaf == "." || leaf == ".." || leaf.size() > 255)
    return {failure(MutationError::InvalidRequest, QStringLiteral("Recovery leaf name is invalid.")), {}};
  auto current = parent.current();
  if (!current.ok()) return {current, {}};
  Capture capture(parent.observation().mountId, parent.observation().device, cancellation, progress);
  auto result = capture.entry(parent.descriptor(), leaf, {}, 0);
  if (!result.ok()) return {result, {}};
  current = parent.current();
  if (!current.ok()) return {current, {}};
  auto manifest = capture.take();
  if (recoveryManifestDigest(manifest, true).isEmpty())
    return {failure(MutationError::InvalidRequest, QStringLiteral("Recovery manifest validation failed.")), {}};
  return {{}, std::move(manifest)};
}
} // namespace QindaQt::Apps::FileManager
